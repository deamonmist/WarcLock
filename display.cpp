/*
 * display.cpp — canvas setup and frame rendering implementation
 * WarcLock.ino — by Deamonmist (multi-file)
 */
#include "config.h"
#include "display.h"
#include "gpsclock.h"
#include "messages.h"
#include <math.h>

static Arduino_DataBus *bus    = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, GFX_NOT_DEFINED);
static Arduino_GFX     *output = new Arduino_GC9A01(bus, TFT_RST, 0, true);

bool displayBegin() {
  gfx = new Arduino_Canvas(240, 240, output);
  if (!gfx->begin(40000000)) {
    return false;
  }
  gfx->fillScreen(C_BLACK);
  gfx->flush();
  return true;
}

// ── Local drawing helpers ─────────────────────────────────────────────
static void thickLine(int x0, int y0, int x1, int y1, uint16_t color, int thickness) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len == 0) return;
  float nx = -dy / len, ny = dx / len;
  for (int t = -(thickness / 2); t <= thickness / 2; t++) {
    gfx->drawLine(x0 + (int)(nx * t), y0 + (int)(ny * t),
                  x1 + (int)(nx * t), y1 + (int)(ny * t), color);
  }
}

static void spearTip(float angle, int length, uint16_t color) {
  float a = angle - PI / 2;
  int tx = CX + (int)(cos(a) * length);
  int ty = CY + (int)(sin(a) * length);
  int bx = CX + (int)(cos(a) * (length - 12));
  int by = CY + (int)(sin(a) * (length - 12));
  float perp = a + PI / 2;
  gfx->fillTriangle(tx, ty,
    bx + (int)(cos(perp) * 4), by + (int)(sin(perp) * 4),
    bx - (int)(cos(perp) * 4), by - (int)(sin(perp) * 4),
    color);
}

static void romanNumeral(int pos, int x, int y) {
  const char* labels[] = {"XII","I","II","III","IV","V","VI","VII","VIII","IX","X","XI"};
  gfx->setTextColor(C_TEAL2);
  gfx->setTextSize(1);
  gfx->setCursor(x - (strlen(labels[pos]) * 3), y - 4);
  gfx->print(labels[pos]);
}

// Draw a thick arc as a series of short segments (centre cx,cy)
static void arcSeg(int cx, int cy, int radius, float a0, float a1,
                   uint16_t col, int thick) {
  const int steps = 24;
  for (int i = 0; i < steps; i++) {
    float t0 = a0 + (a1 - a0) * (i     / (float)steps);
    float t1 = a0 + (a1 - a0) * ((i+1) / (float)steps);
    thickLine(cx + (int)(cos(t0) * radius), cy + (int)(sin(t0) * radius),
              cx + (int)(cos(t1) * radius), cy + (int)(sin(t1) * radius),
              col, thick);
  }
}

// WiFi "fan" icon: a dot with nested arcs opening upward (GPS locked).
static void wifiIcon(int wx, int wy) {
  int dotY = wy + 8;
  uint16_t cols[3]  = {C_TEAL2, C_TEAL, C_TEAL3};
  int      radii[3] = {6, 11, 16};
  const float a0 = -2.44f, a1 = -0.70f;
  const int steps = 22;
  for (int r = 0; r < 3; r++) {
    for (int s = 0; s < steps; s++) {
      float t0 = a0 + (a1 - a0) * (s     / (float)steps);
      float t1 = a0 + (a1 - a0) * ((s+1) / (float)steps);
      gfx->drawLine(wx + (int)(cos(t0) * radii[r]), dotY + (int)(sin(t0) * radii[r]),
                    wx + (int)(cos(t1) * radii[r]), dotY + (int)(sin(t1) * radii[r]),
                    cols[r]);
    }
  }
  gfx->fillCircle(wx, dotY, 2, C_TEAL2);
}

// Wardrive status dot above Deamonmist: hollow = off, filled = on.
static void wardriveIcon(int wx, int wy) {
  if (g_wardriveOn) {
    gfx->fillCircle(wx, wy, 4, C_BLUE);
    gfx->drawCircle(wx, wy, 4, C_BLUE2);
  } else {
    gfx->drawCircle(wx, wy, 4, C_BLUE);
  }
}

// Small lightning bolt in the gauge center (charging indicator)
static void drawBolt(int cx, int cy, uint16_t col) {
  thickLine(cx + 3, cy - 7, cx - 2, cy,     col, 2);
  thickLine(cx - 2, cy,     cx + 2, cy,     col, 2);
  thickLine(cx + 2, cy,     cx - 3, cy + 7, col, 2);
}

// Battery arc gauge, right of center.
//  - on battery: smooth 0-100%, colour by level, number in middle
//  - charging:   animated sweep + lightning bolt (percentage is unreliable
//                while the charger drives the rail, so we don't show it)
static void batteryGauge() {
  const float sweepStart = 150.0f * PI / 180.0f;   // open-top C shape
  const float sweepEnd   = 390.0f * PI / 180.0f;

  // Empty track
  arcSeg(BAT_GX, BAT_GY, BAT_GR, sweepStart, sweepEnd, C_TRACK, 3);

  if (g_charging) {
    // Animated fill that cycles each second, plus a bolt
    int h, m, s, h24;
    getClock(h, m, s, h24);
    float frac = ((s % 4) + 1) / 4.0f;
    float fillEnd = sweepStart + (sweepEnd - sweepStart) * frac;
    arcSeg(BAT_GX, BAT_GY, BAT_GR, sweepStart, fillEnd, C_TEAL2, 3);
    drawBolt(BAT_GX, BAT_GY, C_GOLD2);
    return;
  }

  // On battery: colour by charge level
  uint16_t col = C_TEAL2;
  if (g_batPct <= 15)      col = C_RED;
  else if (g_batPct <= 35) col = C_AMBER;

  float fillEnd = sweepStart + (sweepEnd - sweepStart) * (g_batPct / 100.0f);
  arcSeg(BAT_GX, BAT_GY, BAT_GR, sweepStart, fillEnd, col, 3);

  char buf[5];
  snprintf(buf, sizeof(buf), "%d", g_batPct);
  int w = strlen(buf) * 6;
  gfx->setTextColor(col);
  gfx->setTextSize(1);
  gfx->setCursor(BAT_GX - w / 2, BAT_GY - 4);
  gfx->print(buf);
}

// Status dots, left of center (mirror of the battery gauge on the right).
//  red  = charging (USB power detected)
static void statusDots() {
  const int sx = CX - 34;
  if (g_charging)         gfx->fillCircle(sx, CY - 7, 4, C_RED);
}

// ── Scrolling message strip ───────────────────────────────────────────
//
// Sits in the arc between the top tick ring and the WiFi icon (~Y 18-34).
// One message scrolls right-to-left once, then nothing until the next
// random trigger (1-10 minutes).
//
// State is entirely local — nothing outside renderFrame needs to know.

#define SCROLL_Y       22    // vertical centre of the text strip
#define SCROLL_SPEED    6    // pixels per second (left movement)
#define CHAR_W          6    // pixels per char at textSize(1)
#define FACE_LEFT      14    // leftmost X still inside the round face at SCROLL_Y
#define FACE_RIGHT    226    // rightmost X still inside the round face at SCROLL_Y

static void updateScroller() {
  static int    scrollX       = FACE_RIGHT;   // current left edge of text
  static int    msgIndex      = -1;           // -1 = none active
  static unsigned long nextTrigger = 0;       // millis() when next msg fires
  static bool   initialised   = false;

  if (!initialised) {
    randomSeed(millis());
    nextTrigger = millis() + random(60000UL, 600000UL);
    initialised = true;
  }

  unsigned long now = millis();

  // Time to pick a new message?
  if (msgIndex == -1 && now >= nextTrigger) {
    msgIndex = random(0, g_messageCount);
    scrollX  = 240;   // start fully off the right edge of the display
  }

  if (msgIndex == -1) return;   // nothing to draw right now

  // Erase the strip (a thin horizontal band across the face)
  int msgW = strlen(g_messages[msgIndex]) * CHAR_W;
  // Erase generously: previous position + a little buffer
  gfx->fillRect(FACE_LEFT, SCROLL_Y - 5, FACE_RIGHT - FACE_LEFT, 14, C_DKGREEN);

  // Redraw any face decoration erased in that strip
  // (the dashed inner ring at R-45 = 70px from centre; strip is ~8px from
  //  centre top, well inside the ring — nothing to restore)

  // Draw text clipped to face interior
  if (scrollX < FACE_RIGHT) {
    gfx->setTextColor(C_TEAL2);
    gfx->setTextSize(1);
    gfx->setCursor(scrollX, SCROLL_Y - 4);
    gfx->print(g_messages[msgIndex]);
  }

  // Advance position
  scrollX -= SCROLL_SPEED;

  // Message fully off the left edge — done
  if (scrollX + msgW < FACE_LEFT) {
    msgIndex    = -1;
    nextTrigger = millis() + random(60000UL, 600000UL);
  }
}

// ── Full frame render ─────────────────────────────────────────────────

void renderFrame() {
  // Dim mode: blank screen to save power. Button press restores.
  if (g_dimmed) {
    gfx->fillScreen(C_BLACK);
    gfx->flush();
    return;
  }

  // 1. Clear + bezel + face
  gfx->fillScreen(C_BLACK);
  gfx->drawCircle(CX, CY, R+4, C_GOLD);
  gfx->drawCircle(CX, CY, R+5, C_GOLD);
  gfx->drawCircle(CX, CY, R+7, C_GOLD2);
  gfx->drawCircle(CX, CY, R+9, C_TEAL);
  gfx->fillCircle(CX, CY, R, C_DKGREEN);

  // 2. 72 tick marks
  for (int i = 0; i < 72; i++) {
    float a = (i / 72.0) * 2 * PI;
    bool major = (i % 6 == 0);
    int r1 = R - 2, r2 = major ? R - 10 : R - 6;
    uint16_t col = major ? C_TEAL2 : C_TEAL;
    int x1 = CX + (int)(cos(a) * r1), y1 = CY + (int)(sin(a) * r1);
    int x2 = CX + (int)(cos(a) * r2), y2 = CY + (int)(sin(a) * r2);
    gfx->drawLine(x1, y1, x2, y2, col);
    if (major) gfx->drawLine(x1+1, y1, x2+1, y2, col);
  }

  // 3. Chevron hour markers
  int chevR = R - 20;
  for (int i = 0; i < 12; i++) {
    float a = (i / 12.0) * 2 * PI - PI/2;
    float perp = a + PI/2;
    int mx = CX + (int)(cos(a) * chevR), my = CY + (int)(sin(a) * chevR);
    int tx = mx + (int)(cos(a) * 6),     ty = my + (int)(sin(a) * 6);
    gfx->drawLine(mx + (int)(cos(perp)*5), my + (int)(sin(perp)*5), tx, ty, C_TEAL2);
    gfx->drawLine(mx - (int)(cos(perp)*5), my - (int)(sin(perp)*5), tx, ty, C_TEAL2);
  }

  // 4. Inner dashed ring
  int innerR = R - 45;
  for (int i = 0; i < 120; i++) {
    if (i % 3 == 0) continue;
    float a = (i / 120.0) * 2 * PI;
    gfx->drawPixel(CX + (int)(cos(a) * innerR),     CY + (int)(sin(a) * innerR), C_GOLD);
    gfx->drawPixel(CX + (int)(cos(a) * innerR) + 1, CY + (int)(sin(a) * innerR), C_GOLD);
  }

  // 5. Inner ring ticks
  for (int i = 0; i < 12; i++) {
    float a = (i / 12.0) * 2 * PI;
    gfx->drawLine(CX + (int)(cos(a) * (innerR - 2)), CY + (int)(sin(a) * (innerR - 2)),
                  CX + (int)(cos(a) * (innerR - 9)), CY + (int)(sin(a) * (innerR - 9)), C_TEAL);
  }

  // 6. Diagonal accent lines
  for (int i = 0; i < 4; i++) {
    float a = (i / 4.0) * 2 * PI - PI/4;
    int x1 = CX + (int)(cos(a) * 40), y1 = CY + (int)(sin(a) * 40);
    int x2 = CX + (int)(cos(a) * 68), y2 = CY + (int)(sin(a) * 68);
    gfx->drawLine(x1, y1, x2, y2, C_GOLD);
    gfx->fillCircle(x2, y2, 2, C_GOLD);
  }

  // 7. Cross ticks
  for (int i = 0; i < 4; i++) {
    float a = (i / 4.0) * 2 * PI;
    int r = innerR - 20;
    gfx->drawLine(CX + (int)(cos(a) * (r-8)),       CY + (int)(sin(a) * (r-8)),
                  CX + (int)(cos(a) * (r+8)),       CY + (int)(sin(a) * (r+8)), C_TEAL);
    gfx->drawLine(CX + (int)(cos(a+PI/2) * (r-4)),  CY + (int)(sin(a+PI/2) * (r-4)),
                  CX + (int)(cos(a+PI/2) * (r+4)),  CY + (int)(sin(a+PI/2) * (r+4)), C_TEAL);
  }

  // 8. Roman numerals
  romanNumeral(0, CX,          CY - (R-38));
  romanNumeral(3, CX + (R-38), CY);
  romanNumeral(6, CX,          CY + (R-38));
  romanNumeral(9, CX - (R-38), CY);

  // 9. WiFi (GPS-lock) icon, only when locked
  if (g_gpsLocked) {
    wifiIcon(WIFI_X, WIFI_Y);
  }

  // 9b. Scrolling message strip
  updateScroller();

  // 10. Battery gauge, right of center
  batteryGauge();

  // Status dots (charging / uploading), left of center
  statusDots();

  // 11. Wardrive status dot above Deamonmist
  wardriveIcon(CX, CY + 38);

  // 12. Deamonmist label
  gfx->setTextColor(C_TEAL);
  gfx->setTextSize(1);
  gfx->setCursor(CX - 27, CY + 48);
  gfx->print("Deamonmist");

  // 13. Hands
  int h, m, s, h24;
  getClock(h, m, s, h24);
  float sA = (s / 60.0) * 2 * PI;
  float mA = ((m + s / 60.0) / 60.0) * 2 * PI;
  float hA = ((h + m / 60.0) / 12.0) * 2 * PI;

  {
    float a = hA - PI/2;
    thickLine(CX + (int)(cos(a)*14), CY + (int)(sin(a)*14),
              CX + (int)(cos(a)*62), CY + (int)(sin(a)*62), C_GOLD2, 5);
    spearTip(hA, 62, C_GOLD2);
  }
  {
    float a = mA - PI/2;
    thickLine(CX + (int)(cos(a)*14), CY + (int)(sin(a)*14),
              CX + (int)(cos(a)*90), CY + (int)(sin(a)*90), C_GOLD2, 3);
    spearTip(mA, 90, C_GOLD2);
  }
  {
    float a = sA - PI/2;
    thickLine(CX + (int)(cos(a)*14),  CY + (int)(sin(a)*14),
              CX + (int)(cos(a)*100), CY + (int)(sin(a)*100), C_TEAL2, 2);
  }

  // 14. Center cap
  gfx->fillCircle(CX, CY, 5, C_TEAL2);
  gfx->drawCircle(CX, CY, 5, C_GOLD);
  gfx->drawCircle(CX, CY, 6, C_GOLD);

  // 15. Blit to panel
  gfx->flush();
}