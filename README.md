# SmartGCP

An open-source smart ground control point (GCP) for drone mapping and survey work.

SmartGCP is a survey target with a multi-band GNSS receiver built in. Place it
on the ground, let it log, and post-process the recording to get the target's
position, with centimetre-level accuracy as the design goal. Every session is
written to a microSD card, and the unit can be checked and controlled from a
phone over Wi-Fi or Bluetooth.

> **Project status: early development.** The firmware runs on a breadboard
> build and is being bench-tested. The enclosure CAD exists but is not yet
> published here, and the PCB has not been started. Accuracy has not yet been
> independently verified. See [Status](#status).

## Features

- **Multi-band GNSS**: Unicore UM980 receiver logging raw observations
  (RTCM3 MSM7) for GPS, GLONASS, Galileo, BeiDou and QZSS
- **Post-processed positioning (PPK)**: logs convert to RINEX and process
  against a nearby reference station with free tools such as RTKLIB
- **Local logging**: one file per session on microSD, flushed every 2 seconds
  so a power loss costs very little data
- **Wi-Fi**: the unit hosts its own access point and web page for live status,
  start/stop, and file download
- **Bluetooth LE**: status and start/stop from any BLE terminal app
- **Power-on self-test**: checks the SD card and the receiver at every boot
- **No external libraries**: builds with the stock ESP32 Arduino core

## Repository layout

| Directory | Contents | State |
|---|---|---|
| [`firmware/`](firmware/) | ESP32 Arduino firmware and build guide | Working, benchmark stage |
| [`cad/`](cad/) | Enclosure and target mechanical design | Model exists, files not yet added |
| [`pcb/`](pcb/) | Schematic and board layout | Not started |
| [`docs/`](docs/) | Wiring and PPK processing guides | In progress |

## Hardware

The current build is hand-wired from off-the-shelf modules:

| Part | Notes |
|---|---|
| ESP32 board | Seeed XIAO ESP32-C3, ESP32-C3 SuperMini, or a classic ESP32 DevKit |
| Unicore UM980 GNSS module | On a carrier board with a 3.3 V UART |
| Multi-band GNSS antenna | Must match the bands you want to log |
| microSD breakout (SPI) | Card formatted FAT32 |

Pin assignments are in [docs/wiring.md](docs/wiring.md).

## Quick start

1. Wire the modules as shown in [docs/wiring.md](docs/wiring.md).
2. Open `firmware/SmartGCP/SmartGCP.ino` in Arduino IDE 2.x and flash it,
   following [firmware/README.md](firmware/README.md).
3. Set a unit name and **change the default Wi-Fi password** in
   `firmware/SmartGCP/config.h`.
4. Power the unit under open sky. Logging starts automatically.
5. Join the unit's Wi-Fi network and open `http://192.168.4.1` to watch status
   and download the session.
6. Turn the log into coordinates with [docs/ppk-workflow.md](docs/ppk-workflow.md).

## Status

- [x] Raw GNSS logging to microSD
- [x] Wi-Fi web interface and BLE link
- [x] Power-on self-test and benchmark reporting
- [ ] Bench and open-sky benchmark completed
- [ ] Status LED and button (written, currently disabled)
- [ ] Accuracy validated against surveyed points
- [ ] CAD files published
- [ ] PCB schematic and layout

## Contributing

Contributions, bug reports and field-test results are welcome. Please read
[CONTRIBUTING.md](CONTRIBUTING.md) first. This project follows a
[Code of Conduct](CODE_OF_CONDUCT.md).

## License

- **Firmware and software** (`firmware/`): [MIT License](LICENSE)
- **Hardware** (`cad/`, `pcb/`): [CERN Open Hardware Licence Version 2 - Permissive](https://ohwr.org/cern_ohl_p_v2.txt) (CERN-OHL-P-2.0)
- **Documentation**: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

## Disclaimer

SmartGCP is experimental hardware and software provided as is, without
warranty. Verify results independently before relying on them for any survey,
legal or safety-related purpose.
