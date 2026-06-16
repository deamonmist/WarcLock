/* WarcLock.ino — by Deamonmist (multi-file)
 * gpsclock.h — GPS parsing and drift-free timekeeping
 */
#pragma once
#include <TinyGPSPlus.h>

extern TinyGPSPlus gps;   // shared so wardrive.cpp can read gps.date

void gpsBegin();
void gpsFeed();
void gpsUpdate();
void getClock(int &h, int &m, int &s, int &h24);
