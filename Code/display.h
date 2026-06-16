/*
 * display.h — canvas setup and frame rendering
 * WarcLock.ino — by Deamonmist (multi-file)
 */
#pragma once

bool displayBegin();   // allocate canvas + init panel. false if it fails.
void renderFrame();    // draw the full watch face for the current state
