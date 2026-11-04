#pragma once
/*
  Storage.h
  ------------------------------------------------------------
  Saves/loads AppSettings, the Quran bookmark, and the time
  offset to the ESP32's non-volatile storage (flash), using the
  Preferences library (built into the Arduino-ESP32 core - no
  extra lib_deps needed).

  This survives power loss / battery changes, unlike RTC_DATA_ATTR
  (which only survives deep sleep, not a full power-off).
*/

#include <Arduino.h>
#include <Preferences.h>
#include "AppConfig.h"

namespace QWStorage {

static const char *NS = "quranwatch";

inline void loadSettings(AppSettings &settings) {
  Preferences prefs;
  prefs.begin(NS, true); // read-only
  settings.latitude = prefs.getFloat("lat", DEFAULT_APP_SETTINGS.latitude);
  settings.longitude = prefs.getFloat("lon", DEFAULT_APP_SETTINGS.longitude);
  settings.utcOffsetSeconds =
      prefs.getInt("utcOff", DEFAULT_APP_SETTINGS.utcOffsetSeconds);
  settings.daylightSaving =
      prefs.getBool("dst", DEFAULT_APP_SETTINGS.daylightSaving);
  settings.hijriAdjustment =
      (int8_t)prefs.getChar("hijriAdj", DEFAULT_APP_SETTINGS.hijriAdjustment);
  settings.prayerMethod =
      prefs.getUChar("prMethod", DEFAULT_APP_SETTINGS.prayerMethod);
  settings.asrMethod =
      prefs.getUChar("asrMethod", DEFAULT_APP_SETTINGS.asrMethod);
  settings.prayerVibrate =
      prefs.getBool("prVib", DEFAULT_APP_SETTINGS.prayerVibrate);
  settings.invertDisplay =
      prefs.getBool("invert", DEFAULT_APP_SETTINGS.invertDisplay);
  settings.prayerAlertVibrate =
      prefs.getBool("alertVib", DEFAULT_APP_SETTINGS.prayerAlertVibrate);
  settings.driftTenths =
      (int16_t)prefs.getShort("drift", DEFAULT_APP_SETTINGS.driftTenths);
  prefs.end();
}

inline void saveSettings(const AppSettings &settings) {
  Preferences prefs;
  prefs.begin(NS, false); // read/write
  prefs.putFloat("lat", settings.latitude);
  prefs.putFloat("lon", settings.longitude);
  prefs.putInt("utcOff", settings.utcOffsetSeconds);
  prefs.putBool("dst", settings.daylightSaving);
  prefs.putChar("hijriAdj", settings.hijriAdjustment);
  prefs.putUChar("prMethod", settings.prayerMethod);
  prefs.putUChar("asrMethod", settings.asrMethod);
  prefs.putBool("prVib", settings.prayerVibrate);
  prefs.putBool("invert", settings.invertDisplay);
  prefs.putBool("alertVib", settings.prayerAlertVibrate);
  prefs.putShort("drift", settings.driftTenths);
  prefs.end();
}

inline void loadBookmark(QuranBookmark &bm) {
  Preferences prefs;
  prefs.begin(NS, true);
  bm.surah = prefs.getUChar("surah", DEFAULT_BOOKMARK.surah);
  bm.verse = prefs.getUShort("verse", DEFAULT_BOOKMARK.verse);
  prefs.end();
}

inline void saveBookmark(const QuranBookmark &bm) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putUChar("surah", bm.surah);
  prefs.putUShort("verse", bm.verse);
  prefs.end();
}

// Time offset (seconds) added to the RTC's own time to get the time
// shown on the watch. Lets us "set" the time/date without ever
// writing to the RTC chip - see main.cpp.
inline int32_t loadTimeOffset() {
  Preferences prefs;
  prefs.begin(NS, true);
  int32_t v = prefs.getInt("tOff", 0);
  prefs.end();
  return v;
}

inline void saveTimeOffset(int32_t offsetSeconds) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putInt("tOff", offsetSeconds);
  prefs.end();
}

// RTC time (seconds since 1970, raw RTC reading) from which the clock
// drift correction is accumulated. 0 = no reference yet (no correction).
inline uint32_t loadDriftRef() {
  Preferences prefs;
  prefs.begin(NS, true);
  uint32_t v = prefs.getUInt("dRef", 0);
  prefs.end();
  return v;
}

inline void saveDriftRef(uint32_t rtcSeconds) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putUInt("dRef", rtcSeconds);
  prefs.end();
}

// Moment (UTC, seconds since 1970) of the last bookmark change, i.e.
// the last Quran reading. 0 = never recorded. Drives the 5 stars.
inline uint32_t loadLastReadTime() {
  Preferences prefs;
  prefs.begin(NS, true);
  uint32_t v = prefs.getUInt("lastRead", 0);
  prefs.end();
  return v;
}

inline void saveLastReadTime(uint32_t utcSeconds) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putUInt("lastRead", utcSeconds);
  prefs.end();
}

// Last position of the QURAN READER (unit 0..37 = surahs 78..114 then
// Ayat al-Kursi, and verse number inside the unit). Used for the
// long-press shortcut on the watch face.
inline void loadReaderPosition(uint8_t &unit, uint16_t &verse) {
  Preferences prefs;
  prefs.begin(NS, true);
  unit = prefs.getUChar("rdUnit", 0);
  verse = prefs.getUShort("rdVerse", 1);
  prefs.end();
}

inline void saveReaderPosition(uint8_t unit, uint16_t verse) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putUChar("rdUnit", unit);
  prefs.putUShort("rdVerse", verse);
  prefs.end();
}

// Start of the contemplation spiral: RTC minute (since 1970) at which
// the number line was at 1. 0 = not started yet.
inline uint32_t loadSpiralStart() {
  Preferences prefs;
  prefs.begin(NS, true);
  uint32_t v = prefs.getUInt("spStart", 0);
  prefs.end();
  return v;
}

inline void saveSpiralStart(uint32_t minutes) {
  Preferences prefs;
  prefs.begin(NS, false);
  prefs.putUInt("spStart", minutes);
  prefs.end();
}

} // namespace QWStorage