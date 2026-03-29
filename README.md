# study-ZephyrCAN

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/uist1idrju3i/study-ZephyrCAN)

> [Japanese / 日本語版はこちら](README.ja.md)

## Verified Hardware

The following hardware platforms have been tested with OpenBlink:

- [Seeed XIAO nRF54L15](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/) (Board target: xiao_nrf54l15/nrf54l15/cpuapp)
- Microchip [MCP251863](https://www.microchip.com/en-us/product/MCP251863) (CAN FD controller + transceiver)

## Development Environment Versions

- nRF Connect SDK toolchain v3.2.1
- nRF Connect SDK v3.2.1

---

## CAN Sample Application

This project provides a complete CAN bus send/receive sample running on Zephyr RTOS. The nRF54L15 SoC (which has no native CAN peripheral) communicates with an external MCP251863 CAN FD controller over SPI.

### System Architecture

```mermaid
block-beta
  columns 3

  block:MCU["Seeed XIAO nRF54L15"]:2
    columns 2
    APP["Zephyr Application\n(src/main.c)"]
    CAN_API["Zephyr CAN API\n(can_send / can_add_rx_filter)"]
    SPI_DRV["SPI Master Driver\n(SPIM0)"]
    MCP_DRV["MCP251XFD Driver\n(can_mcp251xfd)"]
  end

  block:EXT["MCP251863"]:1
    columns 1
    CTRL["CAN FD\nController"]
    XCVR["CAN\nTransceiver"]
  end

  SPI_DRV --> CTRL
  XCVR --> BUS["CAN Bus"]

  style MCU fill:#e3f2fd,stroke:#1565c0
  style EXT fill:#fff3e0,stroke:#e65100
```

### Pin Assignment

| Signal | GPIO | SPI Instance | Description |
|--------|------|-------------|-------------|
| SCK | P1.1 | SPI00 | SPI clock |
| MOSI | P1.2 | SPI00 | SPI data out |
| MISO | P1.3 | SPI00 | SPI data in |
| CS | P1.0 | SPI00 | Chip select (active low) |
| INT | P1.8 | - | MCP251863 interrupt (active low) |
| WS2812 | P1.4-P1.7 | SPI20-SPI30 | LED strip data (existing) |

> **Note:** Pin assignments are placeholders. Verify against your actual hardware wiring before flashing.

### Application Flow

```mermaid
flowchart TD
    A([main]) --> B[can_device_init]
    B --> B1{Device ready?}
    B1 -->|No| ERR1([Return error])
    B1 -->|Yes| B2[Register state-change callback]
    B2 --> B3[Set CAN_MODE_NORMAL]
    B3 --> B4[can_start with retry\nup to 10 attempts]
    B4 --> B5{Started?}
    B5 -->|No| ERR1
    B5 -->|Yes| C[can_setup_rx_filter\nID=0x200 mask=0x7FF]
    C --> C1{Filter added?}
    C1 -->|No| ERR1
    C1 -->|Yes| D[Main TX loop]

    D --> E[Sleep 1000 ms]
    E --> F{Bus-off?}
    F -->|Yes| G[can_recover_from_bus_off\nstop -> sleep -> restart]
    G --> G1{Recovered?}
    G1 -->|No| E
    G1 -->|Yes| H
    F -->|No| H[Build TX frame\nID=0x100 DLC=8]
    H --> I[can_send_frame_with_timeout]
    I --> I1[can_send with K_NO_WAIT]
    I1 --> I2[k_sem_take with 100 ms timeout]
    I2 --> I3{Success?}
    I3 -->|Yes| J[Log TX + increment counter]
    I3 -->|No| K[Log error + increment tx_error_count]
    J --> L{counter % 10 == 0?}
    K --> L
    L -->|Yes| M[Log statistics:\nTX / RX / TX_ERR]
    L -->|No| E
    M --> E
```

### CAN State Machine

The application monitors the CAN controller's error state via a callback and handles bus-off recovery.

```mermaid
stateDiagram-v2
    [*] --> ErrorActive : can_start()

    ErrorActive --> ErrorWarning : TEC or REC >= 96
    ErrorWarning --> ErrorActive : Counters decrease
    ErrorWarning --> ErrorPassive : TEC or REC >= 128
    ErrorPassive --> ErrorWarning : Counters decrease
    ErrorPassive --> BusOff : TEC >= 256

    BusOff --> Stopped : can_stop()
    Stopped --> ErrorActive : can_start()\n(recovery)

    note right of BusOff
        Recovery sequence:
        1. can_stop()
        2. k_msleep(1000)
        3. can_start()
    end note
```

### TX Completion Flow

Transmission uses a semaphore-based synchronization between the main thread and the ISR callback.

```mermaid
sequenceDiagram
    participant Main as Main Thread
    participant API as Zephyr CAN API
    participant ISR as TX Callback (ISR)

    Main->>Main: k_sem_reset(&tx_sem)
    Main->>API: can_send(frame, K_NO_WAIT, callback)
    API-->>Main: 0 (queued)
    Main->>Main: k_sem_take(&tx_sem, 100 ms)

    Note over API: Hardware transmits frame

    API->>ISR: can_tx_callback(error)
    ISR->>ISR: tx_callback_error = error
    ISR->>Main: k_sem_give(&tx_sem)

    Main->>Main: Check tx_callback_error
    alt error == 0
        Main->>Main: tx_count++
    else error != 0
        Main->>Main: tx_error_count++
    end
```

### File Structure

| File | Description |
|------|-------------|
| `src/main.c` | CAN application: init, TX/RX, callbacks, bus-off recovery |
| `app.overlay` | Devicetree overlay: SPI00 + MCP251863, WS2812 LEDs |
| `prj.conf` | Kconfig: CAN driver, BLE, logging, peripherals |
| `mcp251xfd.md` | MCP251XFD driver technical documentation |
| `mcp251xfd.ja.md` | Same documentation in Japanese |

### Configuration Constants

Defined in `src/main.c`:

| Constant | Value | Description |
|----------|-------|-------------|
| `CAN_TX_MSG_ID` | `0x100` | CAN ID for transmitted frames |
| `CAN_RX_FILTER_ID` | `0x200` | CAN ID accepted by the RX filter |
| `CAN_RX_FILTER_MASK` | `0x7FF` | Filter mask (exact match) |
| `CAN_TX_INTERVAL_MS` | `1000` | Transmission interval (ms) |
| `CAN_SEND_TIMEOUT_MS` | `100` | TX completion timeout (ms) |
| `CAN_INIT_MAX_RETRIES` | `10` | Max `can_start()` retry attempts |
| `CAN_INIT_RETRY_DELAY_MS` | `500` | Delay between init retries (ms) |
| `CAN_RECOVERY_DELAY_MS` | `1000` | Delay during bus-off recovery (ms) |

### Kconfig (CAN section in prj.conf)

| Symbol | Value | Description |
|--------|-------|-------------|
| `CONFIG_CAN` | `y` | Enable CAN subsystem |
| `CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE` | `8` | TX queue depth |
| `CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS` | `16` | RX FIFO depth |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE` | `1536` | Interrupt handler stack (default: 768) |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO` | `2` | Interrupt handler thread priority |
| `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES` | `5` | SPI read CRC retry count |
| `CONFIG_CAN_DEFAULT_BITRATE` | `500000` | Default CAN bitrate (500 kbps) |

### TX Frame Format

```
Byte:  [0]    [1]    [2]   [3]   [4]   [5]   [6]   [7]
Data:  CNT_H  CNT_L  0xCA  0xFE  0xDE  0xAD  0xBE  0xEF
       |___________|
        Rolling counter (big-endian uint16)
```

- **CAN ID:** `0x100` (standard 11-bit)
- **DLC:** 8
- **Bytes 0-1:** Rolling counter (increments on each successful TX)
- **Bytes 2-7:** Fixed pattern `0xCAFEDEADBEEF`

## Build

```bash
west build -b xiao_nrf54l15/nrf54l15/cpuapp
```

## Flash

```bash
west flash
```

> Requires physical hardware connected via USB or a debug probe.

## References

- [Zephyr CAN API](https://docs.zephyrproject.org/latest/hardware/peripherals/can/index.html)
- [MCP251XFD Driver Documentation](mcp251xfd.md) ([Japanese](mcp251xfd.ja.md))
- [Microchip MCP251863 Product Page](https://www.microchip.com/en-us/product/MCP251863)
