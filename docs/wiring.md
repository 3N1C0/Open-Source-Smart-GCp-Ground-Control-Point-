# Wiring

Pin assignments for the breadboard / jumper-wire build. These are defaults
from [`config.h`](../firmware/SmartGCP/config.h), not a fixed pinout: either wire to match, or edit
the numbers in `config.h`. Numbers are GPIO numbers.

| Signal | XIAO ESP32-C3 / C3 SuperMini | Classic ESP32 DevKit | Connects to |
|---|---|---|---|
| GNSS RX | GPIO20 (D7) | GPIO16 | UM980 **TX** |
| GNSS TX | GPIO21 (D6) | GPIO17 | UM980 **RX** |
| SD SCK | GPIO8 (D8) | GPIO18 | microSD CLK |
| SD MISO | GPIO9 (D9) | GPIO19 | microSD DO |
| SD MOSI | GPIO10 (D10) | GPIO23 | microSD DI |
| SD CS | GPIO4 (D2) | GPIO5 | microSD CS |
| LED R / G / B (stage 2) | GPIO5 / 6 / 7 | GPIO25 / 26 / 27 | via 220–330 Ω each |
| Button (stage 2) | GPIO3 (D1) | GPIO33 | button to GND |

- TX and RX cross over: ESP32 RX goes to UM980 TX.
- UM980 UART is 3.3 V logic. All grounds must be common.
- microSD breakout runs from 3V3. Card formatted FAT32.
- The firmware picks the right column automatically from the chip type.
