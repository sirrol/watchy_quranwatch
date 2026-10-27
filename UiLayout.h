#pragma once
/*
  UiLayout.h
  ------------------------------------------------------------
  Draws the normal watch face (the 9 information zones) on a
  200x200 monochrome canvas.

  Layout:
    +--------+----------------+----------+
    | Moon   |  Info (Hijri   |  Quran   |   row 1 (0-64)
    | phase  |  date, battery)|  bookmark|
    +--------+----------------+----------+
    | Hijri  |     Hour       | Quran %  |
    | 4x3    |   (quarter-    | bar      |   row 2 (64-160)
    | grid   |    hour dots)  |          |
    +--------+----------------+----------+
    |         Prayer times (2x3)          |   row 3 (160-200)
    +--------------------------------------+
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>

#include "UiCommon.h"
#include "AppConfig.h"
#include "QuranData.h"
#include "MoonPhase.h"
#include "HijriCalendar.h"
#include "PrayerTimes.h"

#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0
#endif
#ifndef GxEPD_WHITE
#define GxEPD_WHITE 1
#endif

// ---------------------------------------------------------------
// Layout constants (200 x 200 canvas)
// ---------------------------------------------------------------
namespace UiZones {
constexpr int16_t W = 200, H = 200;

constexpr int16_t ROW1_Y0 = 0,   ROW1_Y1 = 64;
constexpr int16_t ROW2_Y0 = 64,  ROW2_Y1 = 160;
constexpr int16_t ROW3_Y0 = 160, ROW3_Y1 = 200;

constexpr int16_t R1_COL1_X0 = 0,   R1_COL1_X1 = 64;  // moon
constexpr int16_t R1_COL2_X0 = 64,  R1_COL2_X1 = 136; // info
constexpr int16_t R1_COL3_X0 = 136, R1_COL3_X1 = 200; // bookmark

constexpr int16_t R2_COL1_X0 = 0,   R2_COL1_X1 = 72;  // hijri grid
constexpr int16_t R2_COL2_X0 = 72,  R2_COL2_X1 = 146; // hour
constexpr int16_t R2_COL3_X0 = 146, R2_COL3_X1 = 200; // Quran % bar
} // namespace UiZones

// ---------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------
inline uint8_t batteryVoltageToPercent(float v) {
  // Simple LiPo curve approximation (3.3V = 0%, 4.2V = 100%).
  if (v >= 4.2f) return 100;
  if (v <= 3.3f) return 0;
  float pct = (v - 3.3f) / (4.2f - 3.3f) * 100.0f;
  return (uint8_t)pct;
}

// Centers `text` horizontally around cx, top of the glyphs at topY,
// at the given built-in-font size.
inline void drawCenteredText(Adafruit_GFX &d, const char *text, int16_t cx,
                              int16_t topY, uint8_t size) {
  qwSetText(d, size);
  int16_t w = qwTextInkWidth((uint8_t)strlen(text), size);
  d.setCursor(cx - w / 2, topY);
  d.print(text);
}

// Draws a thick vertical progress bar. Fill grows from the bottom.
inline void drawVerticalBar(Adafruit_GFX &d, int16_t x, int16_t y,
                             int16_t w, int16_t h, uint8_t percent) {
  d.drawRect(x, y, w, h, qwInk());
  int16_t fillH = (int16_t)((int32_t)(h - 2) * percent / 100);
  if (fillH > 0) {
    d.fillRect(x + 1, y + (h - 1 - fillH), w - 2, fillH, qwInk());
  }
}

// ---------------------------------------------------------------
// Zone 1: Moon phase
// ---------------------------------------------------------------
// TINY-BEGIN
// ---------------------------------------------------------------
// Tiny 3x5 pixel font (digits and %), smaller than the smallest
// built-in font (5x7). Each glyph is 5 rows of 3 bits (MSB = left).
// Used for the moon's illumination %, in the zone's bottom-right
// corner, just outside the moon's outline.
// ---------------------------------------------------------------
static const uint8_t TINY_GLYPHS[11][5] = {
    {7, 5, 5, 5, 7}, // 0
    {2, 6, 2, 2, 7}, // 1
    {7, 1, 7, 4, 7}, // 2
    {7, 1, 7, 1, 7}, // 3
    {5, 5, 7, 1, 1}, // 4
    {7, 4, 7, 1, 7}, // 5
    {7, 4, 7, 5, 7}, // 6
    {7, 1, 1, 2, 2}, // 7
    {7, 5, 7, 5, 7}, // 8
    {7, 5, 7, 1, 7}, // 9
    {5, 1, 2, 4, 5}  // %
};

// Draws "0".."9" and "%" characters of `text`; 3 px per glyph + 1 px gap.
inline int16_t tinyTextWidth(const char *text) {
  int16_t n = (int16_t)strlen(text);
  return n > 0 ? n * 4 - 1 : 0;
}

inline void drawTinyText(Adafruit_GFX &d, const char *text, int16_t x,
                         int16_t y) {
  for (; *text; text++, x += 4) {
    int g = (*text == '%') ? 10 : (*text - '0');
    if (g < 0 || g > 10) continue;
    for (int row = 0; row < 5; row++) {
      for (int col = 0; col < 3; col++) {
        if (TINY_GLYPHS[g][row] & (4 >> col)) {
          d.drawPixel(x + col, y + row, qwInk());
        }
      }
    }
  }
}
// TINY-END

inline void drawZoneMoon(Adafruit_GFX &d, time_t utcNow) {
  using namespace UiZones;
  MoonData moon = calculateMoonPhase(utcNow);

  // 58 px moon centered in the 64 x 64 zone (3 px margin to the
  // separators); the illumination % is drawn in the tiny font in the
  // bottom-right corner, which stays free outside the moon's circle.
  int16_t cx = (R1_COL1_X0 + R1_COL1_X1) / 2;
  int16_t cy = (ROW1_Y0 + ROW1_Y1) / 2;
  drawMoonIcon(d, cx, cy, 58, moon);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", moonIlluminationPercent(moon));
  drawTinyText(d, buf, R1_COL1_X1 - 2 - tinyTextWidth(buf), ROW1_Y1 - 6);
}

// ---------------------------------------------------------------
// Zone 3: Info - Hijri day + month abbreviation, year, battery.
// All three lines are centered. Battery line inverts with a "!"
// once the charge is at or below LOW_BATTERY_PERCENT.
// ---------------------------------------------------------------
static const char *HIJRI_MONTH_SHORT[12] = {"Muh", "Saf", "R1",  "R2",
                                             "J1",  "J2",  "Raj", "Sha",
                                             "Ram", "Shw", "DQ",  "DH"};

inline void drawZoneInfo(Adafruit_GFX &d, const HijriDate &hijri,
                          float batteryVoltage) {
  using namespace UiZones;
  int16_t cx = (R1_COL2_X0 + R1_COL2_X1) / 2;
  d.setTextColor(qwInk());

  // Line 1: day + month abbreviation, centered as one block (size 2).
  char dayBuf[4];
  snprintf(dayBuf, sizeof(dayBuf), "%02d", hijri.day);
  const char *mon = (hijri.month >= 1 && hijri.month <= 12)
                        ? HIJRI_MONTH_SHORT[hijri.month - 1]
                        : "?";
  int16_t dayW = qwTextInkWidth((uint8_t)strlen(dayBuf), 2);
  int16_t monW = qwTextInkWidth((uint8_t)strlen(mon), 2);
  int16_t gap = 4;
  int16_t x = cx - (dayW + gap + monW) / 2;
  qwSetText(d, 2);
  d.setCursor(x, ROW1_Y0 + 6);
  d.print(dayBuf);
  d.setCursor(x + dayW + gap, ROW1_Y0 + 6);
  d.print(mon);

  // Line 2: year, centered.
  char year[8];
  snprintf(year, sizeof(year), "%04d", hijri.year);
  drawCenteredText(d, year, cx, ROW1_Y0 + 26, 2);

  // Line 3: battery, centered. Inverted with "!" when low.
  uint8_t batPct = batteryVoltageToPercent(batteryVoltage);
  bool lowBat = batPct <= LOW_BATTERY_PERCENT;
  char bat[16];
  qwSetText(d, 1);
  if (lowBat) {
    snprintf(bat, sizeof(bat), "BAT %u%%!", batPct);
    int16_t w = qwTextInkWidth((uint8_t)strlen(bat), 1);
    int16_t tx = cx - w / 2;
    int16_t ty = ROW1_Y0 + 50;
    d.fillRect(tx - 3, ty - 2, w + 6, qwTextHeight(1) + 4, qwInk());
    d.setTextColor(qwPaper());
    d.setCursor(tx, ty);
    d.print(bat);
    d.setTextColor(qwInk());
  } else {
    snprintf(bat, sizeof(bat), "BAT %u%%", batPct);
    drawCenteredText(d, bat, cx, ROW1_Y0 + 50, 1);
  }
}

// ---------------------------------------------------------------
// Zone 4: Quran bookmark
// ---------------------------------------------------------------
inline void drawZoneBookmark(Adafruit_GFX &d, const QuranBookmark &bm) {
  using namespace UiZones;
  int16_t cx = (R1_COL3_X0 + R1_COL3_X1) / 2;
  d.setTextColor(qwInk());
  char buf[16];
  snprintf(buf, sizeof(buf), "S%03u", bm.surah);
  drawCenteredText(d, buf, cx, ROW1_Y0 + 10, 2);
  snprintf(buf, sizeof(buf), ":%03u", bm.verse);
  drawCenteredText(d, buf, cx, ROW1_Y0 + 34, 2);
}

// ---------------------------------------------------------------
// Reading streak: 5 stars under the bookmark. All 5 are full right
// after the bookmark is changed (a reading); one is lost every 24 h
// (5 stars for the first 24 h, 4 up to 48 h, ... 0 after 120 h).
// Active = filled star, lost = hollow star.
// ---------------------------------------------------------------
constexpr uint8_t QW_STAR_COUNT = 5;

inline uint8_t quranStarsLeft(time_t utcNow, uint32_t lastReadUtc) {
  if (lastReadUtc == 0) return 0; // no reading recorded yet
  if ((int32_t)utcNow <= (int32_t)lastReadUtc) return QW_STAR_COUNT;
  uint32_t days = ((uint32_t)utcNow - lastReadUtc) / 86400UL;
  return days >= QW_STAR_COUNT ? 0 : (uint8_t)(QW_STAR_COUNT - days);
}

// 5-pointed star centered on (cx, cy), outer radius R, tip up.
inline void drawStar(Adafruit_GFX &d, int16_t cx, int16_t cy, int16_t R,
                     bool filled) {
  int16_t vx[10], vy[10];
  float rIn = R * 0.45f;
  for (int i = 0; i < 10; i++) {
    float a = -PI / 2 + i * PI / 5; // 36 degrees per vertex
    float r = (i % 2 == 0) ? R : rIn;
    vx[i] = cx + (int16_t)lroundf(r * cosf(a));
    vy[i] = cy + (int16_t)lroundf(r * sinf(a));
  }
  for (int i = 0; i < 10; i++) {
    int j = (i + 1) % 10;
    if (filled) {
      d.fillTriangle(cx, cy, vx[i], vy[i], vx[j], vy[j], qwInk());
    }
    d.drawLine(vx[i], vy[i], vx[j], vy[j], qwInk());
  }
}

inline void drawZoneStars(Adafruit_GFX &d, uint8_t stars) {
  using namespace UiZones;
  int16_t cx = (R1_COL3_X0 + R1_COL3_X1) / 2;
  const int16_t spacing = 12, R = 5;
  int16_t cy = ROW1_Y0 + 57;
  for (uint8_t i = 0; i < QW_STAR_COUNT; i++) {
    int16_t x = cx + (int16_t)(((int)i - 2) * spacing);
    drawStar(d, x, cy, R, i < stars);
  }
}

// ---------------------------------------------------------------
// Zone 2: Hijri 4x3 month grid. The current month is shown in an
// inverted box (white digits on a black cell).
// ---------------------------------------------------------------
inline void drawZoneHijriGrid(Adafruit_GFX &d, const HijriDate &hijri) {
  using namespace UiZones;
  int16_t cellW = (R2_COL1_X1 - R2_COL1_X0) / 3;
  int16_t cellH = (ROW2_Y1 - ROW2_Y0) / 4;

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      int monthNum = row * 3 + col + 1;
      int16_t cellX0 = R2_COL1_X0 + col * cellW;
      int16_t cellY0 = ROW2_Y0 + row * cellH;
      int16_t cellCx = cellX0 + cellW / 2;
      int16_t cellCy = cellY0 + cellH / 2;

      bool isCurrent = isCurrentHijriMonth(monthNum, hijri);
      if (isCurrent) {
        d.fillRect(cellX0 + 2, cellY0 + 2, cellW - 4, cellH - 4, qwInk());
      }

      d.setTextColor(isCurrent ? qwPaper() : qwInk());
      char buf[4];
      snprintf(buf, sizeof(buf), "%02d", monthNum);
      drawCenteredText(d, buf, cellCx, cellCy - qwTextHeight(1) / 2, 1);
    }
  }
  d.setTextColor(qwInk());
}

// ---------------------------------------------------------------
// Zone 5: Hour, with four quarter-hour dots underneath.
//   - already-passed quarters: solid black dot
//   - current quarter:         black dot with a white center (ring)
//   - upcoming quarters:       empty (outline only)
// ---------------------------------------------------------------
inline void drawZoneHour(Adafruit_GFX &d, int hour24, int minute) {
  using namespace UiZones;
  int16_t cx = (R2_COL2_X0 + R2_COL2_X1) / 2;

  d.setTextColor(qwInk());
  char buf[4];
  snprintf(buf, sizeof(buf), "%02d", hour24);
  int16_t hourTop = ROW2_Y0 + 16;
  drawCenteredText(d, buf, cx, hourTop, 5);

  const int16_t r = 6;
  const int16_t spacing = 16;
  int16_t dotsCy = ROW2_Y0 + 64;
  int filled = minute / 15 + 1;
  if (filled > 4) filled = 4;
  int current = filled - 1;

  for (int i = 0; i < 4; i++) {
    int16_t dx = cx + (int16_t)((i - 1.5f) * spacing);
    if (i < current) {
      d.fillCircle(dx, dotsCy, r, qwInk());
    } else if (i == current) {
      d.fillCircle(dx, dotsCy, r, qwInk());
      d.fillCircle(dx, dotsCy, r - 3, qwPaper());
    } else {
      d.drawCircle(dx, dotsCy, r, qwInk());
      d.drawCircle(dx, dotsCy, r - 1, qwInk());
    }
  }
}

// ---------------------------------------------------------------
// Special Islamic days, shown under the quarter-hour dots.
//   ACTIVE  (inverted box):
//     ASHURA        10 Muharram
//     ARAFAH         9 Dhul Hijjah
//     EID AL-ADHA   10 Dhul Hijjah
//     LAST 10 NIGHTS from Maghrib on 20 Ramadan (the night of the
//                    21st begins at Maghrib) until Maghrib on the
//                    last day of Ramadan
//   WARNING (plain text), the day before:
//     D-1: ASHURA   9 Muharram
//     D-1: ARAFAH   8 Dhul Hijjah
//     D-1: LAST 10  20 Ramadan, until Maghrib
//   (no warning for Eid al-Adha: it follows Arafah directly)
// The Hijri day changes at midnight (arithmetic calendar); only
// the Last 10 Nights use Maghrib, as the Islamic night begins then.
// ---------------------------------------------------------------
enum SpecialKind : uint8_t { SPECIAL_NONE = 0, SPECIAL_WARNING, SPECIAL_ACTIVE };

struct SpecialDay {
  SpecialKind kind;
  const char *line1;
  const char *line2; // nullptr = single line
};

inline SpecialDay findSpecialDay(const HijriDate &today,
                                 const HijriDate &tomorrow, int nowMinutes,
                                 int maghribMinutes) {
  SpecialDay none = {SPECIAL_NONE, nullptr, nullptr};
  int m = today.month, d = today.day;
  bool afterMaghrib = nowMinutes >= maghribMinutes;

  if (m == 1 && d == 10) return {SPECIAL_ACTIVE, "ASHURA", nullptr};
  if (m == 12 && d == 9) return {SPECIAL_ACTIVE, "ARAFAH", nullptr};
  if (m == 12 && d == 10) return {SPECIAL_ACTIVE, "EID AL-ADHA", nullptr};

  if (m == 9) {
    bool lastDayOfRamadan = (tomorrow.month != 9);
    if (d == 20 && afterMaghrib) return {SPECIAL_ACTIVE, "LAST 10", "NIGHTS"};
    if (d >= 21 && !(lastDayOfRamadan && afterMaghrib)) {
      return {SPECIAL_ACTIVE, "LAST 10", "NIGHTS"};
    }
    if (d == 20 && !afterMaghrib) {
      return {SPECIAL_WARNING, "D-1: LAST 10", nullptr};
    }
  }
  if (m == 1 && d == 9) return {SPECIAL_WARNING, "D-1: ASHURA", nullptr};
  if (m == 12 && d == 8) return {SPECIAL_WARNING, "D-1: ARAFAH", nullptr};
  return none;
}

inline void drawZoneSpecial(Adafruit_GFX &d, const SpecialDay &sp) {
  using namespace UiZones;
  if (sp.kind == SPECIAL_NONE) return;
  int16_t cx = (R2_COL2_X0 + R2_COL2_X1) / 2;
  const int16_t boxTop = 138, boxH = 20;

  qwSetText(d, 1);
  if (sp.kind == SPECIAL_ACTIVE) {
    d.fillRect(R2_COL2_X0 + 3, boxTop, (R2_COL2_X1 - R2_COL2_X0) - 5, boxH,
               qwInk());
    d.setTextColor(qwPaper());
  } else {
    d.setTextColor(qwInk());
  }

  if (sp.line2) {
    drawCenteredText(d, sp.line1, cx, boxTop + 2, 1);
    drawCenteredText(d, sp.line2, cx, boxTop + 11, 1);
  } else {
    drawCenteredText(d, sp.line1, cx, boxTop + (boxH - 8) / 2, 1);
  }
  d.setTextColor(qwInk());
}

// ---------------------------------------------------------------
// Zone 7/8/9 area: overall Quran progress only (labelled "Quran %").
// ---------------------------------------------------------------
inline void drawZoneQuranBars(Adafruit_GFX &d, const QuranBookmark &bm) {
  using namespace UiZones;
  int16_t areaX0 = R2_COL3_X0 + 6, areaX1 = R2_COL3_X1 - 6;
  int16_t barW = areaX1 - areaX0;
  int16_t barTop = ROW2_Y0 + 22, barBottom = ROW2_Y1 - 20;
  int16_t barH = barBottom - barTop;
  uint8_t percent = totalQuranPercent(bm);

  d.setTextColor(qwInk());
  drawCenteredText(d, "Quran %", areaX0 + barW / 2, ROW2_Y0 + 8, 1);
  drawVerticalBar(d, areaX0, barTop, barW, barH, percent);

  char pctBuf[6];
  snprintf(pctBuf, sizeof(pctBuf), "%u%%", percent);
  drawCenteredText(d, pctBuf, areaX0 + barW / 2, barBottom + 6, 1);
}

// ---------------------------------------------------------------
// Zone 6: Prayer times. The current prayer's cell is inverted
// (white text on a black cell).
// ---------------------------------------------------------------
inline void drawZonePrayerTimes(Adafruit_GFX &d, const PrayerTimesResult &t,
                                 PrayerIndex current) {
  using namespace UiZones;
  const char *labels[6] = {"F", "S", "D", "A", "M", "I"};
  int mins[6] = {t.fajrMin, t.shuruqMin, t.dhuhrMin, t.asrMin, t.maghribMin,
                 t.ishaMin};

  const int16_t xs[4] = {0, W / 3, 2 * W / 3, W - 1};
  const int16_t ys[3] = {ROW3_Y0, ROW3_Y0 + (ROW3_Y1 - ROW3_Y0) / 2, H - 1};

  for (int i = 0; i < 6; i++) {
    int col = i % 3;
    int row = i / 3;
    int16_t x0 = xs[col], x1 = xs[col + 1];
    int16_t y0 = ys[row], y1 = ys[row + 1];

    bool isCurrent = ((int)current == i);
    if (isCurrent) {
      d.fillRect(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, qwInk());
    }

    int h, m;
    minutesToHM(mins[i], h, m);
    char buf[10];
    snprintf(buf, sizeof(buf), "%s %02d:%02d", labels[i], h, m);

    qwSetText(d, 1);
    d.setTextColor(isCurrent ? qwPaper() : qwInk());
    int16_t textW = qwTextInkWidth((uint8_t)strlen(buf), 1);
    d.setCursor(x0 + (x1 - x0 - textW) / 2,
                y0 + (y1 - y0 - qwTextHeight(1)) / 2);
    d.print(buf);
  }
  d.setTextColor(qwInk());
}

// ---------------------------------------------------------------
// Separators between all zones
// ---------------------------------------------------------------
inline void drawSeparators(Adafruit_GFX &d) {
  using namespace UiZones;
  d.drawFastHLine(0, ROW1_Y1, W, qwInk());
  d.drawFastHLine(0, ROW2_Y1, W, qwInk());
  d.drawFastVLine(R1_COL2_X0, ROW1_Y0, ROW1_Y1 - ROW1_Y0, qwInk());
  d.drawFastVLine(R1_COL3_X0, ROW1_Y0, ROW1_Y1 - ROW1_Y0, qwInk());
  d.drawFastVLine(R2_COL2_X0, ROW2_Y0, ROW2_Y1 - ROW2_Y0, qwInk());
  d.drawFastVLine(R2_COL3_X0, ROW2_Y0, ROW2_Y1 - ROW2_Y0, qwInk());
  d.drawFastVLine(W / 3, ROW3_Y0, ROW3_Y1 - ROW3_Y0, qwInk());
  d.drawFastVLine(2 * W / 3, ROW3_Y0, ROW3_Y1 - ROW3_Y0, qwInk());
  d.drawFastHLine(0, ROW3_Y0 + (ROW3_Y1 - ROW3_Y0) / 2, W, qwInk());
  d.drawRect(0, 0, W, H, qwInk()); // outer border
}

// ---------------------------------------------------------------
// Top-level entry point
// ---------------------------------------------------------------
inline void drawMainFace(Adafruit_GFX &d, const AppSettings &settings,
                          const QuranBookmark &bookmark, int gYear, int gMonth,
                          int gDay, int hour24, int minute, time_t utcNow,
                          float batteryVoltage, uint8_t stars) {
  d.fillScreen(qwPaper());

  HijriDate hijri =
      gregorianToHijri(gYear, gMonth, gDay, settings.hijriAdjustment);
  PrayerTimesResult prayers = calculatePrayerTimes(gYear, gMonth, gDay, settings);
  int nowMinutes = hour24 * 60 + minute;
  PrayerIndex current = currentPrayerIndex(nowMinutes, prayers);

  drawZoneMoon(d, utcNow);
  drawZoneInfo(d, hijri, batteryVoltage);
  drawZoneBookmark(d, bookmark);
  drawZoneStars(d, stars);
  drawZoneHijriGrid(d, hijri);
  drawZoneHour(d, hour24, minute);
  HijriDate hijriTomorrow = jdnToHijri(
      gregorianToJDN(gYear, gMonth, gDay) + 1, settings.hijriAdjustment);
  drawZoneSpecial(d, findSpecialDay(hijri, hijriTomorrow, nowMinutes,
                                    prayers.maghribMin));
  drawZoneQuranBars(d, bookmark);
  drawZonePrayerTimes(d, prayers, current);
  drawSeparators(d);
}