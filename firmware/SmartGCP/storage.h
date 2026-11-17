// =============================================================================
//  storage.h - microSD card and logging sessions
//
//  One session = two files in /logs:
//    GCP-01_S0007.rtcm  raw receiver stream (RTCM3 MSM7 + NMEA), byte-for-byte
//    GCP-01_S0007.txt   metadata: unit, firmware, UTC start/stop, statistics
//
//  All SD access goes through sdLock()/sdUnlock() because the logger runs in
//  its own task while the web page and console run in the main loop.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include "config.h"
#include "gnss.h"

struct Session {
  volatile bool active = false;
  uint32_t number = 0;
  char     path[48] = "";
  char     metaPath[48] = "";
  uint32_t startMs = 0;
  uint32_t bytesWritten = 0;
  uint32_t writeErrors = 0;
  uint32_t maxWriteMs = 0;     // slowest single SD write, ms
  bool     utcLogged = false;
  uint32_t frames0 = 0, crcErr0 = 0;   // receiver counters when the session began
  double   sumLat = 0, sumLon = 0, sumAlt = 0;
  uint32_t nPos = 0;
};

Session session;
bool sdOk = false;
uint64_t sdTotalMB = 0, sdFreeMB = 0;
static File logFile;
static uint32_t lastFlushMs = 0;
static SemaphoreHandle_t sdMutex = nullptr;
static Preferences prefs;

static inline void sdLock()   { xSemaphoreTake(sdMutex, portMAX_DELAY); }
static inline void sdUnlock() { xSemaphoreGive(sdMutex); }

void sessionAddPosition(double lat, double lon, double alt) {
  if (!session.active) return;
  session.sumLat += lat; session.sumLon += lon; session.sumAlt += alt; session.nPos++;
}

static void sdRefreshSpace() {            // can take a second or two on a large card
  sdTotalMB = SD.totalBytes() / (1024ULL * 1024ULL);
  sdFreeMB  = (SD.totalBytes() - SD.usedBytes()) / (1024ULL * 1024ULL);
}

static bool storageBegin() {
  if (!sdMutex) sdMutex = xSemaphoreCreateMutex();
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  sdOk = SD.begin(PIN_SD_CS, SPI, SD_SPI_HZ);
  if (!sdOk) return false;
  if (!SD.exists(LOG_DIR)) SD.mkdir(LOG_DIR);
  sdRefreshSpace();
  return true;
}

// Write, read back and delete a small file. Returns write speed in kB/s, or -1.
static float storageSelfTest() {
  if (!sdOk) return -1;
  const char* p = LOG_DIR "/_selftest.bin";
  static uint8_t blk[1024];
  for (size_t i = 0; i < sizeof(blk); i++) blk[i] = (uint8_t)(i * 7 + 3);
  sdLock();
  File f = SD.open(p, FILE_WRITE);
  if (!f) { sdUnlock(); return -1; }
  uint32_t t0 = millis();
  bool ok = true;
  for (int i = 0; i < 64 && ok; i++) ok = (f.write(blk, sizeof(blk)) == sizeof(blk));
  f.close();
  uint32_t dt = millis() - t0;
  if (ok) {
    f = SD.open(p, FILE_READ);
    ok = f && f.size() == 64 * sizeof(blk);
    static uint8_t rd[1024];
    if (ok) ok = (f.read(rd, sizeof(rd)) == sizeof(rd)) && !memcmp(rd, blk, sizeof(blk));
    if (f) f.close();
  }
  SD.remove(p);
  sdUnlock();
  if (!ok) return -1;
  return 64.0f * 1000.0f / (dt ? dt : 1);
}

static void metaAppend(const char* text) {       // caller holds no lock
  sdLock();
  File m = SD.open(session.metaPath, FILE_APPEND);
  if (m) { m.print(text); m.close(); }
  sdUnlock();
}

static bool sessionStart() {
  if (session.active) return true;
  if (!sdOk) return false;

  prefs.begin("gcp", false);
  uint32_t n = prefs.getUInt("session", 0) + 1;
  prefs.putUInt("session", n);
  prefs.end();

  session.number = n;
  snprintf(session.path, sizeof(session.path), LOG_DIR "/%s_S%04lu.rtcm", DEVICE_ID, (unsigned long)n);
  snprintf(session.metaPath, sizeof(session.metaPath), LOG_DIR "/%s_S%04lu.txt", DEVICE_ID, (unsigned long)n);
  session.bytesWritten = 0; session.writeErrors = 0; session.maxWriteMs = 0;
  session.sumLat = session.sumLon = session.sumAlt = 0; session.nPos = 0;
  session.utcLogged = false;
  session.frames0 = gnss.rtcmFrames; session.crcErr0 = gnss.rtcmCrcErr;
  session.startMs = millis();

  char hdr[420];
  snprintf(hdr, sizeof(hdr),
           "device=%s\nfirmware=%s\nsession=%lu\ndata_file=%s\n"
           "format=RTCM3 MSM7 + NMEA (raw UM980 stream)\nreceiver=%s\n",
           DEVICE_ID, FW_VERSION, (unsigned long)n, session.path, gnssVersion);

  sdLock();
  File m = SD.open(session.metaPath, FILE_WRITE);
  if (m) { m.print(hdr); m.close(); }
  logFile = SD.open(session.path, FILE_WRITE);
  bool ok = (bool)logFile;
  lastFlushMs = millis();
  if (ok) session.active = true;
  sdUnlock();
  return ok;
}

// Record the UTC start time as soon as the receiver knows the date.
static void sessionNoteUtc() {
  if (!session.active || session.utcLogged || !gnssHasDate()) return;
  char iso[24], line[80];
  gnssUtcIso(iso, sizeof(iso));
  // The time is read some seconds after the session began; subtract that offset
  // when you need the exact start. RTKLIB only needs it to the nearest hour.
  snprintf(line, sizeof(line), "utc_first_known=%s (at +%lus)\n", iso,
           (unsigned long)((millis() - session.startMs) / 1000));
  metaAppend(line);
  session.utcLogged = true;
}

static void sessionStop() {
  if (!session.active) return;
  sdLock();
  session.active = false;
  logFile.flush();
  logFile.close();
  sdUnlock();

  char iso[24], txt[420];
  gnssUtcIso(iso, sizeof(iso));
  int k = snprintf(txt, sizeof(txt),
                   "utc_stop=%s\nduration_s=%lu\nbytes=%lu\nrtcm_frames=%lu\nrtcm_crc_errors=%lu\n"
                   "sd_write_errors=%lu\nsd_max_write_ms=%lu\n",
                   iso, (unsigned long)((millis() - session.startMs) / 1000),
                   (unsigned long)session.bytesWritten, (unsigned long)(gnss.rtcmFrames - session.frames0),
                   (unsigned long)(gnss.rtcmCrcErr - session.crcErr0), (unsigned long)session.writeErrors,
                   (unsigned long)session.maxWriteMs);
  if (session.nPos > 0 && k > 0 && k < (int)sizeof(txt)) {
    snprintf(txt + k, sizeof(txt) - k,
             "approx_lat=%.8f\napprox_lon=%.8f\napprox_alt_m=%.3f\napprox_samples=%lu\n"
             "note=approx_* is the receiver's own real-time average, NOT the surveyed coordinate. "
             "Post-process the .rtcm file for the GCP position.\n",
             session.sumLat / session.nPos, session.sumLon / session.nPos,
             session.sumAlt / session.nPos, (unsigned long)session.nPos);
  }
  metaAppend(txt);
  sdLock(); sdRefreshSpace(); sdUnlock();
}

// Called from the logging task for every chunk received from the UM980.
static void storageWrite(const uint8_t* data, size_t n) {
  sdLock();
  if (session.active) {
    uint32_t t0 = millis();
    size_t w = logFile.write(data, n);
    if (w != n) session.writeErrors++;
    session.bytesWritten += w;
    if (millis() - lastFlushMs >= LOG_FLUSH_MS) { logFile.flush(); lastFlushMs = millis(); }
    uint32_t dt = millis() - t0;
    if (dt > session.maxWriteMs) session.maxWriteMs = dt;
  }
  sdUnlock();
}

// JSON array of files in /logs, e.g. [{"n":"GCP-01_S0001.rtcm","s":12345}, ...]
static String storageListJson() {
  String out = "[";
  if (!sdOk) return out + "]";
  sdLock();
  File dir = SD.open(LOG_DIR);
  if (dir) {
    bool first = true;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
      if (!f.isDirectory()) {
        const char* name = f.name();
        const char* base = strrchr(name, '/');
        base = base ? base + 1 : name;
        if (!first) out += ",";
        first = false;
        out += "{\"n\":\""; out += base; out += "\",\"s\":"; out += String((uint32_t)f.size()); out += "}";
      }
      f.close();
    }
    dir.close();
  }
  sdUnlock();
  return out + "]";
}
