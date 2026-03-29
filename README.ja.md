# study-ZephyrCAN

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/uist1idrju3i/study-ZephyrCAN)

> [English version is here](README.md)

## 動作確認済みハードウェア

以下のハードウェアで study-ZephyrCAN の動作を確認しています:

- [Seeed XIAO nRF54L15](https://wiki.seeedstudio.com/xiao_nrf54l15_sense_getting_started/) (ボードターゲット: xiao_nrf54l15/nrf54l15/cpuapp)
- Microchip [MCP251863](https://www.microchip.com/en-us/product/MCP251863) (CAN FD コントローラ + トランシーバ)

## 開発環境バージョン

- nRF Connect SDK toolchain v3.2.1
- nRF Connect SDK v3.2.1

---

## CAN サンプルアプリケーション

本プロジェクトは、Zephyr RTOS 上で動作する CAN バス送受信サンプルです。nRF54L15 SoC（ネイティブ CAN ペリフェラルなし）が SPI 経由で外部 MCP251863 CAN FD コントローラと通信します。

### システム構成

```mermaid
block-beta
  columns 3

  block:MCU["Seeed XIAO nRF54L15"]:2
    columns 2
    APP["Zephyr アプリケーション\n(src/main.c)"]
    CAN_API["Zephyr CAN API\n(can_send / can_add_rx_filter)"]
    SPI_DRV["SPI マスタードライバ\n(SPIM0)"]
    MCP_DRV["MCP251XFD ドライバ\n(can_mcp251xfd)"]
  end

  block:EXT["MCP251863"]:1
    columns 1
    CTRL["CAN FD\nコントローラ"]
    XCVR["CAN\nトランシーバ"]
  end

  SPI_DRV --> CTRL
  XCVR --> BUS["CAN バス"]

  style MCU fill:#e3f2fd,stroke:#1565c0
  style EXT fill:#fff3e0,stroke:#e65100
```

### ピンアサイン

| 信号 | GPIO | SPI インスタンス | 説明 |
|------|------|-----------------|------|
| SCK | P1.1 | SPI00 | SPI クロック |
| MOSI | P1.2 | SPI00 | SPI データ出力 |
| MISO | P1.3 | SPI00 | SPI データ入力 |
| CS | P1.0 | SPI00 | チップセレクト (アクティブ Low) |
| INT | P1.8 | - | MCP251863 割り込み (アクティブ Low) |
| WS2812 | P1.4-P1.7 | SPI20-SPI30 | LED ストリップデータ (既存) |

> **注意:** ピンアサインはプレースホルダです。フラッシュ前に実際のハードウェア配線と照合してください。

### アプリケーションフロー

```mermaid
flowchart TD
    A([main]) --> B[can_device_init]
    B --> B1{デバイス準備完了?}
    B1 -->|No| ERR1([エラーで終了])
    B1 -->|Yes| B2[状態変更コールバック登録]
    B2 --> B3[CAN_MODE_NORMAL 設定]
    B3 --> B4[can_start リトライ\n最大10回]
    B4 --> B5{起動成功?}
    B5 -->|No| ERR1
    B5 -->|Yes| C[can_setup_rx_filter\nID=0x200 mask=0x7FF]
    C --> C1{フィルタ登録成功?}
    C1 -->|No| ERR1
    C1 -->|Yes| D[メイン送信ループ]

    D --> E[1000 ms スリープ]
    E --> F{バスオフ?}
    F -->|Yes| G[can_recover_from_bus_off\nstop -> sleep -> restart]
    G --> G1{復帰成功?}
    G1 -->|No| E
    G1 -->|Yes| H
    F -->|No| H[TX フレーム構築\nID=0x100 DLC=8]
    H --> I[can_send_frame_with_timeout]
    I --> I1[can_send K_NO_WAIT]
    I1 --> I2[k_sem_take 100 ms タイムアウト]
    I2 --> I3{成功?}
    I3 -->|Yes| J[TX ログ + カウンタ増加]
    I3 -->|No| K[エラーログ + tx_error_count 増加]
    J --> L{counter % 10 == 0?}
    K --> L
    L -->|Yes| M[統計ログ出力:\nTX / RX / TX_ERR]
    L -->|No| E
    M --> E
```

### CAN ステートマシン

アプリケーションはコールバックで CAN コントローラのエラー状態を監視し、バスオフ復帰を行います。

```mermaid
stateDiagram-v2
    [*] --> ErrorActive : can_start()

    ErrorActive --> ErrorWarning : TEC or REC >= 96
    ErrorWarning --> ErrorActive : カウンタ減少
    ErrorWarning --> ErrorPassive : TEC or REC >= 128
    ErrorPassive --> ErrorWarning : カウンタ減少
    ErrorPassive --> BusOff : TEC >= 256

    BusOff --> Stopped : can_stop()
    Stopped --> ErrorActive : can_start()\n(復帰)

    note right of BusOff
        復帰シーケンス:
        1. can_stop()
        2. k_msleep(1000)
        3. can_start()
    end note
```

### TX 完了フロー

送信はメインスレッドと ISR コールバック間のセマフォベースの同期で行われます。

```mermaid
sequenceDiagram
    participant Main as メインスレッド
    participant API as Zephyr CAN API
    participant ISR as TX コールバック (ISR)

    Main->>Main: k_sem_reset(&tx_sem)
    Main->>API: can_send(frame, K_NO_WAIT, callback)
    API-->>Main: 0 (キュー投入済み)
    Main->>Main: k_sem_take(&tx_sem, 100 ms)

    Note over API: ハードウェアがフレームを送信

    API->>ISR: can_tx_callback(error)
    ISR->>ISR: tx_callback_error = error
    ISR->>Main: k_sem_give(&tx_sem)

    Main->>Main: tx_callback_error を確認
    alt error == 0
        Main->>Main: tx_count++
    else error != 0
        Main->>Main: tx_error_count++
    end
```

### ファイル構成

| ファイル | 説明 |
|----------|------|
| `src/main.c` | CAN アプリケーション: 初期化、TX/RX、コールバック、バスオフ復帰 |
| `app.overlay` | Devicetree オーバーレイ: SPI00 + MCP251863、WS2812 LED |
| `prj.conf` | Kconfig: CAN ドライバ、BLE、ログ、ペリフェラル |
| `mcp251xfd.md` | MCP251XFD ドライバ技術ドキュメント (英語) |
| `mcp251xfd.ja.md` | 同ドキュメント日本語版 |

### 設定定数

`src/main.c` で定義:

| 定数 | 値 | 説明 |
|------|----|------|
| `CAN_TX_MSG_ID` | `0x100` | 送信フレームの CAN ID |
| `CAN_RX_FILTER_ID` | `0x200` | 受信フィルタの CAN ID |
| `CAN_RX_FILTER_MASK` | `0x7FF` | フィルタマスク (完全一致) |
| `CAN_TX_INTERVAL_MS` | `1000` | 送信間隔 (ms) |
| `CAN_SEND_TIMEOUT_MS` | `100` | TX 完了タイムアウト (ms) |
| `CAN_INIT_MAX_RETRIES` | `10` | `can_start()` 最大リトライ回数 |
| `CAN_INIT_RETRY_DELAY_MS` | `500` | 初期化リトライ間隔 (ms) |
| `CAN_RECOVERY_DELAY_MS` | `1000` | バスオフ復帰時の待機時間 (ms) |

### Kconfig (prj.conf の CAN セクション)

| シンボル | 値 | 説明 |
|----------|----|------|
| `CONFIG_CAN` | `y` | CAN サブシステム有効化 |
| `CONFIG_CAN_MCP251XFD_MAX_TX_QUEUE` | `8` | TX キュー深度 |
| `CONFIG_CAN_MCP251XFD_RX_FIFO_ITEMS` | `16` | RX FIFO 深度 |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_STACK_SIZE` | `1536` | 割り込みハンドラスタック (デフォルト: 768) |
| `CONFIG_CAN_MCP251XFD_INT_THREAD_PRIO` | `2` | 割り込みハンドラスレッド優先度 |
| `CONFIG_CAN_MCP251XFD_READ_CRC_RETRIES` | `5` | SPI 読み取り CRC リトライ回数 |
| `CONFIG_CAN_DEFAULT_BITRATE` | `500000` | デフォルト CAN ビットレート (500 kbps) |

### TX フレームフォーマット

```
Byte:  [0]    [1]    [2]   [3]   [4]   [5]   [6]   [7]
Data:  CNT_H  CNT_L  0xCA  0xFE  0xDE  0xAD  0xBE  0xEF
       |___________|
        ローリングカウンタ (ビッグエンディアン uint16)
```

- **CAN ID:** `0x100` (標準 11 ビット)
- **DLC:** 8
- **Byte 0-1:** ローリングカウンタ (送信成功ごとに +1)
- **Byte 2-7:** 固定パターン `0xCAFEDEADBEEF`

## ビルド

```bash
west build -b xiao_nrf54l15/nrf54l15/cpuapp
```

## フラッシュ

```bash
west flash
```

> USB またはデバッグプローブで実機を接続する必要があります。

## 参考資料

- [Zephyr CAN API](https://docs.zephyrproject.org/latest/hardware/peripherals/can/index.html)
- [MCP251XFD ドライバドキュメント](mcp251xfd.md) ([日本語](mcp251xfd.ja.md))
- [Microchip MCP251863 製品ページ](https://www.microchip.com/en-us/product/MCP251863)
