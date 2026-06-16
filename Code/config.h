/*
 * config.h — shared constants and global declarations
 * WarcLock.ino — by Deamonmist (multi-file)
 */
#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ── Timezone ──────────────────────────────────────────────────────────
#define UTC_OFFSET  -4    // EDT = -4, EST = -5

// ── Display pins ──────────────────────────────────────────────────────
#define TFT_CS   25
#define TFT_DC    7
#define TFT_RST   0
#define TFT_SCK   8
#define TFT_MOSI 10

// ── SD card ───────────────────────────────────────────────────────────
#define SD_CS     1

// ── GPS UART ──────────────────────────────────────────────────────────
#define GPS_RX   12
#define GPS_TX   11

// ── Wardrive slide switch ─────────────────────────────────────────────
#define WARDRIVE_SW  24   // D5 = GPIO24 on XIAO ESP32-C5. Switch to GND. LOW=wardrive, HIGH=idle
#define DIM_BTN      23   // D4 = GPIO23. Tactile button to GND (INPUT_PULLUP)
#define DIM_TIMEOUT  120000UL  // auto-dim after 2 minutes of inactivity (ms)

// ── Battery voltage sensing ───────────────────────────────────────────
#define BAT_VOLT_PIN     6
#define BAT_VOLT_PIN_EN  26
#define BAT_CALIBRATION  1.16f
#define CHARGE_THRESHOLD 4.22f

// ── Palette (RGB565) ──────────────────────────────────────────────────
#define C_BLACK   0x0000
#define C_DKGREEN 0x0A21
#define C_GOLD    0xC488
#define C_GOLD2   0xE74C
#define C_TEAL    0x15F6
#define C_TEAL2   0x2EFA
#define C_TEAL3   0x0A4A
#define C_CREAM   0xD651
#define C_BLUE    0x2D7F
#define C_BLUE2   0x5CDF
#define C_AMBER   0xDD28
#define C_RED     0xD14A
#define C_GREEN   0x2E68   // reserved
#define C_TRACK   0x09C5

// ── Face geometry ─────────────────────────────────────────────────────
#define CX  120
#define CY  120
#define R   115
#define WIFI_X  CX
#define WIFI_Y  78
#define BAT_GX  (CX + 34)
#define BAT_GY  CY
#define BAT_GR  14

// ── Shared globals (defined in WarWatch01.ino) ────────────────────────
extern Arduino_GFX *gfx;

extern volatile bool g_gpsLocked;
extern volatile bool g_wardriveOn;
extern float g_lat, g_lon, g_alt, g_hdop;

extern bool g_sdOK;
extern char g_logFile[32];

extern int  g_batPct;
extern bool g_charging;
extern bool g_dimmed;          // true when display is in black-screen dim mode
