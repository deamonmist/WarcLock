/*
 * WarcLock.ino — by Deamonmist (multi-file)
 * XIAO ESP32-C5 + GC9A01 round TFT + ATGM336H GPS + V474 microSD
 *
 * Files:
 *   WarcLock.ino  — globals, setup(), loop(), battery, mode controller
 *   config.h        — pins, palette, geometry, shared globals
 *   display.h/.cpp  — canvas framebuffer + full-frame rendering
 *   gpsclock.h/.cpp — GPS parsing + drift-free timekeeping
 *   wardrive.h/.cpp — async WiFi scan + WiGLE CSV logging
 *   messages.h/.cpp — scrolling message phrases
 *
 * Tools -> PSRAM must be ENABLED (canvas framebuffer lives in PSRAM).
 *
 * Slide switch on D5/GPIO24:
 *   LOW  -> WARDRIVE: scan + log to timestamped CSV
 *   HIGH -> IDLE: radio off, clock + display only
 *
 * Dim button on D4/GPIO23: press to toggle black screen, auto-dims after
 * DIM_TIMEOUT ms of inactivity.
 */

#include "config.h"
#include "display.h"
#include "gpsclock.h"
#include "wardrive.h"

// ── Global storage (declared extern in config.h) ──────────────────────
Arduino_GFX  *gfx          = nullptr;
volatile bool g_gpsLocked  = false;
volatile bool g_wardriveOn = false;
float g_lat = 0.0f, g_lon = 0.0f, g_alt = 0.0f, g_hdop = 99.9f;
bool  g_sdOK = false;
char  g_logFile[32] = "";
int   g_batPct = 0;
bool  g_charging = false;
bool  g_dimmed   = false;

// ── Battery ───────────────────────────────────────────────────────────
static int batteryPercent(float v) {
  struct { float v; int p; } curve[] = {
    {4.20f,100}, {4.10f,90}, {4.00f,80}, {3.90f,65}, {3.80f,50},
    {3.70f,35},  {3.60f,20}, {3.50f,12}, {3.40f,6},  {3.30f,3}, {3.00f,0}
  };
  const int n = sizeof(curve) / sizeof(curve[0]);
  if (v >= curve[0].v)   return 100;
  if (v <= curve[n-1].v) return 0;
  for (int i = 0; i < n - 1; i++) {
    if (v <= curve[i].v && v > curve[i+1].v) {
      float frac = (v - curve[i+1].v) / (curve[i].v - curve[i+1].v);
      return (int)(curve[i+1].p + frac * (curve[i].p - curve[i+1].p) + 0.5f);
    }
  }
  return 0;
}

static void updateBattery() {
  digitalWrite(BAT_VOLT_PIN_EN, HIGH);
  delay(5);
  long raw = 0;
  for (int i = 0; i < 16; i++) { raw += analogRead(BAT_VOLT_PIN); delay(2); }
  raw /= 16;
  digitalWrite(BAT_VOLT_PIN_EN, LOW);
  float voltage = raw * (3.3f / 4095.0f) * 2.0f * BAT_CALIBRATION;
  g_charging = (voltage > CHARGE_THRESHOLD);
  g_batPct   = batteryPercent(voltage);
  Serial.printf("Battery: %.2fV  (%d%%)  %s\n",
                voltage, g_batPct, g_charging ? "CHARGING" : "on battery");
}

// ── Setup ─────────────────────────────────────────────────────────────
void setup() {
  setCpuFrequencyMhz(80);
  Serial.begin(115200);
  delay(500);
  Serial.println("\n== ZenDeamon WarWatch ==");
  Serial.printf("CPU @ %d MHz\n", getCpuFrequencyMhz());

  if (!displayBegin()) {
    Serial.println("ERROR: canvas/display init failed.");
    Serial.println("Check that Tools -> PSRAM is ENABLED.");
    while (1) delay(100);
  }
  Serial.println("Display OK");

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(BAT_VOLT_PIN_EN, OUTPUT);
  digitalWrite(BAT_VOLT_PIN_EN, LOW);
  updateBattery();

  renderFrame();

  g_sdOK = wardriveInitSD();
  gpsBegin();

  pinMode(WARDRIVE_SW, INPUT_PULLUP);
  pinMode(DIM_BTN,     INPUT_PULLUP);

  Serial.println("Setup complete");
}

// ── Loop ──────────────────────────────────────────────────────────────
void loop() {
  gpsFeed();
  gpsUpdate();

  // Wardrive switch
  bool sw = (digitalRead(WARDRIVE_SW) == LOW);
  static bool prevSw   = false;
  static bool firstRun = true;
  if (firstRun) { prevSw = !sw; firstRun = false; }

  if (sw != prevSw) {
    if (sw)  wardriveEnter();
    else     wardriveExit();
    prevSw = sw;
  }
  g_wardriveOn = sw;

  if (sw) wardriveTask();

  // Battery every 10 s
  static unsigned long lastBat = 0;
  if (millis() - lastBat > 10000UL) {
    lastBat = millis();
    updateBattery();
  }

  // Dim button + auto-dim
  static unsigned long lastActivity = millis();
  static bool          prevBtn      = HIGH;
  bool curBtn = digitalRead(DIM_BTN);
  if (prevBtn == HIGH && curBtn == LOW) {
    g_dimmed     = !g_dimmed;
    lastActivity = millis();
  }
  prevBtn = curBtn;
  if (g_gpsLocked || g_wardriveOn) lastActivity = millis();
  if (!g_dimmed && (millis() - lastActivity > DIM_TIMEOUT)) {
    g_dimmed = true;
  }

  // Redraw on state change
  static int  lastSec      = -1;
  static bool lastLocked   = false;
  static bool lastWardrive = false;
  static bool lastCharging = false;
  static bool lastDimmed   = false;
  int h, m, s, h24;
  getClock(h, m, s, h24);
  if (s != lastSec || g_gpsLocked != lastLocked ||
      g_wardriveOn != lastWardrive || g_charging != lastCharging ||
      g_dimmed != lastDimmed) {
    lastSec      = s;
    lastLocked   = g_gpsLocked;
    lastWardrive = g_wardriveOn;
    lastCharging = g_charging;
    lastDimmed   = g_dimmed;
    renderFrame();
  }
}
