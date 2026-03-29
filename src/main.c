/* SPDX-License-Identifier: BSD-3-Clause */

/**
 * @file main.c
 * @brief CAN bus send/receive sample application for Zephyr RTOS.
 *
 * This application demonstrates CAN communication using the MCP251863
 * CAN FD controller connected via SPI. It periodically transmits CAN
 * frames and receives frames matching a configured filter, with full
 * error handling and bus-off recovery.
 *
 * Hardware: Seeed XIAO nRF54L15 + Microchip MCP251863
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(can_sample, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/* Constants                                                          */
/* ------------------------------------------------------------------ */

/** CAN ID used for transmitted frames */
#define CAN_TX_MSG_ID            0x100

/** CAN ID to accept in the receive filter */
#define CAN_RX_FILTER_ID         0x200

/** Mask applied to the receive filter (exact match) */
#define CAN_RX_FILTER_MASK       0x7FF

/** Interval between consecutive transmissions [ms] */
#define CAN_TX_INTERVAL_MS       1000

/** Timeout waiting for a single CAN send to complete [ms] */
#define CAN_SEND_TIMEOUT_MS      100

/** Delay between initialisation retries [ms] */
#define CAN_INIT_RETRY_DELAY_MS  500

/** Maximum number of initialisation attempts */
#define CAN_INIT_MAX_RETRIES     10

/** Delay used during bus-off recovery sequence [ms] */
#define CAN_RECOVERY_DELAY_MS    1000

/** Maximum consecutive recovery failures before extending the delay */
#define CAN_RECOVERY_MAX_RETRIES 5

/** Extended delay after repeated recovery failures [ms] */
#define CAN_RECOVERY_BACKOFF_MS  5000

/** Number of data bytes in each transmitted frame (standard CAN maximum) */
#define CAN_TX_DATA_LEN          8

/** Number of successful TX frames between statistics log messages */
#define STATS_PRINT_INTERVAL     10

/** Log a warning after this many consecutive TX failures */
#define CAN_TX_ERR_BURST_THRESHOLD 5

/* ------------------------------------------------------------------ */
/* CAN device obtained from Devicetree                                */
/* ------------------------------------------------------------------ */

/* Compile-time check: ensure zephyr,canbus is defined in Devicetree */
#if !DT_HAS_CHOSEN(zephyr_canbus)
#error "Devicetree node 'zephyr,canbus' is not defined. " \
       "Check your board overlay or app.overlay."
#endif

/** CAN device selected via `chosen { zephyr,canbus }` in Devicetree */
static const struct device *const can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

/* ------------------------------------------------------------------ */
/* Synchronisation primitives                                         */
/* ------------------------------------------------------------------ */

/** Semaphore used to wait for TX completion inside the send helper */
static K_SEM_DEFINE(tx_sem, 0, 1);

/** Error status reported by the TX callback (written in callback context) */
static volatile int tx_callback_error;

/**
 * Generation counter for TX callback synchronisation.
 * Incremented before each can_send(); the callback checks whether its
 * generation still matches the current one and silently discards stale
 * completions (e.g. from a previously timed-out frame).
 */
static volatile uint32_t tx_generation;

/* ------------------------------------------------------------------ */
/* Statistics counters                                                 */
/* ------------------------------------------------------------------ */

/** Number of frames successfully transmitted */
static volatile uint32_t tx_count;

/** Number of frames received via the RX callback */
static volatile uint32_t rx_count;

/** Number of transmission errors */
static volatile uint32_t tx_error_count;

/** Tracks the current CAN controller error state */
static volatile enum can_state current_can_state = CAN_STATE_STOPPED;

/** Number of consecutive recovery failures (reset on success) */
static uint32_t recovery_fail_count;

/* ------------------------------------------------------------------ */
/* Forward declarations                                               */
/* ------------------------------------------------------------------ */

static void can_rx_callback(const struct device *dev,
                            struct can_frame *frame, void *user_data);
static void can_tx_callback(const struct device *dev, int error,
                            void *user_data);
static void can_state_change_callback(const struct device *dev,
                                      enum can_state state,
                                      struct can_bus_err_cnt err_cnt,
                                      void *user_data);

/* ------------------------------------------------------------------ */
/* Initialisation helpers                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the CAN device.
 *
 * Checks device readiness, registers the state-change callback, sets
 * the operating mode to normal, and starts the controller.  The start
 * step is retried up to @ref CAN_INIT_MAX_RETRIES times.
 *
 * @return 0 on success, negative errno on failure.
 */
static int can_device_init(void)
{
        int ret;

        /* Verify the CAN device is ready */
        if (!device_is_ready(can_dev)) {
                LOG_ERR("CAN device %s is not ready", can_dev->name);
                return -ENODEV;
        }
        LOG_INF("CAN device %s is ready", can_dev->name);

        /* Register state-change callback for error monitoring */
        can_set_state_change_callback(can_dev, can_state_change_callback, NULL);

        /* Set the CAN controller to normal operating mode */
        ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
        if (ret != 0) {
                LOG_ERR("Failed to set CAN mode (err %d)", ret);
                return ret;
        }

        /* Start the CAN controller with retry logic */
        for (int attempt = 0; attempt < CAN_INIT_MAX_RETRIES; attempt++) {
                ret = can_start(can_dev);
                if (ret == 0) {
                        LOG_INF("CAN controller started successfully");
                        return 0;
                }
                LOG_WRN("CAN start attempt %d/%d failed (err %d)",
                        attempt + 1, CAN_INIT_MAX_RETRIES, ret);
                k_msleep(CAN_INIT_RETRY_DELAY_MS);
        }

        LOG_ERR("CAN controller failed to start after %d attempts",
                CAN_INIT_MAX_RETRIES);
        return ret;
}

/**
 * @brief Register the receive filter for incoming CAN frames.
 *
 * Adds a filter that matches standard CAN ID @ref CAN_RX_FILTER_ID
 * with mask @ref CAN_RX_FILTER_MASK, and binds it to
 * @ref can_rx_callback.
 *
 * @return Filter ID (>= 0) on success, negative errno on failure.
 */
static int can_setup_rx_filter(void)
{
        struct can_filter filter = {
                .flags = 0,  /* Standard (11-bit) ID filter */
                .id    = CAN_RX_FILTER_ID,
                .mask  = CAN_RX_FILTER_MASK,
        };

        int filter_id = can_add_rx_filter(can_dev, can_rx_callback,
                                          NULL, &filter);
        if (filter_id < 0) {
                LOG_ERR("Failed to add RX filter (err %d)", filter_id);
                return filter_id;
        }

        LOG_INF("RX filter added: ID=0x%03X mask=0x%03X (filter_id=%d)",
                CAN_RX_FILTER_ID, CAN_RX_FILTER_MASK, filter_id);
        return filter_id;
}

/* ------------------------------------------------------------------ */
/* Callbacks                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Callback invoked when a CAN frame matching the RX filter
 *        is received.
 *
 * Runs in the MCP251XFD driver's interrupt-handler thread (not a
 * hardware ISR).  LOG_INF is safe here under all logging modes.
 *
 * @param dev       CAN device that received the frame.
 * @param frame     Pointer to the received CAN frame.
 * @param user_data User data (unused).
 */
static void can_rx_callback(const struct device *dev,
                            struct can_frame *frame, void *user_data)
{
        ARG_UNUSED(dev);
        ARG_UNUSED(user_data);

        if (frame == NULL) {
                LOG_ERR("RX callback: NULL frame pointer");
                return;
        }

        rx_count++;

        uint8_t len = can_dlc_to_bytes(frame->dlc);

        /* Clamp to standard CAN data length for safe array access */
        if (len > CAN_MAX_DLEN) {
                len = CAN_MAX_DLEN;
        }

        LOG_INF("RX: ID=0x%03X DLC=%u data=[%02X %02X %02X %02X %02X %02X %02X %02X]",
                frame->id, frame->dlc,
                (len > 0U) ? frame->data[0] : 0U,
                (len > 1U) ? frame->data[1] : 0U,
                (len > 2U) ? frame->data[2] : 0U,
                (len > 3U) ? frame->data[3] : 0U,
                (len > 4U) ? frame->data[4] : 0U,
                (len > 5U) ? frame->data[5] : 0U,
                (len > 6U) ? frame->data[6] : 0U,
                (len > 7U) ? frame->data[7] : 0U);
}

/**
 * @brief Callback invoked when a CAN transmission completes.
 *
 * Gives the TX semaphore so the blocking send helper can proceed.
 *
 * @param dev       CAN device that completed the send.
 * @param error     0 on success, negative errno on failure.
 * @param user_data TX generation counter cast to pointer.
 */
static void can_tx_callback(const struct device *dev, int error,
                            void *user_data)
{
        ARG_UNUSED(dev);

        uint32_t cb_gen = (uint32_t)(uintptr_t)user_data;

        /* Discard stale callbacks from a previously timed-out frame */
        if (cb_gen != tx_generation) {
                LOG_WRN("Stale TX callback (gen %u, expected %u) - discarded",
                        cb_gen, tx_generation);
                return;
        }

        tx_callback_error = error;

        if (error != 0) {
                LOG_ERR("TX callback error: %d", error);
        }

        k_sem_give(&tx_sem);
}

/**
 * @brief Callback invoked when the CAN controller changes error state.
 *
 * Logs the transition and updates @ref current_can_state.
 *
 * @param dev       CAN device whose state changed.
 * @param state     New CAN state.
 * @param err_cnt   Current TX/RX error counters.
 * @param user_data User data (unused).
 */
static void can_state_change_callback(const struct device *dev,
                                      enum can_state state,
                                      struct can_bus_err_cnt err_cnt,
                                      void *user_data)
{
        ARG_UNUSED(dev);
        ARG_UNUSED(user_data);

        static const char *const state_names[] = {
                [CAN_STATE_ERROR_ACTIVE]  = "Error Active",
                [CAN_STATE_ERROR_WARNING] = "Error Warning",
                [CAN_STATE_ERROR_PASSIVE] = "Error Passive",
                [CAN_STATE_BUS_OFF]       = "Bus Off",
                [CAN_STATE_STOPPED]       = "Stopped",
        };

        enum can_state prev = current_can_state;

        const char *name = (state < ARRAY_SIZE(state_names) &&
                            state_names[state] != NULL)
                           ? state_names[state]
                           : "Unknown";
        const char *prev_name = (prev < ARRAY_SIZE(state_names) &&
                                  state_names[prev] != NULL)
                                ? state_names[prev]
                                : "Unknown";

        LOG_WRN("CAN state: %s -> %s (TX err=%u, RX err=%u)",
                prev_name, name, err_cnt.tx_err_cnt, err_cnt.rx_err_cnt);

        current_can_state = state;
}

/* ------------------------------------------------------------------ */
/* TX helpers                                                         */
/* ------------------------------------------------------------------ */

/**
 * @brief Build a standard CAN TX frame.
 *
 * Constructs a CAN 2.0 frame with ID @ref CAN_TX_MSG_ID, DLC = 8,
 * and embeds a rolling counter in the first two data bytes.
 *
 * @param[out] frame   Pointer to the frame to populate.
 * @param      counter Rolling counter value to embed.
 */
static void can_build_tx_frame(struct can_frame *frame, uint16_t counter)
{
        __ASSERT_NO_MSG(frame != NULL);

        memset(frame, 0, sizeof(*frame));
        frame->id    = CAN_TX_MSG_ID;
        frame->dlc   = CAN_TX_DATA_LEN;
        frame->flags = 0;  /* Explicit: standard CAN 2.0 frame (redundant after memset, kept for clarity) */

        /* Bytes 0-1: rolling counter (big-endian) */
        frame->data[0] = (uint8_t)(counter >> 8);
        frame->data[1] = (uint8_t)(counter & 0xFF);
        /* Bytes 2-7: padding / reserved */
        frame->data[2] = 0xCA;
        frame->data[3] = 0xFE;
        frame->data[4] = 0xDE;
        frame->data[5] = 0xAD;
        frame->data[6] = 0xBE;
        frame->data[7] = 0xEF;
}

/**
 * @brief Send a CAN frame with a timeout.
 *
 * Calls can_send() with @ref can_tx_callback, then blocks on the TX
 * semaphore for up to @ref CAN_SEND_TIMEOUT_MS milliseconds.
 *
 * If the semaphore times out, the frame may still be pending in the
 * driver's TX queue.  A generation counter is used so that any late
 * callback from a timed-out frame is safely discarded.
 *
 * @note This function is NOT thread-safe. It must only be called from
 *       a single thread context (the main thread in this application).
 *
 * @param frame Pointer to the CAN frame to transmit.
 * @return 0 on success, -EINVAL if @p frame is NULL,
 *         -EAGAIN on timeout, or another negative errno on failure.
 */
static int can_send_frame_with_timeout(struct can_frame *frame)
{
        int ret;

        if (frame == NULL) {
                return -EINVAL;
        }

        /* Advance generation so any stale callback is discarded */
        tx_generation++;

        /* Reset semaphore and callback error before sending */
        k_sem_reset(&tx_sem);
        tx_callback_error = 0;

        ret = can_send(can_dev, frame, K_NO_WAIT,
                       can_tx_callback,
                       (void *)(uintptr_t)tx_generation);
        if (ret != 0) {
                LOG_ERR("can_send() failed (err %d)", ret);
                tx_error_count++;
                return ret;
        }

        /* Wait for the TX callback to signal completion */
        ret = k_sem_take(&tx_sem, K_MSEC(CAN_SEND_TIMEOUT_MS));
        if (ret != 0) {
                LOG_ERR("TX completion timed out");
                tx_error_count++;
                return ret;
        }

        /* Check whether the callback reported an error */
        if (tx_callback_error != 0) {
                tx_error_count++;
                return tx_callback_error;
        }

        tx_count++;
        return 0;
}

/* ------------------------------------------------------------------ */
/* Controller recovery                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief Attempt to recover the CAN controller.
 *
 * Called when the controller is in bus-off or stopped state.
 * Stops the controller (tolerating -EALREADY if already stopped),
 * waits, then restarts it.
 *
 * The wait duration uses exponential back-off: the first
 * @ref CAN_RECOVERY_MAX_RETRIES attempts wait @ref CAN_RECOVERY_DELAY_MS;
 * subsequent attempts wait @ref CAN_RECOVERY_BACKOFF_MS.  The failure
 * counter is reset to zero on a successful recovery.
 *
 * @return 0 on success, negative errno on failure.
 */
static int can_recover_controller(void)
{
        int ret;
        uint32_t delay;

        if (recovery_fail_count < UINT32_MAX) {
                recovery_fail_count++;
        }

        if (recovery_fail_count > CAN_RECOVERY_MAX_RETRIES) {
                delay = CAN_RECOVERY_BACKOFF_MS;
                LOG_WRN("Recovery attempt %u (backoff %u ms)",
                        recovery_fail_count, delay);
        } else {
                delay = CAN_RECOVERY_DELAY_MS;
                LOG_WRN("Attempting CAN controller recovery (%u/%u)...",
                        recovery_fail_count, CAN_RECOVERY_MAX_RETRIES);
        }

        ret = can_stop(can_dev);
        if (ret != 0 && ret != -EALREADY) {
                LOG_ERR("can_stop() failed during recovery (err %d)", ret);
                return ret;
        }

        k_msleep(delay);

        ret = can_start(can_dev);
        if (ret != 0) {
                LOG_ERR("can_start() failed during recovery (err %d)", ret);
                return ret;
        }

        LOG_INF("CAN controller recovery succeeded after %u attempt(s)",
                recovery_fail_count);
        recovery_fail_count = 0;
        return 0;
}

/* ------------------------------------------------------------------ */
/* Main                                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Application entry point.
 *
 * 1. Initialises the CAN device (retries handled internally).
 * 2. Registers the RX filter.
 * 3. Enters a 1-second periodic TX loop that:
 *    - Handles bus-off recovery when detected.
 *    - Builds and transmits a CAN frame.
 *    - Prints statistics every 10 frames.
 *
 * @return Does not return under normal operation.
 */
int main(void)
{
        int ret;
        uint16_t counter = 0;
        uint32_t last_stats_tx_count = 0;
        uint32_t consecutive_tx_errors = 0;
        struct can_frame tx_frame;

        LOG_INF("CAN sample application starting...");

        /* ----- Initialise CAN device (retries handled inside) ----- */
        ret = can_device_init();
        if (ret != 0) {
                LOG_ERR("CAN initialisation failed - halting");
                goto halt;
        }

        /* ----- Register RX filter ----- */
        ret = can_setup_rx_filter();
        if (ret < 0) {
                LOG_ERR("RX filter setup failed - halting");
                goto halt;
        }

        LOG_INF("Entering main TX loop (interval=%d ms)", CAN_TX_INTERVAL_MS);

        /* ----- Main TX loop ----- */
        while (1) {
                k_msleep(CAN_TX_INTERVAL_MS);

                /* Snapshot the current CAN state for this iteration */
                enum can_state state_snapshot = current_can_state;

                /* Handle bus-off or stopped condition */
                if (state_snapshot == CAN_STATE_BUS_OFF ||
                    state_snapshot == CAN_STATE_STOPPED) {
                        ret = can_recover_controller();
                        if (ret != 0) {
                                LOG_ERR("Controller recovery failed; retrying next cycle");
                                continue;
                        }
                }

                /* Build and send a CAN frame */
                can_build_tx_frame(&tx_frame, counter);

                ret = can_send_frame_with_timeout(&tx_frame);
                if (ret == 0) {
                        LOG_INF("TX: ID=0x%03X counter=%u", CAN_TX_MSG_ID, counter);
                        counter++;
                        consecutive_tx_errors = 0;
                } else {
                        consecutive_tx_errors++;
                        LOG_ERR("TX failed (err %d, consecutive=%u)",
                                ret, consecutive_tx_errors);
                        if (consecutive_tx_errors == CAN_TX_ERR_BURST_THRESHOLD) {
                                LOG_WRN("TX error burst: %u consecutive failures",
                                        consecutive_tx_errors);
                        }
                }

                /* Print statistics every STATS_PRINT_INTERVAL successful frames */
                uint32_t tx_snap = tx_count;

                if (tx_snap > 0U &&
                    (tx_snap % STATS_PRINT_INTERVAL) == 0U &&
                    tx_snap != last_stats_tx_count) {
                        last_stats_tx_count = tx_snap;
                        LOG_INF("Stats: TX=%u RX=%u TX_ERR=%u",
                                tx_snap, rx_count, tx_error_count);
                }
        }

halt:
        LOG_ERR("Fatal error (err %d) - system halted", ret);
        while (1) {
                k_msleep(1000);
        }
        return 0; /* unreachable */
}
