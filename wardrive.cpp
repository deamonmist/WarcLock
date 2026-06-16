/* WarcLock.ino — by Deamonmist (multi-file)
 * wardrive.cpp — WiFi scanning + WiGLE CSV logging implementation
 */
#include "config.h"
#include "wardrive.h"
#include "gpsclock.h"
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>

#define SCAN_INTERVAL_MS  30000UL
#define TMP_FILE          "/session_tmp.csv"

static bool          scanning    = false;
static unsigned long lastScan    = 0;
static char          g_startName[32] = "";   // start timestamp for filename

bool wardriveInitSD() {
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  delay(10);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD init failed — check wiring / FAT32 format");
    return false;
  }
  Serial.println("SD card OK");
  return true;
}

// Format a 12-hour time string like "0223PM" from 24h hour + minute + second
static void fmt12(char *buf, int h24, int m) {
  const char *ampm = (h24 >= 12) ? "PM" : "AM";
  int h12 = h24 % 12;
  if (h12 == 0) h12 = 12;
  snprintf(buf, 8, "%02d%02dM", h12, m);  // placeholder, overwrite below
  snprintf(buf, 8, "%02d%02d%s", h12, m, ampm);
}

// Build a full filename: /MM-DD-YYYY-HHMMam-HHMMpm.csv
// Pass the start stamp string and current time for the end stamp.
static void buildFilename(char *out, size_t sz,
                          const char *startStamp,
                          int h24, int m,
                          int day, int mon, int yr) {
  char endStamp[8];
  fmt12(endStamp, h24, m);
  snprintf(out, sz, "/%02d-%02d-%04d-%s-%s.csv", mon, day, yr, startStamp, endStamp);
}

static void openSessionFile() {
  if (!g_sdOK) return;

  // Capture start time
  int h, m, s, h24;
  getClock(h, m, s, h24);

  // Date from GPS if valid, otherwise sensible default
  int day = gps.date.isValid() ? gps.date.day()   : 1;
  int mon = gps.date.isValid() ? gps.date.month() : 1;
  int yr  = gps.date.isValid() ? gps.date.year()  : 2026;

  fmt12(g_startName, h24, m);

  // If a temp file exists from a previous crash, append to it —
  // the data is still valid. Only write the header for a fresh file.
  bool isNew = !SD.exists(TMP_FILE);
  File f = SD.open(TMP_FILE, FILE_APPEND);
  if (!f) { Serial.println("Could not create session file"); return; }
  if (isNew) {
    f.println("WigleWifi-1.6,appRelease=WarWatch01,model=XIAO-ESP32C5,device=WarWatch,display=GC9A01,board=ESP32C5,brand=Seeed");
    f.println("MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type");
  }
  f.close();

  // Keep g_logFile pointing at the temp path for logResults to append to
  strncpy(g_logFile, TMP_FILE, sizeof(g_logFile));

  Serial.printf("Wardrive session started: %s at %s\n",
                (yr > 2000 ? "dated" : "undated"), g_startName);
}

static void closeSessionFile() {
  if (!g_sdOK || !SD.exists(TMP_FILE)) return;

  // Build the final filename using the end time
  int h, m, s, h24;
  getClock(h, m, s, h24);
  int day = gps.date.isValid() ? gps.date.day()   : 1;
  int mon = gps.date.isValid() ? gps.date.month() : 1;
  int yr  = gps.date.isValid() ? gps.date.year()  : 2026;

  char finalName[48];
  buildFilename(finalName, sizeof(finalName), g_startName, h24, m, day, mon, yr);

  // If a file with this exact name already exists (same-minute start+end),
  // append a short counter to avoid collision
  if (SD.exists(finalName)) {
    char tmp[52];
    snprintf(tmp, sizeof(tmp), "%.*s_2.csv", (int)strlen(finalName)-4, finalName);
    strncpy(finalName, tmp, sizeof(finalName));
  }

  // Copy temp -> final (SD lib has no rename)
  File src = SD.open(TMP_FILE, FILE_READ);
  File dst = SD.open(finalName, FILE_WRITE);
  if (!src || !dst) {
    Serial.println("Session rename failed — temp file kept");
    if (src) src.close();
    if (dst) dst.close();
    return;
  }

  uint8_t buf[512];
  while (src.available()) {
    int n = src.read(buf, sizeof(buf));
    dst.write(buf, n);
  }
  src.close();
  dst.close();
  SD.remove(TMP_FILE);

  strncpy(g_logFile, finalName, sizeof(g_logFile));
  Serial.printf("Session saved as: %s\n", finalName);
}

void wardriveEnter() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  scanning = false;
  lastScan = 0;
  openSessionFile();
  Serial.println("Wardrive mode");
}

void wardriveExit() {
  if (scanning) { WiFi.scanDelete(); scanning = false; }
  closeSessionFile();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("Wardrive session ended");
}

static String authCaps(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:           return "[ESS]";
    case WIFI_AUTH_WEP:            return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:        return "[WPA-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA2_PSK:       return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:   return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
    case WIFI_AUTH_WPA3_PSK:       return "[WPA3-SAE-CCMP][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:  return "[WPA2-PSK-CCMP][WPA3-SAE-CCMP][ESS]";
    default:                       return "[ESS]";
  }
}

static int channelToFreq(int ch) {
  if (ch >= 1 && ch <= 13)    return 2407 + ch * 5;
  if (ch == 14)               return 2484;
  if (ch >= 36 && ch <= 177)  return 5000 + ch * 5;
  return 0;
}

static void logResults(int n) {
  if (n <= 0) return;
  int h, m, s, h24;
  getClock(h, m, s, h24);
  char ts[24];
  snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
    gps.date.isValid() ? gps.date.year()  : 2026,
    gps.date.isValid() ? gps.date.month() : 1,
    gps.date.isValid() ? gps.date.day()   : 1,
    h24, m, s);

  File f = SD.open(g_logFile, FILE_APPEND);
  if (!f) { Serial.println("SD append failed"); return; }
  for (int i = 0; i < n; i++) {
    String bssid = WiFi.BSSIDstr(i);
    String ssid  = WiFi.SSID(i);
    ssid.replace(",", " ");
    String caps  = authCaps(WiFi.encryptionType(i));
    int ch   = WiFi.channel(i);
    int freq = channelToFreq(ch);
    int rssi = WiFi.RSSI(i);
    char line[220];
    snprintf(line, sizeof(line),
      "%s,%s,%s,%s,%d,%d,%d,%.6f,%.6f,%.1f,%.1f,,,WIFI",
      bssid.c_str(), ssid.c_str(), caps.c_str(), ts,
      ch, freq, rssi, g_lat, g_lon, g_alt, g_hdop * 5.0f);
    f.println(line);
  }
  f.close();
  Serial.printf("Logged %d networks\n", n);
}

void wardriveTask() {
  if (!g_gpsLocked || !g_sdOK) return;

  if (!scanning) {
    if (millis() - lastScan > SCAN_INTERVAL_MS) {
      WiFi.scanNetworks(true, true);
      scanning = true;
    }
  } else {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      logResults(n);
      WiFi.scanDelete();
      scanning = false;
      lastScan = millis();
    } else if (n == WIFI_SCAN_FAILED) {
      WiFi.scanDelete();
      scanning = false;
      lastScan = millis();
    }
  }
}