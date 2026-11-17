// =============================================================================
//  DIY Smart GCP V1.1 - benchmark firmware
//
//  Hardware in this build:  ESP32  +  UM980 GNSS (UART)  +  microSD (SPI)
//  LED and button code is written but disabled - see ui.h ("//UI" lines).
//
//  What it does:
//    1. Power-on self-test of the SD card and the UM980 (results on USB serial)
//    2. Configures the UM980 to output raw multi-band observations (RTCM3 MSM7)
//    3. Logs that stream to the SD card, one file per session, for PPK
//    4. Serves status/control/downloads over Wi-Fi (http://192.168.4.1) and BLE
//    5. Prints a benchmark line every 5 s so you can judge the data path
//
//  Tabs:  config.h   pins, names, GNSS messages  <-- edit this one
//         gnss.h     UM980 commands + stream checking
//         storage.h  SD card + sessions
//         webui.h    Wi-Fi access point + web page
//         ble_link.h Bluetooth LE link
//         ui.h       LED + button (stage 2, disabled)
//
//  USB serial console (115200): type  help
// =============================================================================
#include "config.h"
#include "gnss.h"
#include "storage.h"
#include "webui.h"
#include "ble_link.h"
#include "ui.h"

volatile bool reqStart = false, reqStop = false;   // set by web / BLE / console / button
static bool gnssDetected = false;
static uint32_t rateBytes = 0, rateFrames = 0;     // per-second rates, updated each report
static uint32_t lastBytes = 0, lastFrames = 0, lastRateMs = 0;

// ------------------------------------------------------------------ logger task
// Runs above the main loop's priority so a slow web download or console command
// can never make the UART buffer overflow.
static void gnssTask(void*) {
  static uint8_t buf[2048];
  for (;;) {
    int avail = GnssSerial.available();
    if (avail > 0) {
      if ((uint32_t)avail > gnss.uartPeak) gnss.uartPeak = avail;
      if (avail > (int)sizeof(buf)) avail = sizeof(buf);
      int n = GnssSerial.readBytes(buf, avail);
      if (n > 0) {
        gnss.bytesRx += n;
        gnss.lastByteMs = millis();
        for (int i = 0; i < n; i++) gnssInspect(buf[i]);
        storageWrite(buf, n);                      // no-op unless a session is active
      }
      vTaskDelay(pdMS_TO_TICKS(2));
    } else {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// ---------------------------------------------------------------------- status
static void updateRates() {
  uint32_t now = millis();
  uint32_t dt = now - lastRateMs;
  if (dt < 1000) return;
  uint32_t b = gnss.bytesRx, f = gnss.rtcmFrames;
  rateBytes  = (uint32_t)((uint64_t)(b - lastBytes) * 1000 / dt);
  rateFrames = (uint32_t)((uint64_t)(f - lastFrames) * 1000 / dt);
  lastBytes = b; lastFrames = f; lastRateMs = now;
}

String statusLine() {
  char s[160];
  snprintf(s, sizeof(s), "%s log=%s S%04lu %lukB fix=%s sats=%u hdop=%.1f rx=%luB/s err=%lu",
           DEVICE_ID, session.active ? "ON" : "OFF", (unsigned long)session.number,
           (unsigned long)(session.bytesWritten / 1024),
           gnssAlive() ? gnssFixName(gnssHasFix() ? gnss.fixQuality : 0) : "NO-DATA",
           gnss.sats, gnss.hdop, (unsigned long)rateBytes,
           (unsigned long)(gnss.rtcmCrcErr + session.writeErrors));
  return String(s);
}

String statusJson() {
  char iso[24];
  gnssUtcIso(iso, sizeof(iso));
  const char* base = strrchr(session.path, '/');
  base = base ? base + 1 : session.path;
  String rx = gnssVersion;                      // keep the JSON valid whatever the receiver says
  rx.replace("\"", "'"); rx.replace("\\", "/");
  if (rx.length() > 70) rx = rx.substring(0, 70);
  char s[640];
  snprintf(s, sizeof(s),
           "{\"id\":\"%s\",\"fw\":\"%s\",\"uptime\":%lu,\"logging\":%s,\"file\":\"%s\",\"bytes\":%lu,"
           "\"gnss\":%s,\"fix\":%u,\"fixName\":\"%s\",\"sats\":%u,\"hdop\":%.2f,"
           "\"lat\":%.8f,\"lon\":%.8f,\"alt\":%.3f,\"utc\":\"%s\",\"bps\":%lu,\"fps\":%lu,"
           "\"crcErr\":%lu,\"sdErr\":%lu,\"sd\":%s,\"sdFreeMB\":%lu,\"heap\":%lu,\"rx\":\"%s\"}",
           DEVICE_ID, FW_VERSION, (unsigned long)(millis() / 1000),
           session.active ? "true" : "false", base, (unsigned long)session.bytesWritten,
           gnssAlive() ? "true" : "false", gnssHasFix() ? gnss.fixQuality : 0,
           gnssFixName(gnssHasFix() ? gnss.fixQuality : 0), gnss.sats, gnss.hdop,
           gnss.lat, gnss.lon, gnss.alt, iso, (unsigned long)rateBytes, (unsigned long)rateFrames,
           (unsigned long)gnss.rtcmCrcErr, (unsigned long)session.writeErrors,
           sdOk ? "true" : "false", (unsigned long)sdFreeMB, (unsigned long)ESP.getFreeHeap(),
           rx.c_str());
  return String(s);
}

static void benchReport() {
  Serial.printf("[BENCH] t=%lus log=%s file=%s size=%lukB | rx=%luB/s rtcm=%lu/s crcErr=%lu nmeaErr=%lu | "
                "fix=%s sats=%u hdop=%.1f | sdMaxWrite=%lums sdErr=%lu uartPeak=%lu%% heap=%luk\n",
                (unsigned long)(millis() / 1000), session.active ? "ON" : "OFF",
                session.active ? session.path : "-", (unsigned long)(session.bytesWritten / 1024),
                (unsigned long)rateBytes, (unsigned long)rateFrames,
                (unsigned long)gnss.rtcmCrcErr, (unsigned long)gnss.nmeaBad,
                gnssAlive() ? gnssFixName(gnssHasFix() ? gnss.fixQuality : 0) : "NO-DATA",
                gnss.sats, gnss.hdop, (unsigned long)session.maxWriteMs,
                (unsigned long)session.writeErrors,
                (unsigned long)(gnss.uartPeak * 100 / GNSS_RX_BUFFER),
                (unsigned long)(ESP.getFreeHeap() / 1024));
}

static void printMessageCounts() {
  Serial.print("RTCM messages seen:");
  bool any = false;
  for (int i = 0; i < 16 && gnss.msgType[i]; i++) {
    Serial.printf(" %u x%lu", gnss.msgType[i], (unsigned long)gnss.msgCount[i]);
    any = true;
  }
  Serial.println(any ? "" : " none");
}

// --------------------------------------------------------------------- console
static void consoleHelp() {
  Serial.println(F(
    "Commands:\n"
    "  status        one-line status + RTCM message counts\n"
    "  start / stop  start or stop a logging session\n"
    "  ls            list files on the SD card\n"
    "  gnss <text>   send a raw command to the UM980 and show its replies for 3 s\n"
    "                e.g.  gnss VERSIONA   |   gnss CONFIG   |   gnss GNGGA 1\n"
    "  reboot        restart the ESP32"));
}

static void consoleExec(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;
  String lc = cmd; lc.toLowerCase();
  if (lc == "help" || lc == "?") consoleHelp();
  else if (lc == "status") { Serial.println(statusLine()); printMessageCounts(); }
  else if (lc == "start") reqStart = true;
  else if (lc == "stop") reqStop = true;
  else if (lc == "ls") Serial.println(storageListJson());
  else if (lc == "reboot") { sessionStop(); delay(100); ESP.restart(); }
  else if (lc.startsWith("gnss ")) {
    gnssEchoUntil = millis() + 3000;
    gnssSend(cmd.substring(5).c_str());
  }
  else Serial.println("Unknown command - type help");
}

static void consolePoll() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') { consoleExec(line); line = ""; }
    else if (line.length() < 120) line += c;
  }
}

// ----------------------------------------------------------------------- setup
static void selfTest() {
  Serial.println(F("\n[TEST] ---- power-on self-test ----"));

  // 1. microSD
  bool sd = storageBegin();
  Serial.printf("[TEST] SD card mount ............ %s\n", sd ? "PASS" : "FAIL");
  if (sd) {
    Serial.printf("[TEST]   size %lu MB, free %lu MB\n", (unsigned long)sdTotalMB, (unsigned long)sdFreeMB);
    float kbps = storageSelfTest();
    Serial.printf("[TEST] SD write/read-back ....... %s", kbps > 0 ? "PASS" : "FAIL");
    if (kbps > 0) Serial.printf(" (%.0f kB/s)", kbps);
    Serial.println();
    if (kbps <= 0) sdOk = false;
  } else {
    Serial.println(F("[TEST]   check CS/SCK/MOSI/MISO pins in config.h, 3V3 + GND, card is FAT32"));
  }

  // 2. UM980
  gnssDetected = gnssDetect();
  Serial.printf("[TEST] UM980 responds ........... %s", gnssDetected ? "PASS" : "FAIL");
  if (gnssDetected) Serial.printf(" (%lu baud)", (unsigned long)gnssBaud);
  Serial.println();
  if (gnssDetected) {
    Serial.printf("[TEST]   %s\n", gnssVersion);
    int failed = gnssConfigure(true);
    Serial.printf("[TEST] UM980 configuration ...... %s\n", failed == 0 ? "PASS" : "CHECK (see FAIL lines)");
  } else {
    Serial.println(F("[TEST]   no reply: swap the TX/RX wires, check the UM980 has power and a common GND"));
  }
}

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT && defined(ARDUINO_USB_MODE) && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);          // never stall on prints when no computer is attached
#endif
  delay(2500);                       // time to open the serial monitor
  Serial.printf("\n=== DIY Smart GCP %s  firmware %s ===\n", DEVICE_ID, FW_VERSION);

  //UI uiBegin();

  selfTest();

  xTaskCreate(gnssTask, "gnss", 8192, nullptr, 3, nullptr);

  // 3. Is data actually flowing?
  delay(4000);
  bool flow = gnss.rtcmFrames > 0 && gnss.rtcmCrcErr == 0;
  Serial.printf("[TEST] Raw data stream .......... %s (%lu bytes, %lu RTCM frames, %lu CRC errors, %lu NMEA)\n",
                flow ? "PASS" : "FAIL", (unsigned long)gnss.bytesRx, (unsigned long)gnss.rtcmFrames,
                (unsigned long)gnss.rtcmCrcErr, (unsigned long)gnss.nmeaOk);

  bleBegin();
  webBegin();
#if ENABLE_WIFI
  Serial.printf("[INFO] Wi-Fi \"%s\"  ->  http://192.168.4.1\n", WIFI_AP_SSID);
#endif
#if ENABLE_BLE
  Serial.printf("[INFO] BLE  \"%s\"\n", BLE_NAME);
#endif
  Serial.printf("[TEST] ---- result: SD %s | UM980 %s | stream %s ----\n\n",
                sdOk ? "ok" : "FAIL", gnssDetected ? "ok" : "FAIL", flow ? "ok" : "FAIL");

  if (AUTO_START_LOGGING) reqStart = true;
  lastRateMs = millis();
}

// ------------------------------------------------------------------------ loop
void loop() {
  static uint32_t lastBench = 0;

  consolePoll();
  webLoop();
  bleLoop();
  //UI uiLoop();

  if (reqStop)  { reqStop = false;  sessionStop();  Serial.println("[LOG] stopped"); }
  if (reqStart) {
    reqStart = false;
    if (sessionStart()) Serial.printf("[LOG] recording to %s\n", session.path);
    else                Serial.println("[LOG] could not start - SD card problem");
  }
  sessionNoteUtc();
  updateRates();

  if (BENCH_REPORT_MS && millis() - lastBench >= BENCH_REPORT_MS) {
    lastBench = millis();
    benchReport();
  }
  delay(2);
}
