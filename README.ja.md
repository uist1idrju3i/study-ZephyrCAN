# study-ZephyrCAN

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/uist1idrju3i/study-ZephyrCAN)

> [English version is here](README.md)

## 対象ハードウェア (ビルド確認済み)

以下のハードウェアを対象としています (ビルド確認済み、実機未テスト):

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
flowchart LR
    subgraph MCU["Seeed XIAO nRF54L15"]
        APP["Zephyr Application\n(src/main.c)"]
        CAN_API["Zephyr CAN API\n(can_send / can_add_rx_filter)"]
        MCP_DRV["MCP251XFD Driver\n(can_mcp251xfd)"]
        SPI_DRV["SPI Master Driver\n(SPI00)"]
        APP --> CAN_API --> MCP_DRV --> SPI_DRV
    end
    subgraph EXT["MCP251863"]
        CTRL["CAN FD Controller\n(MCP2518FD)"]
        XCVR["CAN Transceiver\n(ATA6563)"]
        CTRL --> XCVR
    end
    SPI_DRV -- "SPI (SCK/MOSI/MISO/CS)\nGPIO (INT)" --> CTRL
    XCVR --> BUS["CAN Bus\n(CANH / CANL)"]
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

### ハードウェア接続図

```mermaid
flowchart LR
    subgraph XIAO["XIAO nRF54L15"]
        P10["P1.0 (CS)"]
        P11["P1.1 (SCK)"]
        P12["P1.2 (MOSI)"]
        P13["P1.3 (MISO)"]
        P18["P1.8 (INT)"]
    end
    subgraph MCP["MCP251863"]
        CS_PIN["nCS"]
        SCK_PIN["SCK"]
        SI_PIN["SI"]
        SO_PIN["SO"]
        INT_PIN["nINT"]
        CANH["CANH"]
        CANL["CANL"]
    end
    P10 --- CS_PIN
    P11 --- SCK_PIN
    P12 --- SI_PIN
    P13 --- SO_PIN
    P18 --- INT_PIN
    CANH --- BUS["CAN Bus"]
    CANL --- BUS
```

### アプリケーションフロー

```mermaid
flowchart TD
    A([main]) --> B[can_device_init]
    B --> B1{デバイス準備完了?}
    B1 -->|No| ERR1([goto halt])
    B1 -->|Yes| B2[状態変更コールバック登録]
    B2 --> B3[CAN_MODE_NORMAL 設定]
    B3 --> B4[can_start リトライ\n最大10回]
    B4 --> B5{起動成功?}
    B5 -->|No| ERR1
    B5 -->|Yes| C[can_setup_rx_filter\nID=0x200 mask=0x7FF]
    C --> C1{フィルタ登録成功?}
    C1 -->|No| ERR1

    ERR1 --> HALT(["致命的エラー - システム停止\n(無限スリープループ)"])
    C1 -->|Yes| D[メイン送信ループ]

    D --> E[1000 ms スリープ]
    E --> F{バスオフ /\n停止?}
    F -->|Yes| G[can_recover_controller\nstop -> sleep -> restart]
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

送信はメインスレッドとドライバコールバックスレッド間のセマフォベースの同期で行われます。

```mermaid
sequenceDiagram
    participant Main as Main Thread
    participant API as Zephyr CAN API
    participant CB as TX Callback

    Main->>Main: tx_generation++
    Main->>Main: k_sem_reset(&tx_sem)
    Main->>API: can_send(frame, K_NO_WAIT, callback, generation)
    API-->>Main: 0 (queued)
    Main->>Main: k_sem_take(&tx_sem, 100 ms)

    Note over API: Hardware transmits frame

    API->>CB: can_tx_callback(error, user_data=generation)
    CB->>CB: Check generation match
    alt generation matches
        CB->>CB: tx_callback_error = error
        CB->>Main: k_sem_give(&tx_sem)
    else stale callback
        CB->>CB: Discard (log warning)
    end

    Main->>Main: Check tx_callback_error
    alt error == 0
        Main->>Main: tx_count++
    else error != 0
        Main->>Main: tx_error_count++
    end
```

### ファイル構成

| ファイル | 説明 |
|----------|------|
| `CMakeLists.txt` | Zephyr 用 CMake ビルド設定 |
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
| `CAN_RECOVERY_MAX_RETRIES` | `5` | バックオフ前の最大連続復帰失敗回数 |
| `CAN_RECOVERY_BACKOFF_MS` | `5000` | 連続復帰失敗後の拡張待機時間 (ms) |
| `CAN_TX_DATA_LEN` | `8` | TX フレームあたりのデータバイト数 (標準 CAN 最大値) |
| `STATS_PRINT_INTERVAL` | `10` | 統計ログ出力間隔 (成功 TX フレーム数) |
| `CAN_TX_ERR_BURST_THRESHOLD` | `5` | バースト警告前の連続 TX 失敗回数 |

> **注意:** `prj.conf` には親プロジェクト OpenBlink の BLE、mruby/c VM、その他ペリフェラル用の設定も含まれています。本サンプルに関連するのは以下の CAN 関連設定のみです。

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
| `CONFIG_CAN_MANUAL_RECOVERY_MODE` | (未設定) | 手動バスオフ復帰 (デフォルト: n = 自動復帰) |

> **バスオフ復帰に関する注意:** 現在の設定では自動復帰がデフォルトで有効ですが、`src/main.c` は `can_recover_controller()` による手動復帰ロジックを実装しています。両方のメカニズムが併存しています。量産時は、手動復帰コードと整合させるために `CONFIG_CAN_MANUAL_RECOVERY_MODE=y` を設定するか、アプリケーションから手動復帰ロジックを削除して自動復帰のみに委ねるか、どちらか一方に統一してください。

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

### 期待されるログ出力

正常動作時、以下のようなログが出力されます:

```
[INF] CAN sample application starting...
[INF] CAN device mcp251863@0 is ready
[INF] CAN controller started successfully
[INF] RX filter added: ID=0x200 mask=0x7FF (filter_id=0)
[INF] Entering main TX loop (interval=1000 ms)
[INF] TX: ID=0x100 counter=0
[INF] TX: ID=0x100 counter=1
...
[INF] Stats: TX=10 RX=0 TX_ERR=0
```

## ビルド

```bash
west build -b xiao_nrf54l15/nrf54l15/cpuapp
```

## フラッシュ

```bash
west flash
```

> USB またはデバッグプローブで実機を接続する必要があります。

## トラブルシューティング

| 症状 | 考えられる原因 | 対処方法 |
|------|--------------|----------|
| CAN コントローラが起動しない | SPI 配線エラーまたは `osc-freq` の不一致 | SPI 信号接続 (SCK, MOSI, MISO, CS) を確認してください。app.overlay の `osc-freq` が実際の水晶発振子/オシレータ周波数と一致しているか確認してください。 |
| TX タイムアウトが頻発する | CAN バス上に他のノードがない、または終端抵抗が未接続 | CAN は最低 2 つのアクティブノードが必要です。バス両端に 120 Ω の終端抵抗があることを確認してください。 |
| Bus-Off が頻発する | ビットレート不一致、バス長過大、または終端抵抗の問題 | すべてのノードで同一ビットレートを使用してください。ケーブル長と品質を確認してください。バス両端に 120 Ω の終端抵抗があることを確認してください。 |
| INT ピンの割り込みが動作しない | レベルトリガではなくエッジトリガが設定されている | MCP251XFD はレベルトリガ (`GPIO_INT_LEVEL_ACTIVE`) 割り込みが必須です。エッジトリガではイベントの取りこぼしが発生します。GPIO コントローラの互換性を確認してください。 |
| SPI CRC エラーが頻発する | SPI クロック速度が速すぎるか配線が長い | `spi-max-frequency` を下げてください (例: 18 MHz → 8 MHz)。SPI 信号の配線長を短くしてください。 |

## 参考資料

- [Zephyr CAN API](https://docs.zephyrproject.org/latest/hardware/peripherals/can/index.html)
- [MCP251XFD ドライバドキュメント](mcp251xfd.md) ([日本語](mcp251xfd.ja.md))
- [Microchip MCP251863 製品ページ](https://www.microchip.com/en-us/product/MCP251863)

## ライセンス

本プロジェクトは [BSD 3-Clause License](LICENSE) の下でライセンスされています。
