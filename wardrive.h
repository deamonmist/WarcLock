/* WarcLock.ino — by Deamonmist (multi-file)
 * wardrive.h — WiFi scanning + WiGLE CSV logging
 *
 * Sessions are written to a temp file while active, then renamed on close
 * to MM-DD-YYYY-HHMMam-HHMMpm.csv using the start and end timestamps.
 */
#pragma once
 
bool wardriveInitSD();    // mount SD card (call once in setup)
void wardriveEnter();     // enter scan mode + open a new session file
void wardriveExit();      // close + rename session file, shut down radio
void wardriveTask();      // collect async scans while in wardrive mode