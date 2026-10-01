# FM-1 Transporter

An RP2040 bridge for reading and writing the flash of the **M-VAVE FM-1** (JieLi AC791N / WL82) over its USB lines. You don't need to swap cables or use any vendor tools.

M-VAVE FM-1（JieLi AC791N / WL82）の Flash を USB の線経由で読み書きするための RP2040 ブリッジです。ケーブルの差し替えもメーカーのツールも要りません。

```
Mac ──USB──► XIAO RP2040 ──D+ / D- / GND──► FM-1 (mask ROM UBOOT → loader → SPI flash)
```

## What it does / できること

Each item was verified on hardware.
どれも実機で確認済みです。

- **USB_KEY entry**: gets the FM-1 into the mask-ROM UBOOT at power-on, or after a watchdog reset. No power cycle is needed in the watchdog case.
  電源投入時、またはウォッチドッグのリセット直後に、鍵で ROM の UBOOT に入ります（ウォッチドッグの場合は電源操作不要）。
- **Soft key**: if stock V15 is running, it enters UBOOT through USB-MIDI, without a power cycle.
  純正 V15 が動いていれば、USB-MIDI 経由で電源を入れ直さずに UBOOT に入ります。
- **Info and dump**: reads the chip key and flash ID, and dumps the full 1 MiB of flash in about 3 s.
  チップキーと Flash ID の取得、1MiB 全体の読み出し（約 3 秒）。
- **Guarded writes**: writes only the 4 KiB sectors that differ, reads each one back, and verifies the whole image at the end.
  違う 4KiB 区画だけを書き、区画ごとに読み戻して確認し、最後に全体を照合します。

## Hardware / ハードウェア

**Recommended: Seeed XIAO RP2040** (tested). Three wires, no other parts:
**推奨：Seeed XIAO RP2040**（動作確認済み）。部品なしで 3 本つなぐだけです。

| XIAO RP2040 | FM-1 USB |
|---|---|
| D6 / GP0 | D+ |
| D7 / GP1 | D- |
| GND | GND |

**Do not connect VBUS.** The FM-1 runs from its own battery. 5 V on VBUS would charge it and change how its firmware brings USB up, and the flow above is verified without VBUS.
**VBUS（5V）はつながないでください。** FM-1 はバッテリーで動きます。VBUS をつなぐと充電が始まり、ファームの USB の動き方も変わります。上の手順は VBUS なしで確認しています。

### Compatibility / 互換リスト

| Board | Status |
|---|---|
| Seeed XIAO RP2040 | ✅ Recommended, tested / 推奨・確認済み |
| Other RP2040 boards with GP0/GP1 free | ⚠️ Untested; should work / 未確認（動く見込み） |
| Waveshare RP2350-USB-A | ❌ **Not supported / 非対応** |

Why the Waveshare RP2350-USB-A is not supported:
Waveshare RP2350-USB-A が使えない理由：

- RP2350 rev **A2** has erratum **E9**: a pad with its pull-down enabled can latch at about 2 V. The key and attach detection depend on reading D+/D- reliably.
  RP2350 の A2 版には不具合 E9 があり、プルダウンを有効にした入力が約 2V で張り付きます。鍵の判定や接続の検出に影響します。
- The board has a 1.5 kΩ pull-up (R13) on D+ of its USB-A port. This breaks host-mode attach detection and the ROM-handshake detection.
  USB-A 側の D+ に 1.5kΩ のプルアップ（R13）が付いていて、ホストとしての接続検出と ROM の応答検出が壊れます。
- Its USB-A port supplies VBUS (5 V) with no switch, which the FM-1 must not get. See above.
  USB-A ポートからスイッチなしで VBUS（5V）が出るため、FM-1 に 5V が入ってしまいます（上記）。

## Build / ビルド

Requirements: pico-sdk 2.2.0, arm-none-eabi-gcc, CMake, picotool.
You also need `wl82loader.bin` (24064 B) from kagaimiq's [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) (`data/loaderblobs/usb/`). It is a third-party blob: it is embedded at build time and never committed.

必要なもの：pico-sdk 2.2.0、arm-none-eabi-gcc、CMake、picotool。
ほかに、kagaimiq さんの [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) に入っている `wl82loader.bin`（24064 バイト、`data/loaderblobs/usb/`）が必要です。第三者のバイナリなので、ビルド時に埋め込むだけでリポジトリには入れません。

```bash
git submodule update --init
```
```bash
cmake -S . -B build -DPICO_SDK_PATH=$HOME/pico-sdk -DPICO_BOARD=seeed_xiao_rp2040 -DFM1T_LOADER_BIN=/path/to/wl82loader.bin
```
```bash
make -C build -j8
```
```bash
picotool load -f -x build/fm1_transporter.uf2
```

## Usage / 使い方

`tools/fm1t.py` needs pyserial. 
`tools/fm1t.py` を使います（pyserial が必要）。

1. Connect the FM-1 (3 wires) and plug the XIAO into the Mac.
   FM-1 を 3 本でつなぎ、XIAO を Mac に挿します。
2. Get the FM-1 into UBOOT in one of two ways:
   FM-1 を UBOOT に入れます。方法は 2 つです。
   - If stock V15 is running and was on the bus before the XIAO started, `fm1t` sends the soft key by itself.
     純正 V15 が動いていて、XIAO より先につながっていた場合は、`fm1t` が自動でソフトキーを送ります。
   - Otherwise, switch the FM-1 off, then on while the XIAO is keying. The console says `UBOOT READY`.
     それ以外は、FM-1 の電源を一度切り、XIAO が鍵を送っている間に入れます（コンソールに `UBOOT READY` と出ます）。
3. Run one of the commands below.
   次のコマンドを実行します。

```bash
python3 tools/fm1t.py info
```
```bash
python3 tools/fm1t.py dump backup.bin
```
```bash
python3 tools/fm1t.py write --package FM-1_vNN.fwsc --ref backup.bin
```

`write` is a dry run until you add `--write`. Packages must pass the review in [fm-1-research-lab](https://github.com/kurogedelic/fm-1-research-lab) (`fm1_ota.require_reviewed`). Other commands: `status`, `uboot`, `runapp`, `rekey`.
`write` は `--write` を付けるまで、確認だけして何も書きません。パッケージは fm-1-research-lab の審査（`fm1_ota.require_reviewed`）を通る必要があります。ほかのコマンド：`status`、`uboot`、`runapp`、`rekey`。

## Notes / 注意事項

- **Use at your own risk.** Writing firmware can brick the FM-1. Take a `dump` first and keep it.
  **自己責任で使ってください。** 書き込みで FM-1 が起動しなくなることがあります。書く前に必ず `dump` を取って保管してください。
- The firmware never writes outside `[0x4000, 0x93000)`. The header, SPL and isd_config below `0x4000`, and the device data from `0x93000` up, are never written.
  ファームは `[0x4000, 0x93000)` の外には書き込みません。`0x4000` より前（ヘッダー、SPL、設定）と、`0x93000` 以降（機器データ）は書きません。
- If firmware hangs without a watchdog, the FM-1 needs one power-on while the XIAO is keying.
  ウォッチドッグなしで固まったファームの場合は、XIAO が鍵を送っている間に電源を 1 回入れる必要があります。
- Without VBUS, stock V15 enables USB only after a power-on. After a session, power-cycle the FM-1 to use it over USB again.
  VBUS がないと、純正 V15 は電源を入れたときしか USB を有効にしません。作業のあと USB で使うには、電源を入れ直してください。
- Not affiliated with M-VAVE or JieLi.
  M-VAVE、JieLi とは関係ありません。

## Docs / 資料

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): firmware design / 設計
- [docs/PROTOCOL.md](docs/PROTOCOL.md): JieLi UBOOT / loader protocol / 通信仕様
- [docs/DEVLOG.md](docs/DEVLOG.md): bring-up history and hardware results / 開発の記録

## License / ライセンス

MIT. See [LICENSE](LICENSE).

Third-party components:
第三者のコンポーネント：

- [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) (MIT, submodule; patched at build time)
- [TinyUSB](https://github.com/hathach/tinyusb) (MIT) and [pico-sdk](https://github.com/raspberrypi/pico-sdk) (BSD-3-Clause)
- The CRC and cipher routines are ported from [jl-uboot-tool](https://github.com/kagaimiq/jl-uboot-tool) (MIT, © kagaimiq).
  CRC と暗号化の処理は jl-uboot-tool（MIT、© kagaimiq）から移植しました。
- `wl82loader.bin` is not included.
  `wl82loader.bin` は含まれていません。
