// =============================================================================
//  DIY Smart GCP V1.1 - firmware configuration
//  Everything a builder normally needs to change is in this one file.
// =============================================================================
#pragma once
#include <Arduino.h>

#define FW_VERSION   "0.1.0-bench"

// ---- Unit identity (change per unit: GCP-01 .. GCP-04) ----------------------
#define DEVICE_ID    "GCP-01"            // matches the QR/label on the target
#define WIFI_AP_SSID "SmartGCP-01"       // Wi-Fi network the unit broadcasts
#define WIFI_AP_PASS "gcp12345"          // min 8 characters ("" = open network)
#define BLE_NAME     "SmartGCP-01"       // Bluetooth LE advertised name

// ---- Feature switches --------------------------------------------------------
#define ENABLE_WIFI          1   // Wi-Fi access point + web page at http://192.168.4.1
#define ENABLE_BLE           1   // BLE "Nordic UART" service: status + start/stop
#define AUTO_START_LOGGING   1   // start a session at power-up (no button needed)
#define BENCH_REPORT_MS      5000// benchmark line on USB serial every N ms (0 = off)

// ---- Pin map -----------------------------------------------------------------
// !!! CHECK THESE AGAINST YOUR ACTUAL WIRING BEFORE FLASHING !!!
// Numbers are GPIO numbers, not the "D" labels printed on the board.
#if CONFIG_IDF_TARGET_ESP32C3
  // Seeed XIAO ESP32-C3 (label in brackets). Also valid for ESP32-C3 SuperMini.
  #define PIN_GNSS_RX   20   // [D7]  ESP32 RX  <- UM980 TX
  #define PIN_GNSS_TX   21   // [D6]  ESP32 TX  -> UM980 RX
  #define PIN_SD_SCK     8   // [D8]  microSD CLK
  #define PIN_SD_MISO    9   // [D9]  microSD DO  (MISO)
  #define PIN_SD_MOSI   10   // [D10] microSD DI  (MOSI)
  #define PIN_SD_CS      4   // [D2]  microSD CS
  // Stage 2 (LED + button) - not used by the benchmark build
  #define PIN_LED_R      5   // [D3]
  #define PIN_LED_G      6   // [D4]
  #define PIN_LED_B      7   // [D5]
  #define PIN_BUTTON     3   // [D1]  momentary button to GND (must be GPIO0-5 for wake)
#else
  // Classic ESP32 DevKit (ESP32-WROOM-32, the common micro-USB board)
  #define PIN_GNSS_RX   16   // ESP32 RX2 <- UM980 TX
  #define PIN_GNSS_TX   17   // ESP32 TX2 -> UM980 RX
  #define PIN_SD_SCK    18
  #define PIN_SD_MISO   19
  #define PIN_SD_MOSI   23
  #define PIN_SD_CS      5
  #define PIN_LED_R     25
  #define PIN_LED_G     26
  #define PIN_LED_B     27
  #define PIN_BUTTON    33   // RTC-capable pin, momentary button to GND
#endif

#define LED_COMMON_ANODE   0   // 1 if the LED's long pin goes to 3V3, 0 if it goes to GND
#define BUTTON_LONG_MS  3000   // hold this long to stop logging and sleep

// ---- microSD -----------------------------------------------------------------
#define SD_SPI_HZ        4000000UL  // 4 MHz is safe on jumper wires; 20 MHz on a PCB
#define LOG_DIR          "/logs"
#define LOG_FLUSH_MS     2000       // power loss costs at most this much data

// ---- GNSS receiver (Unicore UM980) ------------------------------------------
#define GNSS_BAUD        115200     // UM980 factory default
#define GNSS_RX_BUFFER   16384      // UART buffer; rides through slow SD writes

// Commands sent to the UM980 at every boot (nothing is saved in the receiver,
// so a power cycle always returns to this known configuration).
// A number after the message name is the output period in seconds.
// With no COM port named, the UM980 applies the command to the port it arrived on.
// If a line reports FAIL in the self-test, name the port explicitly, e.g.
// "RTCM1077 COM2 1" (use whichever UM980 UART your carrier wires to the ESP32).
static const char* const GNSS_CONFIG_CMDS[] = {
  "MODE ROVER SURVEY",      // static-occupation tracking mode
  // "CONFIG SIGNALGROUP 2", // optional: all bands incl. Galileo E6 (receiver restarts)

  // Position/time for status, file timestamps and the web page
  "GNGGA 1",
  "GNRMC 1",

  // Raw observations for PPK: RTCM3 MSM7 (full code/carrier/doppler/SNR)
  "RTCM1077 1",             // GPS
  "RTCM1087 1",             // GLONASS
  "RTCM1097 1",             // Galileo
  "RTCM1117 1",             // QZSS
  "RTCM1127 1",             // BeiDou

  // Broadcast ephemerides (lets RTKLIB process without a separate nav file)
  "RTCM1019 30",            // GPS
  "RTCM1020 30",            // GLONASS
  "RTCM1042 30",            // BeiDou
  "RTCM1044 30",            // QZSS
  "RTCM1045 30",            // Galileo F/NAV
  "RTCM1046 30",            // Galileo I/NAV

  // Alternative: Unicore native binary instead of RTCM (convert with Unicore's
  // "Converter" tool). Comment out the RTCM lines above and enable these:
  // "OBSVMB 1", "GPSEPHB 120", "GLOEPHB 120", "GALEPHB 120", "BDSEPHB 120", "BD3EPHB 120",
};
#define GNSS_CONFIG_COUNT (sizeof(GNSS_CONFIG_CMDS) / sizeof(GNSS_CONFIG_CMDS[0]))
