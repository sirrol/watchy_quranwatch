/*
  main.cpp - QuranWatch
  ------------------------------------------------------------
  Target : Watchy v2 (ESP32) - including clones, PlatformIO
  Screen : 200 x 200 monochrome e-paper

  Time/date: the RTC chip is NEVER written (some Watchy v2 clones
  misbehave on RTC writes). Displayed time =
        RTC time + stored offset (seconds)
  DST: ON adds +1 h to the displayed clock and to the prayer times
  (TIME ZONE in Settings is the STANDARD, non-DST offset).
  "Setting" the time/date just recomputes and stores that offset
  in flash (see Storage.h). The RTC is re-read on every wake-up,
  since Watchy does not refresh currentTime on a button press.

  Prayer vibration: checked on every wake-up (about once a
  minute). Vibrates once per prayer per day, at Fajr, Dhuhr, Asr,
  Maghrib and Isha (not sunrise), if enabled in Settings.

  Prayer alert: "<45min", "<30min", "<15min" then "Now" under the
  quarter-hour dots (see computePrayerAlert in UiLayout.h). One pulse
  when "<45min" appears, if enabled in Settings.

  QURAN READER: Juz Amma + Ayat al-Kursi (continuous text, like a mushaf
  page), in the main menu, or by a
  long press (1.2 s) on the bottom-right button from the watch face
  (see QuranReader.h). Goes back to the watch face after
  READER_TIMEOUT_MIN minutes without a button press.

  Reading stars: 5 stars under the bookmark. Changing the bookmark
  (BACK in the editor, value changed) restores all 5; one is lost
  every 24 h after that (see quranStarsLeft in UiLayout.h).

  Low battery: below LOW_BATTERY_PERCENT (AppConfig.h), the info
  zone's battery line inverts with a "!" and the watch vibrates an
  SOS in Morse code, repeated at most every 30 minutes.
*/

#include <Arduino.h>
#include <Watchy.h>
#include <TimeLib.h>

#include "AppConfig.h"
#include "QuranData.h"
#include "MoonPhase.h"
#include "HijriCalendar.h"
#include "PrayerTimes.h"
#include "Storage.h"
#include "UiLayout.h"
#include "InputHandler.h"

// Last prayer already signalled by vibration, and the last minute
// an SOS was buzzed - both survive deep sleep via RTC_DATA_ATTR so
// each event fires at most once (prayer) / every 30 min (battery).
RTC_DATA_ATTR int32_t qwLastVibKey = -1;
RTC_DATA_ATTR int32_t qwLastLowBatMinute = -1000000;
constexpr int32_t READER_TIMEOUT_MIN = 10;  // QURAN READER idle time-out
RTC_DATA_ATTR int32_t qwLastAlertKey = -1;  // last "<45min" alert buzzed

// Fast (partial) e-ink refresh in every menu and screen except the watch face: a full refresh takes about
// 1-2 s and flashes, a partial one about 0.3-0.5 s but leaves faint
// ghosts. Inside the games we use partial refreshes and force a full one
// every QW_FULL_EVERY presses, and whenever the screen type changes.
constexpr uint8_t QW_FULL_EVERY = 15;
RTC_DATA_ATTR int8_t qwScreenShown = -1;   // screen of the previous button refresh
RTC_DATA_ATTR uint8_t qwPartialCount = 0;  // partial refreshes since the last full one

class QuranWatch : public Watchy {
 public:
  explicit QuranWatch(const watchySettings &settings) : Watchy(settings) {}

  // +1 hour of civil time while DST is ON in Settings. The stored
  // offset never contains this hour, so toggling DST moves the
  // displayed clock and the prayer times together.
  int32_t dstShiftSeconds() {
    AppSettings s;
    QWStorage::loadSettings(s);
    return s.daylightSaving ? 3600 : 0;
  }

  // RTC time + our stored offset + DST shift. Re-reads the RTC every call:
  // Watchy does NOT refresh currentTime on a button wake-up, so it
  // would otherwise hold a stale value.
  // Clock drift correction accumulated since the reference moment, in
  // seconds: elapsed RTC seconds x drift (0.1 s/day units) / 864000.
  // Zero when there is no reference or the RTC went backwards.
  int32_t driftCorrection(time_t rtcNow, int16_t driftTenths) {
    uint32_t ref = QWStorage::loadDriftRef();
    if (driftTenths == 0 || ref == 0 || (uint32_t)rtcNow < ref) return 0;
    double acc = (double)((uint32_t)rtcNow - ref) * driftTenths / 864000.0;
    return (int32_t)(acc >= 0 ? acc + 0.5 : acc - 0.5);
  }

  tmElements_t effectiveTime() {
    RTC.read(currentTime);
    tmElements_t raw = currentTime;
    time_t rtcNow = makeTime(raw);
    AppSettings ds;
    QWStorage::loadSettings(ds);
    time_t t = rtcNow + QWStorage::loadTimeOffset() + dstShiftSeconds() +
               driftCorrection(rtcNow, ds.driftTenths);
    tmElements_t out;
    breakTime(t, out);
    return out;
  }

  // Vibrate (100 ON / 120 OFF / 100 ON) at Fajr, Dhuhr, Asr, Maghrib
  // and Isha - not at sunrise - if the option is enabled.
  void checkPrayerVibration() {
    AppSettings settings;
    QWStorage::loadSettings(settings);
    if (!settings.prayerVibrate) return;

    tmElements_t now = effectiveTime();
    PrayerTimesResult t = calculatePrayerTimes(now.Year + 1970, now.Month,
                                               now.Day, settings);
    int nowMin = now.Hour * 60 + now.Minute;
    int times[5] = {t.fajrMin, t.dhuhrMin, t.asrMin, t.maghribMin, t.ishaMin};
    int32_t dayNum = (int32_t)(makeTime(now) / 86400);

    for (int i = 0; i < 5; i++) {
      int diff = nowMin - times[i];
      if (diff >= 0 && diff <= 1) { // tolerate a wake-up 1 minute late
        int32_t key = dayNum * 10 + i;
        if (key != qwLastVibKey) {
          qwLastVibKey = key;
          qwPrayerBuzz();
        }
        break;
      }
    }
  }

  // One pulse when the "<45min" prayer alert appears (once per prayer),
  // if enabled in Settings. Uses the same stage logic as the display.
  void checkPrayerAlertVibration() {
    AppSettings settings;
    QWStorage::loadSettings(settings);
    if (!settings.prayerAlertVibrate) return;

    tmElements_t now = effectiveTime();
    PrayerAlert alert = computePrayerAlert(settings, now.Year + 1970,
                                           now.Month, now.Day,
                                           now.Hour * 60 + now.Minute);
    if (alert.stage != ALERT_45) return;

    int32_t dayNum = (int32_t)(makeTime(now) / 86400);
    int32_t key = dayNum * 10 + alert.prayer;
    if (key != qwLastAlertKey) {
      qwLastAlertKey = key;
      qwAlertBuzz();
    }
  }

  // SOS in Morse code once battery is at/below LOW_BATTERY_PERCENT,
  // repeated at most every 30 minutes.
  void checkLowBattery() {
    float v = getBatteryVoltage();
    if (batteryVoltageToPercent(v) > LOW_BATTERY_PERCENT) return;

    tmElements_t now = effectiveTime();
    int32_t nowMinute = (int32_t)(makeTime(now) / 60);
    if (nowMinute - qwLastLowBatMinute >= 30) {
      qwLastLowBatMinute = nowMinute;
      qwSosBuzz();
    }
  }

  // QURAN READER: back to the watch face after READER_TIMEOUT_MIN minutes
  // without any button press (the position is saved first).
  void checkReaderTimeout() {
    bool reader = (qwScreen == SCREEN_QURAN_PICK || qwScreen == SCREEN_QURAN_READ);
    bool games = (qwScreen == SCREEN_GAMES_MENU || qwScreen == SCREEN_GAME_PLAY ||
                  qwScreen == SCREEN_SPIRAL);
    if (!reader && !games) return;
    int32_t nowMin = (int32_t)(makeTime(effectiveTime()) / 60);
    if (nowMin - qwLastInteractMin >= READER_TIMEOUT_MIN) {
      if (reader) qwReaderSavePosition();
      qwScreen = SCREEN_WATCHFACE;
    }
  }

  void drawWatchFace() override {
    checkReaderTimeout();
    checkPrayerVibration();
    checkPrayerAlertVibration();
    checkLowBattery();

    AppSettings settings;
    QWStorage::loadSettings(settings);
    qwInvertColors = settings.invertDisplay; // software inversion (see
                                              // UiCommon.h) - this hardware's
                                              // display.invertDisplay() does
                                              // nothing, so we invert every
                                              // draw call ourselves instead

    switch (qwScreen) {
      case SCREEN_MAIN_MENU:
        qwDrawMainMenu(display);
        break;
      case SCREEN_SETTINGS:
        qwDrawSettingsScreen(display);
        break;
      case SCREEN_SET_TIME:
        qwDrawSetTimeScreen(display);
        break;
      case SCREEN_SET_DATE:
        qwDrawSetDateScreen(display);
        break;
      case SCREEN_VERSE_EDIT:
        qwDrawVerseEditScreen(display);
        break;
      case SCREEN_QURAN_PICK:
        qwDrawReaderPickScreen(display);
        break;
      case SCREEN_SPIRAL: {
        RTC.read(currentTime);
        uint32_t nowMin = (uint32_t)(makeTime(currentTime) / 60);  // raw RTC
        uint32_t startMin = QWStorage::loadSpiralStart();
        if (startMin == 0 || startMin > nowMin) {  // first opening
          startMin = nowMin;
          QWStorage::saveSpiralStart(startMin);
        }
        Sp::drawSpiral(display, Sp::numberAt(nowMin, startMin));
        if (qwSpiralResetAsk) Sp::drawResetBox(display);
        break;
      }
      case SCREEN_QURAN_READ:
        QReader::drawPage(display, qrUnit, qrPage);
        break;
      case SCREEN_GAMES_MENU:
        qwDrawGamesMenu(display);
        break;
      case SCREEN_GAME_PLAY:
        QW_GAMES[qwGameIndex].draw(display);
        if (qwGameLeaveAsk) qwDrawLeaveBox(display);
        break;
      case SCREEN_WATCHFACE:
      default: {
        QuranBookmark bookmark;
        QWStorage::loadBookmark(bookmark);

        tmElements_t now = effectiveTime();
        int gYear = now.Year + 1970;
        int gMonth = now.Month;
        int gDay = now.Day;
        int hour24 = now.Hour;
        int minute = now.Minute;

        // Real UTC time for the moon (time(nullptr) is not valid
        // after deep sleep on the Watchy).
        time_t utcNow = makeTime(now) - settings.utcOffsetSeconds -
                        (settings.daylightSaving ? 3600 : 0);
        float batteryVoltage = getBatteryVoltage();
        uint8_t stars =
            quranStarsLeft(utcNow, QWStorage::loadLastReadTime());

        drawMainFace(display, settings, bookmark, gYear, gMonth, gDay,
                     hour24, minute, utcNow, batteryVoltage, stars);
        break;
      }
    }
  }

  void handleButtonPress() override {
    tmElements_t eff = effectiveTime();

    QwCommit commit = handleQuranWatchButtons(
        *this, eff.Hour, eff.Minute, eff.Day, eff.Month, eff.Year + 1970);

    if (commit.commitTime || commit.commitDate) {
      tmElements_t desired = eff;
      if (commit.commitTime) {
        desired.Hour = commit.hour;
        desired.Minute = commit.minute;
      }
      if (commit.commitDate) {
        desired.Day = commit.day;
        desired.Month = commit.month;
        desired.Year = commit.year - 1970;
      }
      // Keep the RTC's own seconds so the offset is a whole number
      // of minutes (keeps the every-minute wake-up alarm aligned).
      desired.Second = currentTime.Second;

      tmElements_t raw = currentTime;
      // Subtract the DST hour: it is added back by effectiveTime().
      int32_t offset =
          (int32_t)(makeTime(desired) - makeTime(raw)) - dstShiftSeconds();
      QWStorage::saveTimeOffset(offset);
      // The drift correction restarts from this moment.
      QWStorage::saveDriftRef((uint32_t)makeTime(raw));
    } else if (commit.commitDrift) {
      // New drift value: fold what the old value accumulated into the
      // stored offset, then restart from now - the clock does not jump.
      time_t rtcNow = makeTime(currentTime);
      int32_t acc = driftCorrection(rtcNow, commit.oldDriftTenths);
      QWStorage::saveTimeOffset(QWStorage::loadTimeOffset() + acc);
      QWStorage::saveDriftRef((uint32_t)rtcNow);
    }

    // A bookmark change = a Quran reading: stamp the time (UTC, so a
    // later TIME ZONE / DST change does not disturb the 5 stars).
    if (commit.commitBookmark) {
      AppSettings st;
      QWStorage::loadSettings(st);
      time_t utcNow = makeTime(eff) - st.utcOffsetSeconds -
                      (st.daylightSaving ? 3600 : 0);
      QWStorage::saveLastReadTime((uint32_t)utcNow);
    }

    if (commit.spiralReset) {  // restart the contemplation spiral from 1
      RTC.read(currentTime);
      QWStorage::saveSpiralStart((uint32_t)(makeTime(currentTime) / 60));
    }

    // Remember when the last button was pressed (reader time-out).
    qwLastInteractMin = (int32_t)(makeTime(eff) / 60);

    bool partial = false;
    // Every screen except the watch face itself (menus, settings, time,
    // date, bookmark, Quran, games) refreshes fast.
    bool fastScreen = qwScreen != SCREEN_WATCHFACE;
    if (fastScreen && qwScreenShown == (int8_t)qwScreen && qwPartialCount < QW_FULL_EVERY) {
      partial = true;
      qwPartialCount++;
    } else {
      qwPartialCount = 0;
    }
    qwScreenShown = (int8_t)qwScreen;
    showWatchFace(partial);
  }
};

QuranWatch quranWatch(QURANWATCH_SETTINGS);

void setup() {
  quranWatch.init();
}

void loop() {
  // Watchy manages deep sleep itself - nothing to do here.
}
