// =============================================================================
//  gnss.h - UM980 link: detection, configuration, and live stream inspection
//
//  The receiver's output is one mixed byte stream: binary RTCM3 frames (raw
//  observations) plus ASCII NMEA lines (position/time). Every byte is written
//  to the SD card untouched. This file only *looks* at the stream to produce
//  status and benchmark numbers.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "config.h"

HardwareSerial GnssSerial(1);

struct GnssStats {
  uint32_t bytesRx    = 0;
  uint32_t rtcmFrames = 0;   // frames with a valid CRC
  uint32_t rtcmCrcErr = 0;   // corrupted frames (should stay 0)
  uint32_t nmeaOk     = 0;
  uint32_t nmeaBad    = 0;   // NMEA checksum failures (should stay 0)
  uint32_t uartPeak   = 0;   // highest UART buffer fill seen, bytes
  uint16_t msgType[16]  = {0};        // RTCM message numbers seen
  uint32_t msgCount[16] = {0};
  uint8_t  fixQuality = 0;            // GGA: 0 none, 1 single, 2 DGPS, 4 RTK fix, 5 RTK float
  uint8_t  sats = 0;
  float    hdop = 99.9f;
  double   lat = 0, lon = 0, alt = 0;
  char     utcTime[12] = "";          // hhmmss.ss
  char     utcDate[8]  = "";          // ddmmyy
  uint32_t lastGgaMs = 0;
  uint32_t lastByteMs = 0;
};

GnssStats gnss;
char gnssVersion[160] = "not detected";
uint32_t gnssBaud = 0;
volatile uint32_t gnssEchoUntil = 0;   // console passthrough: print ASCII lines until this time

// Called for every GGA with a valid fix (defined in storage.h; used for the session average)
void sessionAddPosition(double lat, double lon, double alt);

static inline bool gnssHasFix()  { return gnss.fixQuality > 0 && (millis() - gnss.lastGgaMs) < 5000; }
static inline bool gnssHasDate() { return gnss.utcDate[0] != 0 && gnss.utcTime[0] != 0; }
static inline bool gnssAlive()   { return gnss.bytesRx > 0 && (millis() - gnss.lastByteMs) < 5000; }

// "2026-11-17T07:12:05Z" from the last RMC/GGA, or "" if no time yet
static void gnssUtcIso(char* out, size_t n) {
  if (!gnssHasDate()) { out[0] = 0; return; }
  const char* d = gnss.utcDate; const char* t = gnss.utcTime;
  snprintf(out, n, "20%c%c-%c%c-%c%cT%c%c:%c%c:%c%cZ",
           d[4], d[5], d[2], d[3], d[0], d[1], t[0], t[1], t[2], t[3], t[4], t[5]);
}

static const char* gnssFixName(uint8_t q) {
  switch (q) {
    case 0: return "no fix";
    case 1: return "single";
    case 2: return "DGPS";
    case 4: return "RTK fixed";
    case 5: return "RTK float";
    case 6: return "estimated";
    default: return "other";
  }
}

// ---------------------------------------------------------------- NMEA parsing
static double nmeaToDeg(const char* s) {            // ddmm.mmmm -> degrees
  double v = atof(s);
  double deg = floor(v / 100.0);
  return deg + (v - deg * 100.0) / 60.0;
}

// Split on commas in place, keeping empty fields. Returns field count.
static int nmeaSplit(char* s, char* f[], int maxF) {
  int n = 0;
  f[n++] = s;
  for (char* p = s; *p && n < maxF; p++) {
    if (*p == ',') { *p = 0; f[n++] = p + 1; }
  }
  return n;
}

static void gnssHandleLine(char* line) {
  if (millis() < gnssEchoUntil) Serial.println(line);
  if (line[0] != '$') return;                       // '#' lines are Unicore ASCII logs

  char* star = strrchr(line, '*');
  if (!star || strlen(star) < 3) return;
  uint8_t sum = 0;
  for (char* p = line + 1; p < star; p++) sum ^= (uint8_t)*p;
  if (sum != (uint8_t)strtoul(star + 1, nullptr, 16)) { gnss.nmeaBad++; return; }
  gnss.nmeaOk++;
  *star = 0;

  if (strlen(line) < 6) return;
  const char* type = line + 3;                      // skip "$GN" / "$GP"
  char* f[20];
  if (!strncmp(type, "GGA", 3)) {
    int n = nmeaSplit(line, f, 20);
    if (n < 10) return;
    strlcpy(gnss.utcTime, f[1], sizeof(gnss.utcTime));
    gnss.fixQuality = (uint8_t)atoi(f[6]);
    gnss.sats = (uint8_t)atoi(f[7]);
    gnss.hdop = f[8][0] ? atof(f[8]) : 99.9f;
    gnss.lastGgaMs = millis();
    if (gnss.fixQuality > 0 && f[2][0] && f[4][0]) {
      double la = nmeaToDeg(f[2]); if (f[3][0] == 'S') la = -la;
      double lo = nmeaToDeg(f[4]); if (f[5][0] == 'W') lo = -lo;
      gnss.lat = la; gnss.lon = lo; gnss.alt = atof(f[9]);
      sessionAddPosition(la, lo, gnss.alt);
    }
  } else if (!strncmp(type, "RMC", 3)) {
    int n = nmeaSplit(line, f, 20);
    if (n < 10) return;
    if (strlen(f[9]) == 6) strlcpy(gnss.utcDate, f[9], sizeof(gnss.utcDate));
  }
}

// ---------------------------------------------------------------- RTCM3 framing
static uint32_t crc24q(const uint8_t* d, size_t n) {
  uint32_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint32_t)d[i] << 16;
    for (int b = 0; b < 8; b++) { crc <<= 1; if (crc & 0x1000000UL) crc ^= 0x1864CFBUL; }
  }
  return crc & 0xFFFFFFUL;
}

static void rtcmCount(uint16_t type) {
  for (int i = 0; i < 16; i++) {
    if (gnss.msgType[i] == type) { gnss.msgCount[i]++; return; }
    if (gnss.msgType[i] == 0)    { gnss.msgType[i] = type; gnss.msgCount[i] = 1; return; }
  }
}

// Feed one received byte to the stream inspectors.
static void gnssInspect(uint8_t b) {
  static uint8_t  rb[1100];          // one RTCM frame: 3 header + up to 1023 payload + 3 CRC
  static uint16_t rlen = 0, rneed = 0;
  static char     line[128];
  static uint8_t  ll = 0;
  static bool     inLine = false;

  if (rlen > 0) {                                    // inside a (possible) RTCM frame
    rb[rlen++] = b;
    if (rlen == 2 && (b & 0xFC)) { rlen = 0; }       // 6 reserved bits must be zero
    else if (rlen == 3) { rneed = (((uint16_t)rb[1] & 0x03) << 8 | rb[2]) + 6; }
    else if (rlen > 3 && rlen == rneed) {
      uint32_t want = ((uint32_t)rb[rlen - 3] << 16) | ((uint32_t)rb[rlen - 2] << 8) | rb[rlen - 1];
      if (crc24q(rb, rlen - 3) == want) {
        gnss.rtcmFrames++;
        if (rlen >= 8) rtcmCount(((uint16_t)rb[3] << 4) | (rb[4] >> 4));
      } else {
        gnss.rtcmCrcErr++;
      }
      rlen = 0;
    }
    return;
  }
  if (b == 0xD3) { rb[0] = b; rlen = 1; inLine = false; return; }   // RTCM preamble

  if (b == '$' || b == '#') { ll = 0; line[ll++] = (char)b; inLine = true; }
  else if (inLine) {
    if (b == '\r' || b == '\n') { line[ll] = 0; inLine = false; gnssHandleLine(line); }
    else if (b < 0x20 || b > 0x7E || ll >= sizeof(line) - 1) inLine = false;
    else line[ll++] = (char)b;
  }
}

// --------------------------------------------- setup-time command/response I/O
// Only used before the logging task starts (it reads the UART directly).

static void gnssSend(const char* cmd) {
  GnssSerial.print(cmd);
  GnssSerial.print("\r\n");
}

// Wait for an ASCII line starting with `prefix`. Copies it to `out` if given.
static bool gnssWaitLine(const char* prefix, uint32_t timeoutMs, char* out, size_t outLen) {
  char line[200]; size_t ll = 0; bool inLine = false;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (!GnssSerial.available()) { delay(2); continue; }
    uint8_t b = GnssSerial.read();
    if (b == '$' || b == '#') { ll = 0; line[ll++] = (char)b; inLine = true; }
    else if (inLine) {
      if (b == '\r' || b == '\n') {
        line[ll] = 0; inLine = false;
        if (!strncasecmp(line, prefix, strlen(prefix))) {
          if (out) strlcpy(out, line, outLen);
          return true;
        }
      } else if (b < 0x20 || b > 0x7E || ll >= sizeof(line) - 1) inLine = false;
      else line[ll++] = (char)b;
    }
  }
  return false;
}

// Send a command and check the UM980's "$command,<cmd>,response: OK*xx" reply.
static bool gnssCommand(const char* cmd, uint32_t timeoutMs = 1500) {
  char resp[200];
  gnssSend(cmd);
  if (!gnssWaitLine("$command", timeoutMs, resp, sizeof(resp))) return false;
  return strstr(resp, "response: OK") != nullptr;
}

// Find the receiver: try the configured baud first, then other common rates.
static bool gnssDetect() {
  const uint32_t bauds[] = { GNSS_BAUD, 460800, 230400, 921600, 9600 };
  for (uint8_t i = 0; i < sizeof(bauds) / sizeof(bauds[0]); i++) {
    if (i > 0 && bauds[i] == GNSS_BAUD) continue;
    GnssSerial.end();
    GnssSerial.setRxBufferSize(GNSS_RX_BUFFER);
    GnssSerial.begin(bauds[i], SERIAL_8N1, PIN_GNSS_RX, PIN_GNSS_TX);
    delay(100);
    gnssSend("UNLOG");                               // silence this port
    delay(300);
    while (GnssSerial.available()) GnssSerial.read();
    gnssSend("VERSIONA");
    if (gnssWaitLine("#VERSION", 1500, gnssVersion, sizeof(gnssVersion))) {
      gnssBaud = bauds[i];
      return true;
    }
  }
  // Leave the port open at the configured baud so data still flows if the
  // receiver is alive but did not answer (e.g. only UM980 TX is connected).
  GnssSerial.end();
  GnssSerial.setRxBufferSize(GNSS_RX_BUFFER);
  GnssSerial.begin(GNSS_BAUD, SERIAL_8N1, PIN_GNSS_RX, PIN_GNSS_TX);
  gnssBaud = GNSS_BAUD;
  return false;
}

// Apply GNSS_CONFIG_CMDS. Returns the number of commands that failed.
static int gnssConfigure(bool verbose) {
  int failed = 0;
  gnssCommand("UNLOG");
  for (size_t i = 0; i < GNSS_CONFIG_COUNT; i++) {
    bool ok = gnssCommand(GNSS_CONFIG_CMDS[i]);
    if (!ok) failed++;
    if (verbose) Serial.printf("[TEST]   %-22s %s\n", GNSS_CONFIG_CMDS[i], ok ? "OK" : "FAIL");
  }
  return failed;
}
