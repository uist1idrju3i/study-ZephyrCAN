# Microchip MCP251XFD CAN FD コントローラドライバ

## 概要

Zephyr の `CONFIG_CAN_MCP251XFD` ドライバは、Microchip MCP251XFD ファミリの外付け SPI 接続 CAN FD コントローラ IC をサポートします。これらのデバイスはホスト MCU と SPI で接続し、CAN 2.0 および CAN FD（データフェーズ最大 8 Mbps）のインターフェースを提供します。

**対応チップ:**

| チップ | 説明 |
|--------|------|
| **MCP2517FD** | スタンドアロン外付け CAN FD コントローラ |
| **MCP2518FD** | スタンドアロン外付け CAN FD コントローラ（MCP2517FD の後継） |
| **MCP251863** | MCP2518FD + ATA6563 CAN FD トランシーバ内蔵の単一パッケージ |

3 種類すべて、Devicetree では同じ compatible 文字列を使用します:

```
compatible = "microchip,mcp251xfd";
```

ドライバのソースコード:

- `drivers/can/can_mcp251xfd.c` -- ドライバ実装
- `drivers/can/can_mcp251xfd.h` -- レジスタ定義、データ構造体、RAM レイアウトマクロ

Devicetree バインディング: `dts/bindings/can/microchip,mcp251xfd.yaml`

---

## MCP251863 の互換性

**MCP251863** は **MCP2518FD** CAN FD コントローラダイと **ATA6563** 高速 CAN FD トランシーバを一つのパッケージに統合したソリューションです。ソフトウェアおよびレジスタの観点では、MCP251863 は MCP2518FD と**同一**であり、同じ compatible 文字列 `"microchip,mcp251xfd"` を使用します。特別な Kconfig オプションやドライバの変更は不要です。

**唯一の実質的な違い**は、CAN FD トランシーバが内蔵されている点です:

- 基板上に外付けトランシーバ IC は**不要**です。
- `can-transceiver` 子ノードの `max-bitrate` プロパティは、内蔵 ATA6563 トランシーバの能力およびボードレベルの制約に合わせて設定してください（トランシーバのデータシートとハードウェア設計上の制限を参照）。
- 外付けトランシーバを能動的に制御する `phys` プロパティは通常不要です。

**MCP251863 用シールドオーバーレイ**がリポジトリに用意されています:

- `boards/shields/mikroe_mcp251xfd_click/mikroe_mcp251863_click.overlay`

例（MikroElektronika Click ボード上の MCP251863）:

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

[シールドの使用方法](#シールドの使用方法) も参照してください。

---

## prj.conf -- 全 Kconfig オプション

### 必須トップレベルオプション

| シンボル | 型 | デフォルト | 説明 |
|----------|-----|-----------|------|
| `CONFIG_CAN` | bool | n | **必須。** CAN ドライバサブシステムを有効化します。`y` に設定してください。 |
| `CONFIG_CAN_MCP251XFD` | bool | y | MCP251XFD ドライバを有効化します。Devicetree に `status = "okay"` の `microchip,mcp251xfd` ノードが存在する場合、デフォルトで `y` になります。`CONFIG_CRC` と `CONFIG_SPI` を自動選択します。 |

### MCP251XFD 固有オプション

`CONFIG_CAN_MCP251XFD` 有効時に利用可能なオプション:

| シンボル | 型 | デフォルト | 範囲 | 説明 |
|----------|-----|-----------|------|------|
| `CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE` | int | 8 | 1--32 | TX キューのメッセージ数。送信コールバックポインタ、セマフォの配列サイズおよび TEF FIFO の深さも決定します。 |
| `CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS` | int | 16 | 1--32 | RX FIFO の CAN メッセージ数。MCP251XFD チップ上の RAM 使用量に直接影響します。 |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE` | int | 768 | -- | 割り込みハンドラスレッドのスタックサイズ（バイト）。 |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO` | int | 2 | -- | 割り込みハンドラスレッドの優先度。数値が大きいほど優先度が高くなります。スレッドは協調型（yield するまでプリエンプトされません）です。 |
| `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES` | int | 5 | -- | SFR レジスタ読み出し時に CRC チェックが失敗した場合のリトライ回数。 |
| `CONFIG_CAN_MAX_FILTER` | int | 5 | 1--32 | `can_add_rx_filter()` でサポートされる同時アクティブ RX フィルタの最大数。注: `CAN_MCP251XFD` Kconfig スコープ内で定義されていますが、汎用的な名前を使用しています。 |

### 汎用 CAN サブシステムオプション

`drivers/can/Kconfig` の以下のオプションも MCP251XFD ドライバに適用されます:

| シンボル | 型 | デフォルト | 説明 |
|----------|-----|-----------|------|
| `CONFIG_CAN_FD_MODE` | bool | n | CAN FD サポートを有効化します。CAN FD データフェーズのビットレート切り替えに必要です。 |
| `CONFIG_CAN_RX_TIMESTAMP` | bool | n | 受信タイムスタンプを有効化します。MCP251XFD のタイムスタンプカウンタは内部クロックを `timestamp-prescaler` で分周して生成されます。有効にすると各 RX FIFO アイテムが 4 バイト増加します。 |
| `CONFIG_CAN_INIT_PRIORITY` | int | 80 | CAN ドライバのデバイス初期化優先度。 |
| `CONFIG_CAN_DEFAULT_BITRATE` | int | 125000 | デフォルトの CAN ビットレート（bit/s）。Devicetree の `bitrate` プロパティでコントローラごとに上書き可能です。 |
| `CONFIG_CAN_DEFAULT_BITRATE_DATA` | int | 1000000 | デフォルトの CAN FD データフェーズビットレート（bit/s）。`CONFIG_CAN_FD_MODE=y` 時のみ利用可能。Devicetree の `bitrate-data` プロパティで上書き可能です。 |
| `CONFIG_CAN_SAMPLE_POINT_MARGIN` | int | 50 | サンプルポイントの最大許容偏差（パーミル）。50 は +/- 5.0% を意味します。範囲: 0--1000。 |
| `CONFIG_CAN_ACCEPT_RTR` | bool | n | RX フィルタに一致する受信 RTR（Remote Transmission Request）フレームを受け入れます。無効の場合、すべての RTR フレームはドライバレベルで拒否されます。 |
| `CONFIG_CAN_MANUAL_RECOVERY_MODE` | bool | n | バスオフ状態からの手動（非自動）復帰を有効化します。 |
| `CONFIG_CAN_STATS` | bool | n | CAN コントローラデバイス統計を有効化します。`CONFIG_STATS` が必要です。 |

### 最小限の prj.conf 例

```ini
# MCP251XFD の最小 CAN 設定
CONFIG_CAN=y
# CONFIG_CAN_MCP251XFD は Devicetree から自動選択されます
```

### フル機能 prj.conf 例

```ini
# MCP251XFD のフル機能 CAN FD 設定
CONFIG_CAN=y
CONFIG_CAN_FD_MODE=y
CONFIG_CAN_RX_TIMESTAMP=y
CONFIG_CAN_ACCEPT_RTR=y
CONFIG_CAN_STATS=y
CONFIG_STATS=y

# MCP251XFD チューニング
CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE=16
CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS=32
CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE=1024
CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO=2
CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES=5
CONFIG_CAN_MAX_FILTER=16

# ビットレートのデフォルト値（Devicetree でも設定可能）
CONFIG_CAN_DEFAULT_BITRATE=500000
CONFIG_CAN_DEFAULT_BITRATE_DATA=2000000
CONFIG_CAN_INIT_PRIORITY=80
```

---

## app.overlay -- 全 Devicetree プロパティ

### MCP251XFD 固有プロパティ

ソース: `dts/bindings/can/microchip,mcp251xfd.yaml`

| プロパティ | 型 | 必須 | デフォルト | 説明 |
|-----------|-----|------|-----------|------|
| `osc-freq` | int | **はい** | -- | 外部オシレータの周波数（Hz）。例: 40 MHz の場合 `40000000`。 |
| `int-gpios` | phandle-array | **はい** | -- | MCP251XFD の INT ピンに接続された GPIO。割り込み信号はアクティブロー、プッシュプルです。フラグはドライバに提示される信号を正しく記述する必要があります（通常 `GPIO_ACTIVE_LOW`）。 |
| `pll-enable` | boolean | いいえ | false | オンチップ PLL を有効化します。入力オシレータ周波数を 10 倍に逓倍します。有効時はクロックソースが PLL 出力になり、無効時はオシレータ直接出力になります。 |
| `timestamp-prescaler` | int | いいえ | 1 | タイムスタンプカウンタのプリスケーラ。内部クロックをこの値で分周します。有効範囲: 1--1024。 |
| `sof-on-clko` | boolean | いいえ | false | CLKO ピンに Start-of-Frame（SOF）信号を出力します。CAN メッセージの送受信時にスタートビットごとに SOF が出力されます。未設定時は内部クロック（通常 40 MHz または 20 MHz）が CLKO ピンに出力されます。 |
| `xstby-enable` | boolean | いいえ | false | GPIO0 のスタンバイピン制御を有効化します。スタンバイ制御入力をサポートするトランシーバ用です。 |
| `clko-div` | int | いいえ | 10 | CLKO ピンのシステムクロック分周比。許容値: `1`, `2`, `4`, `10`。 |

### CAN コントローラ共通プロパティ

ソース: `dts/bindings/can/can-controller.yaml`（`can-fd-controller.yaml` 経由で継承）

| プロパティ | 型 | 必須 | デフォルト | 説明 |
|-----------|-----|------|-----------|------|
| `bitrate` | int | いいえ | `CONFIG_CAN_DEFAULT_BITRATE` | CAN アービトレーションフェーズの初期ビットレート（bit/s）。 |
| `sample-point` | int | いいえ | 自動 | 初期サンプルポイント（パーミル）。例: `875` = 87.5%。未設定時はビットレートに基づいて自動選択: >800 kbit/s で 75.0%、>500 kbit/s で 80.0%、それ以外は 87.5%。 |
| `phys` | phandle | いいえ | -- | 能動制御される CAN トランシーバノードへの参照（例: `can-transceiver-gpio`）。 |
| `bus-speed` | int | いいえ | -- | **非推奨。** `bitrate` に名称変更されました。 |

### CAN FD コントローラプロパティ

ソース: `dts/bindings/can/can-fd-controller.yaml`

| プロパティ | 型 | 必須 | デフォルト | 説明 |
|-----------|-----|------|-----------|------|
| `bitrate-data` | int | いいえ | `CONFIG_CAN_DEFAULT_BITRATE_DATA` | CAN FD データフェーズの初期ビットレート（bit/s）。 |
| `sample-point-data` | int | いいえ | 自動 | データフェーズの初期サンプルポイント（パーミル）。未設定時は `bitrate-data` に基づいて自動選択（`sample-point` と同じルール）。 |
| `bus-speed-data` | int | いいえ | -- | **非推奨。** `bitrate-data` に名称変更されました。 |

### CAN トランシーバ子ノードプロパティ

ソース: `dts/bindings/can/can-controller.yaml`（子バインディング）

パッシブ CAN トランシーバは `can-transceiver` という名前の**子ノード**として記述します:

| プロパティ | 型 | 必須 | デフォルト | 説明 |
|-----------|-----|------|-----------|------|
| `max-bitrate` | int | **はい** | -- | CAN トランシーバがサポートする最大ビットレート（bit/s）。 |
| `min-bitrate` | int | いいえ | -- | CAN トランシーバがサポートする最小ビットレート（bit/s）。 |

例:

```dts
mcp251xfd@0 {
    compatible = "microchip,mcp251xfd";
    /* ... */

    can-transceiver {
        max-bitrate = <1000000>;
    };
};
```

### SPI デバイスプロパティ

ソース: `dts/bindings/spi/spi-device.yaml`（`include: [spi-device.yaml, ...]` 経由で継承）

MCP251XFD ノードは SPI バス上に配置され、標準的な SPI デバイスプロパティをすべて継承します:

| プロパティ | 型 | 必須 | デフォルト | 説明 |
|-----------|-----|------|-----------|------|
| `reg` | int | **はい** | -- | SPI バス上のチップセレクトインデックス（通常 `<0x0>`）。 |
| `spi-max-frequency` | int | **はい** | -- | SPI クロックの最大周波数（Hz）。MCP251XFD データシートに記載されたデバイスおよびボード設計の制限を超えないようにしてください。Zephyr のドライバとバインディングは特定の上限を強制しません。ツリー内の例では一般的に 18--20 MHz の値が使用されています。 |
| `duplex` | int | いいえ | 0（全二重） | SPI デュプレックスモード。`0` = `SPI_FULL_DUPLEX`、`2048` = `SPI_HALF_DUPLEX`。MCP251XFD は**全二重**が必要です。 |
| `frame-format` | int | いいえ | 0（Motorola） | SPI フレームフォーマット。`0` = `SPI_FRAME_FORMAT_MOTOROLA`、`32768` = `SPI_FRAME_FORMAT_TI`。MCP251XFD は **Motorola** フォーマットを使用します。 |
| `spi-cpol` | boolean | いいえ | false | SPI クロック極性（アイドル状態）。設定するとクロックがハイでアイドルになります（CPOL=1）。MCP251XFD はデフォルトで **SPI モード 0,0**（CPOL=0, CPHA=0）で動作します。ハードウェアが要求しない限り設定**しないでください**。 |
| `spi-cpha` | boolean | いいえ | false | SPI クロック位相。設定すると 2 番目のエッジでデータがサンプリングされます（CPHA=1）。MCP251XFD はデフォルトで **SPI モード 0,0** で動作します。ハードウェアが要求しない限り設定**しないでください**。 |
| `spi-lsb-first` | boolean | いいえ | false | 設定すると LSB ファーストで送信されます。MCP251XFD は **MSB ファースト**（デフォルト）を使用します。設定**しないでください**。 |
| `spi-hold-cs` | boolean | いいえ | false | 複数の SPI トランザクション間でチップセレクトをアクティブに保持します。MCP251XFD では通常不要です。 |
| `spi-cs-high` | boolean | いいえ | false | チップセレクトがアクティブハイ。MCP251XFD は**アクティブロー** CS を使用します。設定**しないでください**。 |
| `spi-interframe-delay-ns` | int | いいえ | 0 | SPI ワード間の遅延（ナノ秒）。デフォルトの 0 は SCK 周期の半分を使用します。 |
| `spi-cs-setup-delay-ns` | int | いいえ | -- | CS アサート後、最初のクロックエッジまでの遅延（ナノ秒、イネーブルリードタイム）。 |
| `spi-cs-hold-delay-ns` | int | いいえ | -- | 最後のクロックエッジ後、CS ディアサートまでの遅延（ナノ秒、イネーブルラグタイム）。 |

### 標準プロパティ

| プロパティ | 型 | 必須 | 説明 |
|-----------|-----|------|------|
| `compatible` | string | **はい** | MCP2517FD、MCP2518FD、MCP251863 すべてのバリアントで `"microchip,mcp251xfd"` を指定します。 |
| `status` | string | いいえ | デバイスを有効にするには `"okay"` に設定します。 |

### オーバーレイ例

#### 最小限のオーバーレイ

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

#### フル機能オーバーレイ（CAN FD + トランシーバ）

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

#### 実例: CANIS CANPico シールド

`boards/shields/canis_canpico/canis_canpico.overlay` より:

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

#### 実例: phyBOARD-Polis i.MX8M Mini

`boards/phytec/phyboard_polis/phyboard_polis_mimx8mm6_m4.dts` より:

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

#### MikroElektronika MCP2518FD Click シールド

`boards/shields/mikroe_mcp251xfd_click/mikroe_mcp2518fd_click.overlay` より:

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

## 内部実装の詳細

### アーキテクチャ

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

### RAM レイアウト

MCP251XFD はアドレス `0x400` から始まる **2048 バイト**のオンチップ RAM を持ちます。この RAM はコンパイル時に 3 つの FIFO 領域に分割されます:

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

**主要定数:**

| 定数 | 値 |
|------|-----|
| `MCP251XFD_RAM_START_ADDR` | `0x400` |
| `MCP251XFD_RAM_SIZE` | `2048` バイト |
| `MCP251XFD_RAM_ALIGNMENT` | 4 バイト |
| `MCP251XFD_PAYLOAD_SIZE` | `CAN_MAX_DLEN`（Classic CAN: 8 バイト、CAN FD: 64 バイト） |

**サイズ計算式:**

| FIFO | アイテムサイズ | 計算式 |
|------|---------------|--------|
| TEF FIFO | 8 バイト | `TEF_FIFO_SIZE = MAX_TX_QUEUE * 8` |
| TX キュー | `8 + PAYLOAD_SIZE` | `TX_QUEUE_SIZE = MAX_TX_QUEUE * (8 + PAYLOAD_SIZE)` |
| RX FIFO | `8 + PAYLOAD_SIZE`（`CONFIG_CAN_RX_TIMESTAMP` 有効時 + 4） | `RX_FIFO_SIZE = RX_FIFO_ITEMS * ITEM_SIZE` |

**メモリ使用量計算例 -- Classic CAN（PAYLOAD_SIZE = 8）:**

デフォルト設定（`MAX_TX_QUEUE=8`, `RX_FIFO_ITEMS=16`, タイムスタンプなし）:

| 領域 | アイテム数 | アイテムサイズ | 合計 |
|------|-----------|---------------|------|
| TEF FIFO | 8 | 8 | 64 バイト |
| TX キュー | 8 | 16 | 128 バイト |
| RX FIFO | 16 | 16 | 256 バイト |
| **合計** | | | **448 バイト** / 2048 |

**メモリ使用量計算例 -- CAN FD（PAYLOAD_SIZE = 64）+ タイムスタンプ:**

チューニング設定（`MAX_TX_QUEUE=4`, `RX_FIFO_ITEMS=8`, タイムスタンプ有効）:

| 領域 | アイテム数 | アイテムサイズ | 合計 |
|------|-----------|---------------|------|
| TEF FIFO | 4 | 8 | 32 バイト |
| TX キュー | 4 | 72 | 288 バイト |
| RX FIFO | 8 | 76 | 608 バイト |
| **合計** | | | **928 バイト** / 2048 |

> **注意:** `can_mcp251xfd.h` 内の `BUILD_ASSERT` により、FIFO の合計割り当てが 2048 バイトを超えないことが保証されます。設定が超過するとビルドが失敗します。

### SPI 通信プロトコル

ドライバは以下の主要な SPI コマンドタイプで MCP251XFD と通信します（RESET や WRITE_CRC などの追加オペコードもヘッダに存在しますが、初期化時のみ使用されるか予約されています）:

| コマンド | オペコードビット | 説明 |
|---------|----------------|------|
| **Read** (`mcp251xfd_read_reg`) | `0b0011` | 標準レジスタ読み出し。2 バイトコマンドヘッダ + データ。 |
| **Read with CRC** (`mcp251xfd_read_crc`) | `0b1011` | CRC 付き読み出し。2 バイトコマンド + 1 バイト長 + データ + 2 バイト CRC。データ整合性確保のため SFR 読み出しに使用されます。 |
| **Write** (`mcp251xfd_write`) | `0b0010` | レジスタ書き込み。2 バイトコマンドヘッダ + データ。 |

- **CRC 多項式:** `0x8005`（CRC-16/USB）-- USB 規格で広く使用される 16 ビット多項式で、最大 64 ビットのデータ長に対して HD=6（ハミング距離 6）を提供します（参考: [Koopman CRC Catalog](https://users.ece.cmu.edu/~koopman/crc/index.html)）。
- **CRC 初期値:** `0xFFFF`
- **SPI ワードサイズ:** 8 ビット（インスタンシエーションマクロの `SPI_WORD_SET(8)` で設定）
- **Read CRC リトライ回数:** `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES`（デフォルト: 5）で制御

### 割り込み処理

MCP251XFD は**レベルトリガ、アクティブロー**の割り込み出力を使用します:

1. **GPIO コールバック** (`mcp251xfd_int_gpio_callback`): INT ピンがローになると GPIO コールバックが発火します。即座にピン割り込みを**無効化**し、割り込みセマフォ（`int_sem`）をシグナルします。

2. **割り込みスレッド** (`mcp251xfd_int_thread`): 専用の協調型スレッド（優先度: `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO`、スタック: `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE`）が `int_sem` を待機します。シグナルされると `mcp251xfd_handle_interrupts()` を呼び出し、以下を処理します:
   - **RXIF** -- 受信 FIFO（RX フィルタコールバックにディスパッチ） -- 常に有効
   - **TEFIF** -- 送信イベント FIFO（TX 完了コールバック） -- 常に有効
   - **MODIF** -- モード変更割り込み -- 常に有効
   - **CERRIF** -- CAN エラー割り込み（バスオフ、エラーパッシブ、エラーワーニング） -- 常に有効
   - **IVMIF** -- 無効メッセージ割り込み（プロトコルエラー、CRC エラー） -- 割り込みループで処理されますが、INT イネーブルレジスタでは**有効化されません**（`IVMIE` は設定されません）
   - **RXOVIF** -- RX オーバーフロー割り込み -- `CONFIG_CAN_STATS` 設定時のみ有効

3. 処理後、スレッドはレベルトリガ GPIO 割り込みを**再有効化**します。

> **重要:** ホスト MCU の GPIO コントローラは**レベルトリガ割り込み**（`GPIO_INT_LEVEL_ACTIVE`）を**サポートする必要があります**。エッジトリガ割り込みでは、MCP251XFD が最初の割り込みが応答される前に別の割り込みをアサートした場合、イベントの取りこぼしが発生する可能性があります。

### 初期化シーケンス

`mcp251xfd_init()` 関数は以下のステップを実行します:

1. 外部クロックコントローラの有効化（`clocks` プロパティが存在する場合）
2. セマフォ（`int_sem`, `tx_sem`）およびミューテックスの初期化
3. SPI バスと GPIO ポートの準備確認
4. INT GPIO をレベルトリガ割り込み付き入力として設定
5. 割り込みハンドラスレッドの起動
6. SPI 経由で MCP251XFD を**リセット**（`mcp251xfd_reset`）
7. リセット後にチップがコンフィギュレーションモードに入ったことを確認
8. `bitrate`/`sample-point`（CAN FD の場合は `bitrate-data`/`sample-point-data` も）からビットタイミングを計算
9. レジスタの初期化:
   - **CON** レジスタ（ISO CRC 有効化、TX 帯域共有、プロトコル例外無効化）
   - **OSC** レジスタ（PLL 有効/無効、CLKO 分周、CLKO/SOF 選択）
   - **IOCON** レジスタ（スタンバイピン、GPIO 設定）
   - **INT** レジスタ（RXIE, MODIE, TEFIE, CERRIE を有効化、`CONFIG_CAN_STATS` 有効時は RXOVIE も）
   - **TDC** レジスタ（送信遅延補償 -- 初期は無効、Mixed/CAN FD モード移行時に有効化）
   - **TSCON** レジスタ（`CONFIG_CAN_RX_TIMESTAMP` 時のタイムスタンプ有効化とプリスケーラ）
10. FIFO の初期化:
    - TEF FIFO（深さ = `MAX_TX_QUEUE`）
    - TX キュー（深さ = `MAX_TX_QUEUE`、ペイロード = `PAYLOAD_SIZE`）
    - RX FIFO（深さ = `RX_FIFO_ITEMS`、ペイロード = `PAYLOAD_SIZE`、タイムスタンプはオプション）
11. ノミナルビットタイミングの適用（CAN FD の場合はデータビットタイミングも）

### 動作モードマッピング

ドライバは Zephyr CAN モードを MCP251XFD ハードウェアモードにマッピングします:

| Zephyr モード | MCP251XFD モード | レジスタ値 |
|--------------|-----------------|-----------|
| `CAN_MODE_NORMAL` | CAN 2.0 | `MODE_CAN2_0` (6) |
| `CAN_MODE_FD` | Mixed（CAN FD） | `MODE_MIXED` (0) |
| `CAN_MODE_LISTENONLY` | Listen Only | `MODE_LISTENONLY` (3) |
| `CAN_MODE_LOOPBACK` | External Loopback | `MODE_EXT_LOOPBACK` (5) |
| `CAN_MODE_3_SAMPLES` | -- | 非サポート（`-ENOTSUP`） |
| `CAN_MODE_ONE_SHOT` | -- | 非サポート（`-ENOTSUP`） |

> コンフィギュレーションモードから Mixed（CAN FD）モードに切り替える際、送信遅延補償（TDC）が自動モードで自動的に有効化されます。CAN 2.0 またはループバックモードに切り替える場合、TDC は無効化されます。

### データ構造体

#### `mcp251xfd_config`（デバイスごとの定数設定）

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

#### `mcp251xfd_data`（デバイスごとの可変ランタイム状態）

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

### デバイスインスタンシエーションマクロ

`MCP251XFD_INIT` マクロ（`can_mcp251xfd.c` の末尾）は各 MCP251XFD インスタンスの Devicetree プロパティを config 構造体にマッピングします:

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

**主要マッピング詳細:**

| DT プロパティ / Kconfig | Config 構造体フィールド | 備考 |
|------------------------|----------------------|------|
| `osc-freq` | `.osc_freq` | 直接マッピング |
| `int-gpios` | `.int_gpio_dt` | `GPIO_DT_SPEC_INST_GET` 経由 |
| `sof-on-clko` | `.sof_on_clko` | boolean DT プロパティ |
| `xstby-enable` | `.xstby_enable` | boolean DT プロパティ |
| `pll-enable` | `.pll_enable` | boolean DT プロパティ |
| `clko-div` | `.clko_div` | **enum インデックスとしてマッピング**（0=1, 1=2, 2=4, 3=10）`DT_INST_ENUM_IDX` 経由 |
| `timestamp-prescaler` | `.timestamp_prescaler` | 直接マッピング |
| `reg`, `spi-max-frequency` | `.bus` | `SPI_DT_SPEC_INST_GET` 経由 |
| `bitrate`, `sample-point` 等 | `.common` | `CAN_DT_DRIVER_CONFIG_INST_GET` 経由 |
| `clocks` | `.clk_dev`, `.clk_id` | オプション、`MCP251XFD_SET_CLOCK` 経由 |

### API 関数

ドライバは Zephyr CAN API を完全に実装しています:

| API 関数 | ドライバ実装 | 説明 |
|---------|-------------|------|
| `can_get_capabilities()` | `mcp251xfd_get_capabilities` | `NORMAL`, `LISTENONLY`, `LOOPBACK`（CAN FD 有効時は `FD` も）を返します |
| `can_set_mode()` | `mcp251xfd_set_mode` | 動作モードを設定（`start` 前に呼び出す必要があります） |
| `can_set_timing()` | `mcp251xfd_set_timing` | ノミナルビットタイミングパラメータを設定 |
| `can_set_timing_data()` | `mcp251xfd_set_timing_data` | データフェーズビットタイミングを設定（CAN FD のみ） |
| `can_start()` | `mcp251xfd_start` | コンフィギュレーションモードから動作モードに移行 |
| `can_stop()` | `mcp251xfd_stop` | すべての TX を中止し、コンフィギュレーションモードに戻る |
| `can_send()` | `mcp251xfd_send` | 送信用に CAN フレームをキューに追加 |
| `can_add_rx_filter()` | `mcp251xfd_add_rx_filter` | RX 受信フィルタを追加（最大 `CONFIG_CAN_MAX_FILTER` 個） |
| `can_remove_rx_filter()` | `mcp251xfd_remove_rx_filter` | RX フィルタを削除 |
| `can_get_state()` | `mcp251xfd_get_state` | 現在の CAN バスエラー状態とカウンタを取得 |
| `can_set_state_change_callback()` | `mcp251xfd_set_state_change_callback` | 状態変更コールバックを登録 |
| `can_get_core_clock()` | `mcp251xfd_get_core_clock` | 設定されたオシレータ周波数（`osc_freq`）から導出された CAN コアクロックを返します |
| `can_get_max_filters()` | `mcp251xfd_get_max_filters` | `CONFIG_CAN_MAX_FILTER` を返します |

**ビットタイミング制限:**

| パラメータ | ノミナル最小 | ノミナル最大 | データ最小 | データ最大 |
|-----------|------------|------------|----------|----------|
| SJW | 1 | 128 | 1 | 16 |
| prop_seg | 0 | 0 | 0 | 0 |
| phase_seg1 | 2 | 256 | 1 | 32 |
| phase_seg2 | 1 | 128 | 1 | 16 |
| prescaler | 1 | 256 | 1 | 256 |

---

## ハードウェア要件

### SPI インターフェース

- **SPI モード:** Mode 0,0（CPOL=0, CPHA=0） -- デフォルト
- **最大 SPI クロック:**
  - MCP2517FD: **18 MHz**
  - MCP2518FD / MCP251863: **20 MHz**
- **ワードサイズ:** 8 ビット
- **デュプレックス:** 全二重が必要
- **チップセレクト:** アクティブロー

### 割り込みピン

- **タイプ:** MCP251XFD からのアクティブロー、プッシュプル出力
- **要件:** ホスト MCU の GPIO コントローラは**レベルトリガ割り込み**（`GPIO_INT_LEVEL_ACTIVE`）を**サポートする必要があります**。ドライバは GPIO をレベルトリガとして設定します。エッジトリガ割り込みは**サポートされず**、割り込みの取りこぼしが発生します。

### オシレータ / 水晶振動子

- 一般的な周波数: **20 MHz** または **40 MHz** の外部水晶発振子/オシレータ
- Devicetree の `osc-freq` プロパティで指定
- `pll-enable` を設定すると、内部 PLL がオシレータ周波数を 10 倍に逓倍します（例: 4 MHz オシレータ x 10 = 40 MHz CAN コアクロック）
- PLL 準備完了タイムアウト: 100 ms（100 回リトライ）

### 電源

- 動作電圧: 2.7V -- 5.5V（データシート参照）
- phyBOARD-Polis の例では電源制御用の `supply-gpios` プロパティが示されていますが、これはボード固有の追加であり標準バインディングの一部ではありません

---

## シールドの使用方法

Zephyr は MCP251XFD ベースの Click ボード用のシールド定義を提供しています:

### 利用可能なシールド

| シールド名 | チップ | トランシーバ |
|-----------|--------|-------------|
| `mikroe_mcp2517fd_click` | MCP2517FD | 外付け ATA6563 |
| `mikroe_mcp2518fd_click` | MCP2518FD | 外付け ATA6563 |
| `mikroe_mcp251863_click` | MCP251863 | 内蔵 ATA6563 |
| `canis_canpico` | MCP251XFD | オンボードトランシーバ |

### ビルドコマンド

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

# CANIS CANPico shield (Raspberry Pi Pico)
west build -b rpi_pico samples/drivers/can/counter \
    --shield canis_canpico
```

### シールドの要件

- MikroElektronika Click シールドの場合、ターゲットボードは `mikrobus_spi` と `mikrobus_header` のノードラベルを定義する必要があります（[Zephyr シールドドキュメント](https://docs.zephyrproject.org/latest/hardware/porting/shields/index.html) を参照）。
- ターゲットボードの GPIO コントローラは**レベルトリガ割り込み**をサポートする必要があります。
- ターゲットボードの SPI コントローラは最大 **18 MHz** のクロック周波数をサポートする必要があります。
