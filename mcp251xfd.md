# Microchip MCP251XFD CAN FD Controller Driver

## Overview

The Zephyr `CONFIG_CAN_MCP251XFD` driver supports the Microchip MCP251XFD family of
external SPI-to-CAN FD controller ICs. These devices connect to the host MCU over SPI
and provide a full CAN 2.0 and CAN FD interface with up to 8 Mbps data-phase bitrate.

**Supported chips:**

| Chip | Description |
|------|-------------|
| **MCP2517FD** | Stand-alone external CAN FD controller |
| **MCP2518FD** | Stand-alone external CAN FD controller (successor to MCP2517FD) |
| **MCP251863** | MCP2518FD + integrated ATA6563 CAN FD transceiver in a single package |

All three variants use the same compatible string in Devicetree:

```
compatible = "microchip,mcp251xfd";
```

The driver source is located at:

- `drivers/can/can_mcp251xfd.c` -- driver implementation
- `drivers/can/can_mcp251xfd.h` -- register definitions, data structures, RAM layout macros

Devicetree binding: `dts/bindings/can/microchip,mcp251xfd.yaml`

---

## MCP251863 Compatibility

The **MCP251863** is a single-package solution that combines the **MCP2518FD** CAN FD
controller die with an integrated **ATA6563** high-speed CAN FD transceiver. From a
software and register perspective, the MCP251863 is **identical** to the MCP2518FD --
the same compatible string `"microchip,mcp251xfd"` is used and no special Kconfig
options or driver changes are required.

The **only practical difference** is that the CAN FD transceiver is built-in, so:

- You do **not** need an external transceiver IC on the board.
- The `can-transceiver` child node's `max-bitrate` property should be set according
  to the capabilities of the integrated ATA6563 transceiver and any board-level
  constraints (consult the transceiver datasheet and your hardware design limits).
- The `phys` property referencing an actively-controlled external transceiver is
  typically not needed.

**Shield overlays for MCP251863** are available in the repository:

- `boards/shields/mikroe_mcp251xfd_click/mikroe_mcp251863_click.overlay`

Example (MCP251863 on a MikroElektronika Click board):

```dts
&mikrobus_spi {
    cs-gpios = <&mikrobus_header 2 GPIO_ACTIVE_LOW>;

    mcp251863_mikroe_mcp251863_click: mcp251863@0 {
        compatible = "microchip,mcp251xfd";
        status = "okay";

        spi-max-frequency = <18000000>;
        int-gpios = <&mikrobus_header 7 GPIO_ACTIVE_LOW>;
        reg = <0x0>;
        osc-freq = <40000000>;
    };
};

/ {
    chosen {
        zephyr,canbus = &mcp251863_mikroe_mcp251863_click;
    };
};
```

See also [Using with Shields](#using-with-shields) for build commands.

---

## prj.conf -- All Kconfig Options

### Required Top-Level Options

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_CAN` | bool | n | **Required.** Enables the CAN driver subsystem. Must be set to `y`. |
| `CONFIG_CAN_MCP251XFD` | bool | y | Enables the MCP251XFD driver. Defaults to `y` when a `microchip,mcp251xfd` node with `status = "okay"` exists in the Devicetree, but can be disabled manually. Automatically selects `CONFIG_CRC` and `CONFIG_SPI`. |

### MCP251XFD-Specific Options

These options are available under `CONFIG_CAN_MCP251XFD`:

| Symbol | Type | Default | Range | Description |
|--------|------|---------|-------|-------------|
| `CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE` | int | 8 | 1--32 | Number of messages in the TX queue. Also defines the array size of transmit callback pointers, semaphores, and the TEF FIFO depth. |
| `CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS` | int | 16 | 1--32 | Number of CAN messages in the RX FIFO. Directly affects RAM usage on the MCP251XFD chip. |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE` | int | 768 | -- | Stack size (bytes) for the internal interrupt-handler thread. |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO` | int | 2 | -- | Priority of the interrupt-handler thread. A higher number = higher priority. The thread is cooperative (will not be preempted until it yields). |
| `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES` | int | 5 | -- | Number of retries for SFR register reads if the CRC check fails. |
| `CONFIG_CAN_MAX_FILTER` | int | 5 | 1--32 | Maximum number of concurrent active RX filters supported by `can_add_rx_filter()`. Note: This is defined under the `CAN_MCP251XFD` Kconfig scope but uses a generic name. |

### General CAN Subsystem Options

These options from `drivers/can/Kconfig` also apply to the MCP251XFD driver:

| Symbol | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_CAN_FD_MODE` | bool | n | Enable CAN FD support. Required for CAN FD data-phase bitrate switching. |
| `CONFIG_CAN_RX_TIMESTAMP` | bool | n | Enable receive timestamps. The MCP251XFD timestamp counter is derived from the internal clock divided by `timestamp-prescaler`. When enabled, each RX FIFO item grows by 4 bytes. |
| `CONFIG_CAN_INIT_PRIORITY` | int | 80 | CAN driver device initialization priority. |
| `CONFIG_CAN_DEFAULT_BITRATE` | int | 125000 | Default initial CAN bitrate in bits/s. Can be overridden per controller with the `bitrate` Devicetree property. |
| `CONFIG_CAN_DEFAULT_BITRATE_DATA` | int | 1000000 | Default initial CAN FD data-phase bitrate in bits/s. Only available when `CONFIG_CAN_FD_MODE=y`. Can be overridden with the `bitrate-data` Devicetree property. |
| `CONFIG_CAN_SAMPLE_POINT_MARGIN` | int | 50 | Maximum acceptable sample point deviation in permille. A value of 50 means +/- 5.0% margin. Range: 0--1000. |
| `CONFIG_CAN_ACCEPT_RTR` | bool | n | Accept incoming Remote Transmission Request (RTR) frames matching RX filters. If disabled, all RTR frames are rejected at driver level. |
| `CONFIG_CAN_MANUAL_RECOVERY_MODE` | bool | n | Enable manual (non-automatic) recovery from bus-off state. |
| `CONFIG_CAN_STATS` | bool | n | Enable CAN controller device statistics. Requires `CONFIG_STATS`. |

### Minimal prj.conf Example

```ini
# Minimal CAN configuration for MCP251XFD
CONFIG_CAN=y
# CONFIG_CAN_MCP251XFD is auto-selected from devicetree
```

### Full-Featured prj.conf Example

```ini
# Full-featured CAN FD configuration for MCP251XFD
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
CONFIG_CAN_RX_TIMESTAMP=y
CONFIG_CAN_ACCEPT_RTR=y
CONFIG_CAN_STATS=y
CONFIG_STATS=y

# MCP251XFD tuning
CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE=16
CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS=32
CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE=1024
CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO=2
CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES=5
CONFIG_CAN_MAX_FILTER=16

# Bitrate defaults (can also be set in devicetree)
CONFIG_CAN_DEFAULT_BITRATE=500000
CONFIG_CAN_DEFAULT_BITRATE_DATA=2000000
CONFIG_CAN_INIT_PRIORITY=80
```

---

## app.overlay -- All Devicetree Properties

### MCP251XFD-Specific Properties

Source: `dts/bindings/can/microchip,mcp251xfd.yaml`

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `osc-freq` | int | **yes** | -- | Frequency of the external oscillator in Hz (e.g., `40000000` for 40 MHz). |
| `int-gpios` | phandle-array | **yes** | -- | GPIO connected to the MCP251XFD INT pin. The interrupt signal is active-low, push-pull. Flags must properly describe the signal presented to the driver (typically `GPIO_ACTIVE_LOW`). |
| `pll-enable` | boolean | no | false | Enable the on-chip PLL, which multiplies the input oscillator frequency by 10. When enabled, the clock source is the PLL output; otherwise it is the oscillator directly. |
| `timestamp-prescaler` | int | no | 1 | Prescaler for the timestamp counter. The counter is derived from the internal clock divided by this value. Valid range: 1--1024. |
| `sof-on-clko` | boolean | no | false | Output start-of-frame (SOF) signal on the CLKO pin every time a start bit of a CAN message is transmitted or received. If not set, an internal clock (typically 40 MHz or 20 MHz) is output on the CLKO pin instead. |
| `xstby-enable` | boolean | no | false | Enable standby pin control on GPIO0. Used for transceivers that support a standby control input. |
| `clko-div` | int | no | 10 | Factor to divide the system clock for the CLKO pin. Allowed values: `1`, `2`, `4`, `10`. |

### CAN Controller Common Properties

Source: `dts/bindings/can/can-controller.yaml` (inherited via `can-fd-controller.yaml`)

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `bitrate` | int | no | `CONFIG_CAN_DEFAULT_BITRATE` | Initial CAN arbitration-phase bitrate in bit/s. |
| `sample-point` | int | no | auto | Initial sample point in permille (e.g., `875` = 87.5%). Auto-selected based on bitrate if unset: 75.0% for >800 kbit/s, 80.0% for >500 kbit/s, 87.5% otherwise. |
| `phys` | phandle | no | -- | Reference to an actively controlled CAN transceiver node (e.g., `can-transceiver-gpio`). |
| `bus-speed` | int | no | -- | **Deprecated.** Renamed to `bitrate`. |

### CAN FD Controller Properties

Source: `dts/bindings/can/can-fd-controller.yaml`

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `bitrate-data` | int | no | `CONFIG_CAN_DEFAULT_BITRATE_DATA` | Initial CAN FD data-phase bitrate in bit/s. |
| `sample-point-data` | int | no | auto | Initial data-phase sample point in permille. Auto-selected based on `bitrate-data` if unset (same rules as `sample-point`). |
| `bus-speed-data` | int | no | -- | **Deprecated.** Renamed to `bitrate-data`. |

### CAN Transceiver Child Node Properties

Source: `dts/bindings/can/can-controller.yaml` (child-binding)

A passive CAN transceiver is described as a **child node** named `can-transceiver`:

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `max-bitrate` | int | **yes** | -- | Maximum bitrate supported by the CAN transceiver in bits/s. |
| `min-bitrate` | int | no | -- | Minimum bitrate supported by the CAN transceiver in bits/s. |

Example:

```dts
mcp251xfd@0 {
    compatible = "microchip,mcp251xfd";
    /* ... */

    can-transceiver {
        max-bitrate = <1000000>;
    };
};
```

### SPI Device Properties

Source: `dts/bindings/spi/spi-device.yaml` (inherited via `include: [spi-device.yaml, ...]`)

The MCP251XFD node lives on an SPI bus and inherits all standard SPI device properties:

| Property | Type | Required | Default | Description |
|----------|------|----------|---------|-------------|
| `reg` | int | **yes** | -- | Chip-select index on the SPI bus (typically `<0x0>`). |
| `spi-max-frequency` | int | **yes** | -- | Maximum SPI clock frequency in Hz. This must not exceed the limit specified in the MCP251XFD datasheet for your specific device and board design. The Zephyr driver and bindings do not enforce a particular maximum; in-tree examples commonly use values in the 18--20 MHz range. |
| `duplex` | int | no | 0 (full) | SPI duplex mode. `0` = `SPI_FULL_DUPLEX`, `2048` = `SPI_HALF_DUPLEX`. The MCP251XFD requires **full duplex**. |
| `frame-format` | int | no | 0 (Motorola) | SPI frame format. `0` = `SPI_FRAME_FORMAT_MOTOROLA`, `32768` = `SPI_FRAME_FORMAT_TI`. The MCP251XFD uses **Motorola** format. |
| `spi-cpol` | boolean | no | false | SPI clock polarity (idle state). If set, clock idles high (CPOL=1). The MCP251XFD operates in **SPI mode 0,0** by default (CPOL=0, CPHA=0). Do **not** set this unless your hardware requires it. |
| `spi-cpha` | boolean | no | false | SPI clock phase. If set, data is sampled on the second edge (CPHA=1). The MCP251XFD operates in **SPI mode 0,0** by default. Do **not** set this unless your hardware requires it. |
| `spi-lsb-first` | boolean | no | false | If set, data is transmitted LSB first. The MCP251XFD uses **MSB-first** (default). Do **not** set this. |
| `spi-hold-cs` | boolean | no | false | Hold chip-select active across multiple SPI transactions. Not typically needed for MCP251XFD. |
| `spi-cs-high` | boolean | no | false | Chip-select is active-high. The MCP251XFD uses **active-low** CS. Do **not** set this. |
| `spi-interframe-delay-ns` | int | no | 0 | Delay in nanoseconds between SPI words. Default of 0 uses half of the SCK period. |
| `spi-cs-setup-delay-ns` | int | no | -- | Delay in nanoseconds after CS is asserted before the first clock edge (enable lead time). |
| `spi-cs-hold-delay-ns` | int | no | -- | Delay in nanoseconds before CS is de-asserted after the last clock edge (enable lag time). |

### Standard Properties

| Property | Type | Required | Description |
|----------|------|----------|-------------|
| `compatible` | string | **yes** | Must be `"microchip,mcp251xfd"` for all MCP2517FD, MCP2518FD, and MCP251863 variants. |
| `status` | string | no | Set to `"okay"` to enable the device. |

### Overlay Examples

#### Minimal Overlay

```dts
&spi1 {
    cs-gpios = <&gpio0 4 GPIO_ACTIVE_LOW>;

    mcp251xfd: can@0 {
        compatible = "microchip,mcp251xfd";
        reg = <0x0>;
        spi-max-frequency = <18000000>;
        int-gpios = <&gpio0 5 GPIO_ACTIVE_LOW>;
        osc-freq = <40000000>;
        status = "okay";
    };
};
```

#### Full-Featured Overlay (CAN FD with Transceiver)

```dts
&spi1 {
    cs-gpios = <&gpio0 4 GPIO_ACTIVE_LOW>;

    mcp251xfd: can@0 {
        compatible = "microchip,mcp251xfd";
        reg = <0x0>;
        spi-max-frequency = <20000000>;
        int-gpios = <&gpio0 5 GPIO_ACTIVE_LOW>;
        osc-freq = <40000000>;
        status = "okay";

        /* CAN FD bitrate settings */
        bitrate = <500000>;
        sample-point = <875>;
        bitrate-data = <2000000>;
        sample-point-data = <750>;

        /* MCP251XFD-specific options */
        pll-enable;
        timestamp-prescaler = <1>;
        sof-on-clko;
        clko-div = <10>;

        /* Passive transceiver */
        can-transceiver {
            max-bitrate = <8000000>;
        };
    };
};
```

#### Real-World Example: CANIS CANPico Shield

From `boards/shields/canis_canpico/canis_canpico.overlay`:

```dts
&pico_spi {
    status = "okay";
    cs-gpios = <&pico_header 6 GPIO_ACTIVE_LOW>;
    pinctrl-0 = <&spi1_canpico>;
    pinctrl-names = "default";

    mcp251xfd_canis_canpico: can@0 {
        compatible = "microchip,mcp251xfd";
        spi-max-frequency = <1000000>;
        int-gpios = <&pico_header 5 GPIO_ACTIVE_LOW>;
        status = "okay";
        reg = <0x0>;
        osc-freq = <16000000>;

        can-transceiver {
            max-bitrate = <1000000>;
        };
    };
};

/ {
    chosen {
        zephyr,canbus = &mcp251xfd_canis_canpico;
    };
};
```

#### Real-World Example: phyBOARD-Polis i.MX8M Mini

From `boards/phytec/phyboard_polis/phyboard_polis_mimx8mm6_m4.dts`:

```dts
&ecspi1 {
    status = "disabled";
    pinctrl-0 = <&ecspi1_default>;
    pinctrl-names = "default";
    cs-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>,
               <&gpio2 20 GPIO_ACTIVE_LOW>;

    /* CAN FD */
    mcp2518: mcp2518@0 {
        compatible = "microchip,mcp251xfd";
        reg = <0>;
        spi-max-frequency = <20000000>;
        int-gpios = <&gpio1 8 GPIO_ACTIVE_LOW>;
        supply-gpios = <&gpio1 9 GPIO_ACTIVE_LOW>;
        osc-freq = <40000000>;
        status = "disabled";
    };
};
```

#### MikroElektronika MCP2518FD Click Shield

From `boards/shields/mikroe_mcp251xfd_click/mikroe_mcp2518fd_click.overlay`:

```dts
&mikrobus_spi {
    cs-gpios = <&mikrobus_header 2 GPIO_ACTIVE_LOW>;

    mcp2518fd_mikroe_mcp2518fd_click: mcp2518fd@0 {
        compatible = "microchip,mcp251xfd";
        status = "okay";

        spi-max-frequency = <18000000>;
        int-gpios = <&mikrobus_header 7 GPIO_ACTIVE_LOW>;
        reg = <0x0>;
        osc-freq = <40000000>;
    };
};

/ {
    chosen {
        zephyr,canbus = &mcp2518fd_mikroe_mcp2518fd_click;
    };
};
```

---

## Internal Implementation Details

### Architecture

```
+---------------------+          SPI Bus          +-------------------+
|                     |  MOSI/MISO/SCK/CS/INT     |                   |
|    Host MCU         | <-----------------------> |   MCP251XFD       |
|                     |                            |                   |
|  +---------------+  |                            |  +-------------+  |
|  | Zephyr App    |  |                            |  | CAN FD Core |  |
|  +-------+-------+  |                            |  +------+------+  |
|          |           |                            |         |         |
|  +-------v-------+  |                            |  +------v------+  |
|  | CAN API       |  |                            |  | 2 KB RAM    |  |
|  | (can.h)       |  |                            |  | TEF|TXQ|RXF |  |
|  +-------+-------+  |                            |  +-------------+  |
|          |           |                            |         |         |
|  +-------v-------+  |                            |  +------v------+  |
|  | MCP251XFD     |  |   SPI Read/Write/CRC       |  | SPI Slave   |  |
|  | Driver        +--+--------------------------->|  | Interface   |  |
|  +-------+-------+  |                            |  +-------------+  |
|          |           |                            |                   |
|  +-------v-------+  |        GPIO (INT)          |  INT pin          |
|  | INT Thread    |<-+----------------------------+  (active-low)     |
|  | (cooperative) |  |                            |                   |
|  +---------------+  |                            +-------------------+
+---------------------+                            CAN H/L to Bus
```

### RAM Layout

The MCP251XFD has **2048 bytes** of on-chip RAM starting at address `0x400`. This RAM
is partitioned into three FIFO regions at compile time:

```
Address 0x400
+----------------------------------------------+
|  TEF FIFO (Transmit Event FIFO)              |  Offset 0x000
|  Items = CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE   |
|  Item size = 8 bytes                         |
+----------------------------------------------+
|  TX Queue                                    |
|  Items = CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE   |
|  Item size = 8 + PAYLOAD_SIZE bytes           |
+----------------------------------------------+
|  RX FIFO                                     |
|  Items = CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS  |
|  Item size = 8 + PAYLOAD_SIZE (+ 4 if TS)    |
+----------------------------------------------+
|  (Unused remainder)                          |
+----------------------------------------------+
Address 0xBFF (end of 2 KB)
```

**Key constants:**

| Constant | Value |
|----------|-------|
| `MCP251XFD_RAM_START_ADDR` | `0x400` |
| `MCP251XFD_RAM_SIZE` | `2048` bytes |
| `MCP251XFD_RAM_ALIGNMENT` | 4 bytes |
| `MCP251XFD_PAYLOAD_SIZE` | `CAN_MAX_DLEN` (8 bytes for Classic CAN, 64 bytes for CAN FD) |

**Size formulas:**

| FIFO | Item Size | Formula |
|------|-----------|---------|
| TEF FIFO | 8 bytes | `TEF_FIFO_SIZE = MAX_TX_QUEUE * 8` |
| TX Queue | `8 + PAYLOAD_SIZE` | `TX_QUEUE_SIZE = MAX_TX_QUEUE * (8 + PAYLOAD_SIZE)` |
| RX FIFO | `8 + PAYLOAD_SIZE` (+ 4 if `CONFIG_CAN_RX_TIMESTAMP`) | `RX_FIFO_SIZE = RX_FIFO_ITEMS * ITEM_SIZE` |

**Budget calculation -- Classic CAN (PAYLOAD_SIZE = 8):**

With defaults (`MAX_TX_QUEUE=8`, `RX_FIFO_ITEMS=16`, no timestamps):

| Region | Items | Item Size | Total |
|--------|-------|-----------|-------|
| TEF FIFO | 8 | 8 | 64 bytes |
| TX Queue | 8 | 16 | 128 bytes |
| RX FIFO | 16 | 16 | 256 bytes |
| **Total** | | | **448 bytes** of 2048 |

**Budget calculation -- CAN FD (PAYLOAD_SIZE = 64) with timestamps:**

With tuned settings (`MAX_TX_QUEUE=4`, `RX_FIFO_ITEMS=8`, timestamps enabled):

| Region | Items | Item Size | Total |
|--------|-------|-----------|-------|
| TEF FIFO | 4 | 8 | 32 bytes |
| TX Queue | 4 | 72 | 288 bytes |
| RX FIFO | 8 | 76 | 608 bytes |
| **Total** | | | **928 bytes** of 2048 |

> **Note:** A `BUILD_ASSERT` in `can_mcp251xfd.h` ensures the total FIFO allocation
> never exceeds 2048 bytes. If your configuration exceeds this, the build will fail.

### SPI Communication Protocol

The driver communicates with the MCP251XFD using the following primary SPI command types
(additional opcodes such as RESET and WRITE_CRC exist in the header but are used only
during initialization or are reserved):

| Command | Opcode Bits | Description |
|---------|-------------|-------------|
| **Read** (`mcp251xfd_read_reg`) | `0b0011` | Standard register read. 2-byte command header + data. |
| **Read with CRC** (`mcp251xfd_read_crc`) | `0b1011` | CRC-protected read. 2-byte command + 1-byte length + data + 2-byte CRC. Used for SFR reads to ensure data integrity. |
| **Write** (`mcp251xfd_write`) | `0b0010` | Register write. 2-byte command header + data. |

- **CRC polynomial:** `0x8005` (CRC-16/USB) -- a widely used 16-bit polynomial that provides HD=6 (Hamming distance 6) for data lengths up to 64 bits (ref: [Koopman CRC Catalog](https://users.ece.cmu.edu/~koopman/crc/index.html)).
- **CRC seed:** `0xFFFF`
- **SPI word size:** 8 bits (configured via `SPI_WORD_SET(8)` in the instantiation macro)
- **Read CRC retries:** Controlled by `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES` (default: 5)

### Interrupt Handling

The MCP251XFD uses a **level-triggered, active-low** interrupt output:

1. **GPIO callback** (`mcp251xfd_int_gpio_callback`): When the INT pin goes low, the
   GPIO callback fires. It immediately **disables** further pin interrupts and signals
   the interrupt semaphore (`int_sem`).

2. **Interrupt thread** (`mcp251xfd_int_thread`): A dedicated cooperative thread
   (priority: `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO`, stack:
   `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE`) waits on `int_sem`. When signaled,
   it calls `mcp251xfd_handle_interrupts()` which processes:
   - **RXIF** -- Receive FIFO (dispatches to RX filter callbacks) -- always enabled
   - **TEFIF** -- Transmit event FIFO (TX completion callbacks) -- always enabled
   - **MODIF** -- Mode change interrupts -- always enabled
   - **CERRIF** -- CAN error interrupts (bus-off, error-passive, error-warning) -- always enabled
   - **IVMIF** -- Invalid message interrupts (protocol errors, CRC errors) -- handled in the interrupt loop but **not** enabled via the INT enable register (`IVMIE` is not set)
   - **RXOVIF** -- RX overflow interrupts -- only enabled when `CONFIG_CAN_STATS` is set

3. After processing, the thread **re-enables** level-triggered GPIO interrupts.

> **Important:** The host MCU GPIO controller **must support level-triggered interrupts**
> (`GPIO_INT_LEVEL_ACTIVE`). Edge-triggered interrupts may miss events if the MCP251XFD
> asserts another interrupt before the first is acknowledged.

### Initialization Sequence

The `mcp251xfd_init()` function performs the following steps:

1. Enable external clock controller (if `clocks` property is present)
2. Initialize semaphores (`int_sem`, `tx_sem`) and mutex
3. Verify SPI bus and GPIO port readiness
4. Configure INT GPIO as input with level-triggered interrupt
5. Start the interrupt handler thread
6. **Reset** the MCP251XFD via SPI (`mcp251xfd_reset`)
7. Verify the chip entered Configuration mode after reset
8. Calculate bit timing from `bitrate`/`sample-point` (and `bitrate-data`/`sample-point-data` for CAN FD)
9. Initialize registers:
   - **CON** register (ISO CRC enable, TX bandwidth sharing, protocol exception disable)
   - **OSC** register (PLL enable/disable, CLKO divider, CLKO/SOF selection)
   - **IOCON** register (standby pin, GPIO configuration)
   - **INT** register (enable RXIE, MODIE, TEFIE, CERRIE; additionally RXOVIE when `CONFIG_CAN_STATS` is enabled)
   - **TDC** register (Transmitter Delay Compensation -- disabled initially, enabled when entering Mixed/CAN FD mode)
   - **TSCON** register (timestamp enable and prescaler, if `CONFIG_CAN_RX_TIMESTAMP`)
10. Initialize FIFOs:
    - TEF FIFO (depth = `MAX_TX_QUEUE`)
    - TX Queue (depth = `MAX_TX_QUEUE`, payload = `PAYLOAD_SIZE`)
    - RX FIFO (depth = `RX_FIFO_ITEMS`, payload = `PAYLOAD_SIZE`, timestamps optional)
11. Apply nominal bit timing (and data bit timing for CAN FD)

### Operating Modes Mapping

The driver maps Zephyr CAN modes to MCP251XFD hardware modes:

| Zephyr Mode | MCP251XFD Mode | Register Value |
|-------------|----------------|----------------|
| `CAN_MODE_NORMAL` | CAN 2.0 | `MODE_CAN2_0` (6) |
| `CAN_MODE_FD` | Mixed (CAN FD) | `MODE_MIXED` (0) |
| `CAN_MODE_LISTENONLY` | Listen Only | `MODE_LISTENONLY` (3) |
| `CAN_MODE_LOOPBACK` | External Loopback | `MODE_EXT_LOOPBACK` (5) |
| `CAN_MODE_3_SAMPLES` | -- | Not supported (`-ENOTSUP`) |
| `CAN_MODE_ONE_SHOT` | -- | Not supported (`-ENOTSUP`) |

> When switching from Configuration mode to Mixed (CAN FD) mode, Transmitter Delay
> Compensation (TDC) is automatically enabled in auto mode. When switching to CAN 2.0
> or loopback modes, TDC is disabled.

### Data Structures

#### `mcp251xfd_config` (per-device constant configuration)

```c
struct mcp251xfd_config {
    const struct can_driver_config common;  /* bitrate, sample-point, phys, etc. */

    struct spi_dt_spec bus;                 /* SPI bus from DT (spi-max-frequency, reg, etc.) */
    struct gpio_dt_spec int_gpio_dt;        /* INT GPIO from DT (int-gpios) */

    uint32_t osc_freq;                      /* osc-freq DT property */

    bool sof_on_clko;                       /* sof-on-clko DT property */
    bool xstby_enable;                      /* xstby-enable DT property */
    bool pll_enable;                        /* pll-enable DT property */
    uint8_t clko_div;                       /* clko-div DT property (enum index) */

    uint16_t timestamp_prescaler;           /* timestamp-prescaler DT property */

    const struct device *clk_dev;           /* clock controller (from clocks DT property) */
    uint8_t clk_id;                         /* clock ID */

    struct mcp251xfd_fifo rx_fifo;          /* RX FIFO configuration */
    struct mcp251xfd_fifo tef_fifo;         /* TEF FIFO configuration */
};
```

#### `mcp251xfd_data` (per-device mutable runtime state)

```c
struct mcp251xfd_data {
    struct can_driver_data common;          /* common CAN driver state (started, mode) */

    /* Interrupt Data */
    struct gpio_callback int_gpio_cb;       /* GPIO interrupt callback */
    struct k_thread int_thread;             /* interrupt handler thread */
    k_thread_stack_t *int_thread_stack;     /* interrupt thread stack */
    struct k_sem int_sem;                   /* semaphore signaled by GPIO ISR */

    /* General */
    enum can_state state;                   /* current CAN bus state */
    struct k_mutex mutex;                   /* protects SPI transactions */

    /* TX Callback */
    struct k_sem tx_sem;                    /* TX queue availability semaphore */
    uint32_t mailbox_usage;                 /* bitmask of used TX mailboxes */
    struct mcp251xfd_mailbox mailbox[CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE]; /* TX callbacks */

    /* Filter Data */
    uint32_t filter_usage;                  /* bitmask of used RX filters */
    struct can_filter filter[CONFIG_CAN_MAX_FILTER];                     /* active filters */
    can_rx_callback_t rx_cb[CONFIG_CAN_MAX_FILTER];                      /* RX callbacks */
    void *cb_arg[CONFIG_CAN_MAX_FILTER];                                 /* callback args */

    const struct device *dev;               /* back-reference to device */

    uint8_t next_mcp251xfd_mode;            /* mode to enter on start() */
    uint8_t current_mcp251xfd_mode;         /* current hardware mode */
    int tdco;                               /* Transmitter Delay Compensation Offset */

    struct mcp251xfd_spi_data spi_data;     /* SPI transfer buffer */
};
```

### Device Instantiation Macro

The `MCP251XFD_INIT` macro (at the bottom of `can_mcp251xfd.c`) maps Devicetree
properties to the config struct for each MCP251XFD instance:

```c
#define MCP251XFD_INIT(inst)
    /* Allocate interrupt thread stack */
    static K_KERNEL_STACK_DEFINE(mcp251xfd_int_stack_##inst,
                     CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE);

    static struct mcp251xfd_data mcp251xfd_data_##inst = {
        .int_thread_stack = mcp251xfd_int_stack_##inst,
    };

    static const struct mcp251xfd_config mcp251xfd_config_##inst = {
        .common = CAN_DT_DRIVER_CONFIG_INST_GET(inst, 0, 8000000),
        //        ^^^ min_bitrate=0, max_bitrate=8000000
        .bus = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8)),
        //     ^^^ SPI config from DT (reg, spi-max-frequency, etc.)
        .int_gpio_dt = GPIO_DT_SPEC_INST_GET(inst, int_gpios),

        .sof_on_clko = DT_INST_PROP(inst, sof_on_clko),
        .xstby_enable = DT_INST_PROP(inst, xstby_enable),
        .clko_div = DT_INST_ENUM_IDX(inst, clko_div),
        //          ^^^ Note: enum INDEX, not the raw value
        .pll_enable = DT_INST_PROP(inst, pll_enable),
        .timestamp_prescaler = DT_INST_PROP(inst, timestamp_prescaler),

        .osc_freq = DT_INST_PROP(inst, osc_freq),

        .rx_fifo = { /* configured from Kconfig constants */ },
        .tef_fifo = { /* configured from Kconfig constants */ },

        MCP251XFD_SET_CLOCK(inst)
        //  ^^^ optional: .clk_dev and .clk_id from "clocks" DT property
    };

    CAN_DEVICE_DT_INST_DEFINE(inst, mcp251xfd_init, NULL,
                  &mcp251xfd_data_##inst, &mcp251xfd_config_##inst,
                  POST_KERNEL, CONFIG_CAN_INIT_PRIORITY,
                  &mcp251xfd_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(MCP251XFD_INIT)
```

**Key mapping details:**

| DT Property / Kconfig | Config Struct Field | Notes |
|------------------------|---------------------|-------|
| `osc-freq` | `.osc_freq` | Direct mapping |
| `int-gpios` | `.int_gpio_dt` | Via `GPIO_DT_SPEC_INST_GET` |
| `sof-on-clko` | `.sof_on_clko` | Boolean DT property |
| `xstby-enable` | `.xstby_enable` | Boolean DT property |
| `pll-enable` | `.pll_enable` | Boolean DT property |
| `clko-div` | `.clko_div` | **Mapped as enum index** (0=1, 1=2, 2=4, 3=10) via `DT_INST_ENUM_IDX` |
| `timestamp-prescaler` | `.timestamp_prescaler` | Direct mapping |
| `reg`, `spi-max-frequency` | `.bus` | Via `SPI_DT_SPEC_INST_GET` |
| `bitrate`, `sample-point`, etc. | `.common` | Via `CAN_DT_DRIVER_CONFIG_INST_GET` |
| `clocks` | `.clk_dev`, `.clk_id` | Optional, via `MCP251XFD_SET_CLOCK` |

### API Functions

The driver implements the full Zephyr CAN API:

| API Function | Driver Implementation | Description |
|--------------|----------------------|-------------|
| `can_get_capabilities()` | `mcp251xfd_get_capabilities` | Returns `NORMAL`, `LISTENONLY`, `LOOPBACK` (+ `FD` if enabled) |
| `can_set_mode()` | `mcp251xfd_set_mode` | Set operating mode (must be called before `start`) |
| `can_set_timing()` | `mcp251xfd_set_timing` | Set nominal bit timing parameters |
| `can_set_timing_data()` | `mcp251xfd_set_timing_data` | Set data-phase bit timing (CAN FD only) |
| `can_start()` | `mcp251xfd_start` | Transition from config mode to operational mode |
| `can_stop()` | `mcp251xfd_stop` | Abort all TX, return to config mode |
| `can_send()` | `mcp251xfd_send` | Queue a CAN frame for transmission |
| `can_add_rx_filter()` | `mcp251xfd_add_rx_filter` | Add an RX acceptance filter (up to `CONFIG_CAN_MAX_FILTER`) |
| `can_remove_rx_filter()` | `mcp251xfd_remove_rx_filter` | Remove an RX filter |
| `can_get_state()` | `mcp251xfd_get_state` | Get current CAN bus error state and counters |
| `can_set_state_change_callback()` | `mcp251xfd_set_state_change_callback` | Register a state-change callback |
| `can_get_core_clock()` | `mcp251xfd_get_core_clock` | Returns the CAN core clock derived from the configured oscillator frequency (`osc_freq`) |
| `can_get_max_filters()` | `mcp251xfd_get_max_filters` | Returns `CONFIG_CAN_MAX_FILTER` |

**Bit timing limits:**

| Parameter | Nominal Min | Nominal Max | Data Min | Data Max |
|-----------|-------------|-------------|----------|----------|
| SJW | 1 | 128 | 1 | 16 |
| prop_seg | 0 | 0 | 0 | 0 |
| phase_seg1 | 2 | 256 | 1 | 32 |
| phase_seg2 | 1 | 128 | 1 | 16 |
| prescaler | 1 | 256 | 1 | 256 |

---

## Hardware Requirements

### SPI Interface

- **SPI mode:** Mode 0,0 (CPOL=0, CPHA=0) -- this is the default
- **Maximum SPI clock:**
  - MCP2517FD: **18 MHz**
  - MCP2518FD / MCP251863: **20 MHz**
- **Word size:** 8 bits
- **Duplex:** Full duplex required
- **Chip select:** Active-low

### Interrupt Pin

- **Type:** Active-low, push-pull output from MCP251XFD
- **Requirement:** The host MCU GPIO controller **must support level-triggered interrupts**
  (`GPIO_INT_LEVEL_ACTIVE`). The driver configures the GPIO as level-triggered.
  Edge-triggered interrupts are **not** supported and will cause missed interrupts.

### Oscillator / Crystal

- Common frequencies: **20 MHz** or **40 MHz** external crystal/oscillator
- Specified via the `osc-freq` Devicetree property
- When `pll-enable` is set, the internal PLL multiplies the oscillator frequency by 10
  (e.g., 4 MHz oscillator x 10 = 40 MHz CAN core clock)
- PLL readiness timeout: 100 ms (100 retries)

### Power Supply

- Operating voltage: 2.7V to 5.5V (see datasheet)
- The phyBOARD-Polis example shows a `supply-gpios` property for power control,
  though this is a board-specific addition and not part of the standard binding

---

## Using with Shields

Zephyr provides pre-built shield definitions for MCP251XFD-based Click boards:

### Available Shields

| Shield Name | Chip | Transceiver |
|-------------|------|-------------|
| `mikroe_mcp2517fd_click` | MCP2517FD | External ATA6563 |
| `mikroe_mcp2518fd_click` | MCP2518FD | External ATA6563 |
| `mikroe_mcp251863_click` | MCP251863 | Integrated ATA6563 |
| `canis_canpico` | MCP251XFD | On-board transceiver |

### Build Commands

```bash
# MCP2517FD Click shield
west build -b <your_board> samples/drivers/can/counter \
    --shield mikroe_mcp2517fd_click

# MCP2518FD Click shield
west build -b <your_board> samples/drivers/can/counter \
    --shield mikroe_mcp2518fd_click

# MCP251863 Click shield
west build -b <your_board> samples/drivers/can/counter \
    --shield mikroe_mcp251863_click

# CANIS CANPico shield (for Raspberry Pi Pico)
west build -b rpi_pico samples/drivers/can/counter \
    --shield canis_canpico
```

### Shield Requirements

- The target board must define the `mikrobus_spi` and `mikrobus_header` node labels
  for MikroElektronika Click shields (see
  [Zephyr Shields documentation](https://docs.zephyrproject.org/latest/hardware/porting/shields/index.html)).
- The target board GPIO controller must support **level-triggered interrupts**.
- The target board SPI controller must support clock frequencies up to **18 MHz**.
