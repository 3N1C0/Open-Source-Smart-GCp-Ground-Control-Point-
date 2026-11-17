# SmartGCP firmware (v0.1.0-bench)

Firmware for the ESP32 + UM980 + microSD stack. It logs raw multi-band GNSS
observations to the SD card for PPK, and serves status, control and downloads
over Wi-Fi and Bluetooth LE. LED and button code is included but disabled.

## 1. Check the wiring

Wire the UM980 and microSD to the ESP32 as described in
[docs/wiring.md](../docs/wiring.md), or edit the pin numbers in
[`SmartGCP/config.h`](SmartGCP/config.h) to match your build.

## 2. Flash it (Arduino IDE 2.x)

1. Open `firmware/SmartGCP/SmartGCP.ino`. (Arduino requires the folder name to match the sketch name, so keep the `SmartGCP` folder as it is.)
2. Boards Manager: install **esp32 by Espressif Systems** (tested on 2.0.9 and 3.0.7).
3. Tools menu:
   - Board: **XIAO_ESP32C3** (Seeed XIAO), **ESP32C3 Dev Module** (SuperMini), or **ESP32 Dev Module** (classic)
   - Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)** — required, the default is too small for Wi-Fi + BLE
   - USB CDC On Boot: **Enabled** (C3 Dev Module only)
   - Port: the board's serial port
4. Set `DEVICE_ID`, `WIFI_AP_SSID`, `BLE_NAME` in `config.h` for this unit, and change `WIFI_AP_PASS` from its default.
5. Upload. If it will not connect: hold BOOT, plug in USB, release, upload again.
6. Open Serial Monitor at 115200.

No extra libraries are needed.

## 3. Benchmark

Bench first (antenna near a window is enough), then 30–60 min under open sky.

At power-up you should see:

```
[TEST] SD card mount ............ PASS
[TEST] SD write/read-back ....... PASS (xxx kB/s)
[TEST] UM980 responds ........... PASS (115200 baud)
[TEST]   MODE ROVER SURVEY      OK      (one line per command)
[TEST] UM980 configuration ...... PASS
[TEST] Raw data stream .......... PASS
```

then one line every 5 s:

```
[BENCH] t=125s log=ON file=/logs/GCP-01_S0003.rtcm size=182kB | rx=1432B/s rtcm=11/s crcErr=0 nmeaErr=0 | fix=single sats=31 hdop=0.6 | sdMaxWrite=38ms sdErr=0 uartPeak=12% heap=142k
```

| Number | Good | If not |
|---|---|---|
| `crcErr`, `nmeaErr` | 0 and staying 0 | noisy or long UART wires, bad ground, wrong baud |
| `sdErr` | 0 | card or wiring; lower `SD_SPI_HZ` |
| `uartPeak` | under ~50 % | SD writes too slow; try another card |
| `rx` | roughly 1–3 kB/s, steady | 0 = no data; check TX/RX |
| `rtcm` | about 5–11 /s | 0 = RTCM commands failed (see FAIL lines) |
| `sats` | 20+ under open sky | antenna, cable, sky view |
| `heap` | steady, not falling | report it |

Console commands (type in Serial Monitor): `help`, `status`, `start`, `stop`,
`ls`, `gnss <UM980 command>`, `reboot`.

If a config line reports FAIL, the UM980 may want the port named. Find which
UART your carrier routes to the ESP32 (COM1/COM2/COM3) and change the line in
`config.h`, e.g. `"RTCM1077 COM2 1"`.

File-integrity test: pull power mid-session, then check the `.rtcm` file is
readable and earlier sessions are intact. At most 2 s of data should be lost.

## 4. Wi-Fi and Bluetooth

- Wi-Fi: join `SmartGCP-01` (default password `gcp12345`; change it in `config.h`), open `http://192.168.4.1`.
  Live status, Start/Stop, file list, download, delete.
- BLE: connect to `SmartGCP-01` with nRF Connect or a BLE terminal app. Status
  line every 2 s; send `start`, `stop` or `status`.
- On a XIAO, attach the small external antenna or range will be a metre or two.

## 5. From log file to coordinates (PPK)

Each session produces `GCP-01_Snnnn.rtcm` (raw data) and `.txt` (metadata).
See [docs/ppk-workflow.md](../docs/ppk-workflow.md) for converting and processing them.

## 6. Stage 2: LED and button

Every LED/button line starts with `//UI `. After the benchmark passes, use
Find/Replace to remove `//UI ` (with the trailing space) in `ui.h` and
`SmartGCP.ino`, set `LED_COMMON_ANODE` in `config.h`, and re-upload.
Behaviour is described at the top of `ui.h`.
