#pragma once
/*
  SacredMonth.h
  ------------------------------------------------------------
  On the two Eid days (1 Shawwal, 10 Dhu al-Hijjah) from 9:00 the same zone
  shows the Arabic greeting, until BACK. During the four sacred months (Muharram, Rajab, Dhu al-Qa'dah,
  Dhu al-Hijjah) the info zone of the watch face shows "SACRED MONTH"
  in Noto Sans Mono 17 px. A short BACK press hides it for the rest of that
  month (saved, so it stays hidden after a restart). The sakura crest
  (Garden.h) has priority: BACK hides the crest first, then this message.

  HIFZ TIME: the optional alarm set in HIFZ TRACKER > SETTINGS > ALARM shows
  a reading person with "HIFZ TIME" in the same zone, from the chosen time on
  the chosen weekdays, until BACK (or one hour). It has priority over the
  other messages, which come back after BACK.
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "HijriCalendar.h"
#include "Storage.h"
#include "UiCommon.h"
#include "NotoMono17.h"
#include "EidMessage.h"
#include "NotoMono13.h"
#include "AlarmCore.h"
#include "TeaCore.h"
#include <math.h>

namespace Sac {

RTC_DATA_ATTR uint16_t done = 0;           // month key hidden by BACK (year * 13 + month)
RTC_DATA_ATTR uint8_t loaded = 0;          // 1 once "done" was read from the flash
RTC_DATA_ATTR uint8_t dismissPending = 0;  // set by main.cpp when BACK was pressed

inline bool isSacred(int month) { return month == 1 || month == 7 || month == 11 || month == 12; }
inline uint16_t keyOf(const HijriDate &h) { return (uint16_t)(h.year * 13 + h.month); }

enum Show : uint8_t { SHOW_NONE = 0, SHOW_SACRED = 1, SHOW_EID = 2, SHOW_ZAKAT = 3, SHOW_HIFZ = 4, SHOW_ALARM = 5, SHOW_TEA = 6, SHOW_WHITE = 7 };
RTC_DATA_ATTR int8_t ringIdx = -1;   // alarm ringing (ALARMS app), valid when state() returned SHOW_ALARM

// ---- Hifz alarm (edited in Hifz.h, saved in the blob "hifzA")
RTC_DATA_ATTR uint8_t alLoaded = 0, alOn = 0, alH = 6, alM = 0, alDays = 0x7F, alVib = 1;   // days: bit 0 = Monday
RTC_DATA_ATTR uint32_t alDone = 0;                                                        // day dismissed by BACK
RTC_DATA_ATTR uint32_t alBuzzed = 0;                                                      // day already vibrated
inline void loadAlarm() {
  if (alLoaded) return;
  uint8_t b[5];
  if (QWStorage::loadBlob("hifzA", b, sizeof(b)) && b[1] < 24 && b[2] < 60) {
    alOn = b[0] & 1; alH = b[1]; alM = b[2]; alDays = b[3] & 0x7F; alVib = b[4] & 1;
  }
  alLoaded = 1;
}
inline void saveAlarm() {
  uint8_t b[5] = {alOn, alH, alM, alDays, alVib};
  QWStorage::saveBlob("hifzA", b, sizeof(b));
}
// dayKey = days since 1970 (Thursday), Monday = weekday 0
inline bool alarmDue(int nowMin, uint32_t dayKey) {
  loadAlarm();
  if (!alOn || nowMin < 0) return false;
  int wd = (int)((dayKey + 3) % 7), a = alH * 60 + alM;
  if (!((alDays >> wd) & 1)) return false;
  return nowMin >= a && nowMin < a + 60 && alDone != dayKey;
}

RTC_DATA_ATTR uint16_t eidDone = 0;  // Eid key hidden by BACK (year * 2 + 0 Fitr / 1 Adha)
RTC_DATA_ATTR uint16_t zakatDone = 0;  // Zakat reminder hidden by BACK (year * 13 + month)
RTC_DATA_ATTR uint16_t whiteDone = 0;  // White Days reminder hidden by BACK (year * 13 + month)

// Eid al-Fitr = 1 Shawwal, Eid al-Adha = 10 Dhu al-Hijjah.
inline bool isEid(const HijriDate &h) { return (h.month == 10 && h.day == 1) || (h.month == 12 && h.day == 10); }
inline uint16_t eidKey(const HijriDate &h) { return (uint16_t)(h.year * 2 + (h.month == 12 ? 1 : 0) + 1); }

// What the info zone must show now: the Eid greeting from 9:00, then the
// zakat reminder (zakat = true when it is due: chosen month, day 1, after
// Dhuhr), otherwise the sacred month message. Also applies a pending BACK to
// whatever is showing.
inline uint8_t state(const HijriDate &h, int hour24, bool zakat = false, int nowMin = -1, uint32_t dayKey = 0, bool white = false) {
  if (!loaded) {
    uint16_t v = 0;
    if (QWStorage::loadBlob("sacDone", &v, sizeof(v))) done = v;
    if (QWStorage::loadBlob("eidDone", &v, sizeof(v))) eidDone = v;
    if (QWStorage::loadBlob("zakatDone", &v, sizeof(v))) zakatDone = v;
    if (QWStorage::loadBlob("whiteDone", &v, sizeof(v))) whiteDone = v;
    loaded = 1;
  }
  bool eid = isEid(h) && hour24 >= 9 && eidDone != eidKey(h);
  zakat = zakat && zakatDone != keyOf(h);
  white = white && whiteDone != keyOf(h);
  bool sac = isSacred(h.month) && done != keyOf(h);
  bool hifz = alarmDue(nowMin, dayKey);
  int ra = Alm::ringing(nowMin, dayKey);
  if (dismissPending) {
    dismissPending = 0;
    if (Tea::state == 2) {
      Tea::state = 0;
      Tea::buzz = 0;
    } else if (ra >= 0) {
      Alm::dis[ra] = dayKey;
      ra = -1;
    } else if (hifz) {
      alDone = dayKey;
      hifz = false;
    } else if (eid) {
      eidDone = eidKey(h);
      QWStorage::saveBlob("eidDone", &eidDone, sizeof(eidDone));
      eid = false;
    } else if (zakat) {
      zakatDone = keyOf(h);
      QWStorage::saveBlob("zakatDone", &zakatDone, sizeof(zakatDone));
      zakat = false;
    } else if (white) {
      whiteDone = keyOf(h);
      QWStorage::saveBlob("whiteDone", &whiteDone, sizeof(whiteDone));
      white = false;
    } else if (sac) {
      done = keyOf(h);
      QWStorage::saveBlob("sacDone", &done, sizeof(done));
      sac = false;
    }
  }
  ringIdx = (int8_t)ra;
  if (Tea::state == 2) return SHOW_TEA;
  if (ra >= 0) return SHOW_ALARM;
  return hifz ? SHOW_HIFZ : (eid ? SHOW_EID : (zakat ? SHOW_ZAKAT : (white ? SHOW_WHITE : (sac ? SHOW_SACRED : SHOW_NONE))));
}

inline void line(Adafruit_GFX &d, const char *t, int16_t baseline) {
  int16_t x1, y1;
  uint16_t w, h;
  d.getTextBounds(t, 0, baseline, &x1, &y1, &w, &h);
  d.setCursor(100 - (int16_t)w / 2 - x1, baseline);
  d.print(t);
}

// Fills the info zone (x 64..136, y 0..64).
inline void draw(Adafruit_GFX &d) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  d.setFont(&NotoMono17);
  d.setTextSize(1);
  d.setTextColor(ink);
  line(d, "SACRED", 28);
  line(d, "MONTH", 47);
  d.setFont(nullptr);
}

// Ringing alarm: sonar picture (a dot with three pairs of arcs above and below) and the alarm name.
inline void drawAlarm(Adafruit_GFX &d, int idx) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  d.fillCircle(100, 27, 5, ink);
  for (int r = 10; r <= 20; r += 5)
    for (int a = 40; a <= 140; a++) {
      float t = a * 0.0174533f;
      int x = (int)(100.0f + r * cosf(t) + (cosf(t) < 0 ? -0.5f : 0.5f)), y = (int)(27.0f + r * sinf(t) + 0.5f), y2 = 54 - y;
      d.drawPixel(x, y, ink); d.drawPixel(x, y + 1, ink);
      d.drawPixel(x, y2, ink); d.drawPixel(x, y2 - 1, ink);
    }
  const char *nm = idx == Alm::MAXA ? "QUICK" : ((idx >= 0 && idx < Alm::n && Alm::al[idx].name[0]) ? Alm::al[idx].name : "ALARM");
  d.setFont(nullptr);
  d.setTextSize(1);
  d.setTextColor(ink);
  int x = 100 - 3 * (int)strlen(nm);
  d.setCursor(x, 51); d.print(nm);
  d.setCursor(x + 1, 51); d.print(nm);
}

// Tea ready: a bowl-shaped cup on a saucer (no handle) with three wavy steam lines.
// Everything is symmetric about x = 99.5, the centre of the info zone frame.
inline void drawTea(Adafruit_GFX &d) {
  uint16_t ink = qwInk(), pap = qwPaper();
  d.fillRect(64, 0, 72, 64, pap);
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  for (int y = 31; y <= 49; y++) {                           // cup: flat top, rounded bowl
    float t = (y - 31) / 19.0f;
    int hw = (int)lroundf(22.0f * powf(1.0f - powf(t, 2.6f), 1.0f / 2.6f));
    if (hw < 7) hw = 7;
    d.drawFastHLine(100 - hw, y, 2 * hw, ink);
  }
  static const uint8_t SAU[5] = {14, 22, 26, 22, 14};        // saucer: a flat lens
  for (int i = 0; i < 5; i++) d.drawFastHLine(100 - SAU[i], 51 + i, 2 * SAU[i], ink);
  for (int k = 0; k < 3; k++) {                              // steam
    int cx = 88 + k * 11;
    for (int y = 8; y <= 27; y++) {
      int x = cx + (int)lroundf(2.0f * sinf((y - 8) * 0.55f));
      d.drawPixel(x, y, ink); d.drawPixel(x + 1, y, ink);
    }
  }
}

// "HIFZ TIME": a person reading an open book (line icon), fills the info zone.
inline void thick(Adafruit_GFX &d, int x0, int y0, int x1, int y1, uint16_t c) {
  d.drawLine(x0, y0, x1, y1, c); d.drawLine(x0 + 1, y0, x1 + 1, y1, c); d.drawLine(x0, y0 + 1, x1, y1 + 1, c);
}
inline void drawHifz(Adafruit_GFX &d) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  // head
  d.drawCircle(100, 13, 6, ink); d.drawCircle(100, 13, 5, ink);
  // shoulders: upper half of an ellipse (rx 12, ry 8) centred on (100, 29), two pixels thick
  int px = 88, py = 29;
  for (int a = 180; a <= 360; a += 10) {
    float r = a * 0.0174533f;
    int x = 100 + (int)(12.0f * cosf(r) + (cosf(r) < 0 ? -0.5f : 0.5f)), y = 29 + (int)(8.0f * sinf(r) - 0.5f);
    thick(d, px, py, x, y, ink);
    px = x; py = y;
  }
  d.drawFastVLine(88, 29, 3, ink); d.drawFastVLine(89, 29, 3, ink);
  d.drawFastVLine(111, 29, 3, ink); d.drawFastVLine(112, 29, 3, ink);
  // open book: two pages meeting at the spine (x = 100)
  thick(d, 84, 35, 100, 39, ink); thick(d, 116, 35, 100, 39, ink);
  d.drawFastVLine(84, 35, 8, ink); d.drawFastVLine(85, 35, 8, ink);
  d.drawFastVLine(115, 35, 8, ink); d.drawFastVLine(116, 35, 8, ink);
  d.drawFastVLine(100, 39, 8, ink); d.drawFastVLine(101, 39, 8, ink);
  thick(d, 84, 43, 100, 47, ink); thick(d, 116, 43, 100, 47, ink);
  // message (6x8 font drawn twice for a bolder look)
  d.setFont(nullptr);
  d.setTextSize(1);
  d.setTextColor(ink);
  d.setCursor(73, 51); d.print("HIFZ TIME");
  d.setCursor(74, 51); d.print("HIFZ TIME");
}

// "May Allah accept it from us and from you" - fills the info zone.
inline void drawEid(Adafruit_GFX &d) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  d.drawBitmap(100 - EID_W / 2, 32 - EID_H / 2, EID_BITMAP, EID_W, EID_H, ink);
}

// "TIME TO PREPARE ZAKAT": the zakat is to be prepared (1st day of the chosen
// month, from Dhuhr). Fills the info zone.
inline void drawZakat(Adafruit_GFX &d) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  d.setTextSize(1);
  d.setTextColor(ink);
  d.setFont(&NotoMono13);
  line(d, "TIME TO", 20);
  line(d, "PREPARE", 35);
  d.setFont(&NotoMono17);
  line(d, "ZAKAT", 53);
  d.setFont(nullptr);
}

// "WHITE DAYS START TOMORROW": the eve of the 13th, 14th and 15th of the Hijri
// month (voluntary fasts). Fills the info zone.
inline void drawWhite(Adafruit_GFX &d) {
  uint16_t ink = qwInk();
  d.fillRect(64, 0, 72, 64, qwPaper());
  d.drawRect(66, 2, 68, 60, ink);
  d.drawRect(68, 4, 64, 56, ink);
  d.setTextSize(1);
  d.setTextColor(ink);
  d.setFont(&NotoMono13);
  line(d, "WHITE", 19);
  line(d, "DAYS", 33);
  d.setFont(nullptr);
  line(d, "START", 40);
  line(d, "TOMORROW", 50);
}

}  // namespace Sac
