/* WarcLock.ino — by Deamonmist (multi-file)
 * gpsclock.cpp — GPS parsing and timekeeping implementation
 *
 * Clock time is computed from elapsed millis() since the last GPS sync,
 * so blocking operations can't make the displayed time race to catch up.
 *
 * Time syncs as soon as the GPS reports a valid TIME, even without a
 * position fix — the ATGM336H backup battery keeps time across power
 * cycles, so the clock is correct within seconds of boot. Position lock
 * (the WiFi icon) is tracked separately and needs a real fix.
 */
#include "config.h"
#include "gpsclock.h"
#include <HardwareSerial.h>

TinyGPSPlus           gps;
static HardwareSerial gpsSerial(1);

static uint32_t g_baseSecOfDay = 10UL * 3600 + 10UL * 60;  // default 10:10:00
static uint32_t g_baseMillis   = 0;
static bool     g_timeSynced   = false;

void gpsBegin() {
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
  g_baseMillis = millis();
  Serial.println("GPS UART started");
}

void gpsFeed() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
}

static void syncClock() {
  int h24 = (gps.time.hour() + 24 + UTC_OFFSET) % 24;
  g_baseSecOfDay = (uint32_t)h24 * 3600
                 + (uint32_t)gps.time.minute() * 60
                 + (uint32_t)gps.time.second();
  g_baseMillis   = millis();
  g_timeSynced   = true;
}

void gpsUpdate() {
  // ── Timekeeping: valid time is enough, no position fix required ──────
  if (gps.time.isValid() && gps.time.isUpdated()) {
    static uint32_t lastSync = 0;
    if (!g_timeSynced || (millis() - lastSync > 60000UL)) {
      syncClock();
      lastSync = millis();
    }
  }

  // ── Position lock: needs a real fix ─────────────────────────────────
  if (gps.location.isValid()) {
    g_lat  = gps.location.lat();
    g_lon  = gps.location.lng();
    g_alt  = gps.altitude.isValid() ? gps.altitude.meters() : 0.0f;
    g_hdop = gps.hdop.isValid()     ? gps.hdop.hdop()       : 9.9f;
    if (!g_gpsLocked) {
      g_gpsLocked = true;
      Serial.printf("GPS locked: %.6f, %.6f\n", g_lat, g_lon);
    }
  } else if (g_gpsLocked) {
    g_gpsLocked = false;
    Serial.println("GPS fix lost");
  }
}

void getClock(int &h, int &m, int &s, int &h24) {
  uint32_t elapsed = (millis() - g_baseMillis) / 1000UL;
  uint32_t sod     = (g_baseSecOfDay + elapsed) % 86400UL;
  h24 = sod / 3600;
  m   = (sod % 3600) / 60;
  s   = sod % 60;
  h   = h24 % 12;
}
