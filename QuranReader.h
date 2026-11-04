#pragma once
/*
  QuranReader.h
  ------------------------------------------------------------
  QURAN READER: Juz Amma (surahs 78 to 114) and Ayat al-Kursi
  (2:255), displayed in Arabic.

  The words are one-bit images stored in QuranReaderData.h (see that
  file for the text source and licenses). This file lays them out like
  a mushaf page: the verses of a surah run on continuously, right to
  left, with lines justified, a small Basmala on its own line at the
  top, and a ring with the verse number at the end of every verse.
  The pages are cut from the start of the surah, so a page always
  holds as many verses as fit on the screen.

  Units: 0..36 = surah 78..114, 37 = Ayat al-Kursi (a single verse,
  shown as 255, without marker).
  Verses are numbered like in the Quran (1..n).
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "QuranReaderData.h"

namespace QReader {

constexpr uint8_t UNIT_KURSI = QR_UNIT_COUNT - 1;

constexpr int16_t HEADER_H = 15;               // header text + rule
constexpr int16_t AREA_X0 = 4, AREA_W = 192;    // text area, left edge/width
constexpr int16_t AREA_TOP = 19, AREA_H = 178;  // text area, top/height
constexpr int16_t SPACE = 7;                    // normal gap between words
constexpr int16_t MARK_GAP = 4;                 // gap before a verse marker
constexpr int16_t MAX_GAP = 18;                 // widest gap when justifying
constexpr int16_t LINE_GAP = 1;
constexpr int16_t MIN_ASC = 27, MIN_DESC = 12;  // keeps line pitch even

constexpr uint16_t MAX_WORDS = QR_MAX_UNIT_WORDS;
constexpr uint8_t MAX_LINES = 120;
constexpr uint8_t MAX_VERSES = QR_MAX_VERSES;

struct Layout {
  uint16_t ids[MAX_WORDS];
  uint16_t nWords;
  uint16_t verseWord[MAX_VERSES + 2];  // [v] = index of the first word of verse v ([nVerses + 1] = end)
  uint16_t lineFirst[MAX_LINES];       // index of the first word of the line
  uint8_t lineCount[MAX_LINES];        // number of words
  bool lineCenter[MAX_LINES];          // true for the Basmala
  bool lineJustify[MAX_LINES];         // false for the last line of a paragraph
  uint8_t linePage[MAX_LINES];
  uint16_t lineBase[MAX_LINES];        // baseline y inside the text area
  uint8_t nLines;
  uint8_t nPages;
  uint8_t nVerses;
};

inline bool isMarker(uint16_t id) { return id >= QR_MARK_FIRST; }

inline uint16_t verseCount(uint8_t unit) { return QR_UNIT_VERSES[unit]; }

// Number shown to the user for a verse of a unit.
inline uint16_t verseLabel(uint8_t unit, uint16_t verse) {
  return unit == UNIT_KURSI ? 255 : verse;
}

inline uint8_t unitSurah(uint8_t unit) { return QR_UNIT_SURAH[unit]; }

inline void clampPosition(uint8_t &unit, uint16_t &verse) {
  if (unit >= QR_UNIT_COUNT) unit = 0;
  uint16_t n = verseCount(unit);
  if (verse < 1) verse = 1;
  if (verse > n) verse = n;
}

inline void addLine(Layout &L, uint16_t first, uint8_t count, bool center,
                    bool justify) {
  if (L.nLines >= MAX_LINES) return;
  L.lineFirst[L.nLines] = first;
  L.lineCount[L.nLines] = count;
  L.lineCenter[L.nLines] = center;
  L.lineJustify[L.nLines] = justify;
  L.nLines++;
}

// Width of the word at index i together with its verse marker, if the
// next word is one (a marker never starts a line: it stays with the
// word before it). `span` receives the number of words taken.
inline int16_t unitWidth(const Layout &L, uint16_t i, uint16_t to,
                         uint8_t &span) {
  int16_t w = QR_WORD_W[L.ids[i]];
  span = 1;
  if (i + 1 < to && isMarker(L.ids[i + 1])) {
    w += MARK_GAP + QR_WORD_W[L.ids[i + 1]];
    span = 2;
  }
  return w;
}

// Cuts words [from, to) of L.ids into lines of at most AREA_W pixels.
inline void wrapParagraph(Layout &L, uint16_t from, uint16_t to) {
  uint16_t i = from;
  while (i < to && L.nLines < MAX_LINES) {
    uint16_t first = i;
    uint8_t span;
    int16_t width = unitWidth(L, i, to, span);
    i += span;
    while (i < to) {
      int16_t w = unitWidth(L, i, to, span);
      if (width + SPACE + w > AREA_W) break;
      width += SPACE + w;
      i += span;
    }
    // Safety net: never leave the line table without room for the rest.
    if (L.nLines == MAX_LINES - 1) i = to;
    addLine(L, first, (uint8_t)(i - first), false, i < to);
  }
}

// The Basmala is centered; if it ever does not fit on one line it is
// cut in two halves.
inline void layoutBasmala(Layout &L, uint16_t from, uint16_t to) {
  int16_t width = 0;
  for (uint16_t i = from; i < to; i++) width += QR_WORD_W[L.ids[i]];
  width += SPACE * (to - from - 1);
  if (width <= AREA_W || to - from < 2) {
    addLine(L, from, (uint8_t)(to - from), true, false);
  } else {
    uint16_t mid = from + (to - from) / 2;
    addLine(L, from, (uint8_t)(mid - from), true, false);
    addLine(L, mid, (uint8_t)(to - mid), true, false);
  }
}

// Lays out the whole unit (every verse) and cuts it into pages.
inline void buildLayout(uint8_t unit, Layout &L) {
  if (unit >= QR_UNIT_COUNT) unit = 0;
  L.nWords = 0;
  L.nLines = 0;
  L.nVerses = (uint8_t)verseCount(unit);

  uint16_t basmalaEnd = 0;
  if (unit != UNIT_KURSI) {
    for (uint8_t k = 0; k < QR_BASMALA_WORDS; k++)
      L.ids[L.nWords++] = QR_BASMALA[k];
    basmalaEnd = L.nWords;
  }
  for (uint16_t v = 1; v <= L.nVerses && v <= MAX_VERSES; v++) {
    uint16_t k = QR_UNIT_FIRST_VERSE[unit] + v - 1;
    L.verseWord[v] = (v == 1) ? basmalaEnd : L.nWords;
    for (uint16_t j = QR_VERSE_START[k]; j < QR_VERSE_START[k + 1]; j++) {
      if (L.nWords < MAX_WORDS) L.ids[L.nWords++] = QR_VERSE_WORD[j];
    }
  }
  L.verseWord[L.nVerses + 1] = L.nWords;

  if (basmalaEnd) layoutBasmala(L, 0, basmalaEnd);
  wrapParagraph(L, basmalaEnd, L.nWords);

  // Pagination, with the baseline of every line.
  uint8_t page = 0;
  int16_t y = 0;
  for (uint8_t i = 0; i < L.nLines; i++) {
    int16_t asc = MIN_ASC, desc = MIN_DESC;
    for (uint8_t w = 0; w < L.lineCount[i]; w++) {
      uint16_t id = L.ids[L.lineFirst[i] + w];
      int16_t a = QR_WORD_ASC[id];
      int16_t dsc = (int16_t)QR_WORD_H[id] - a;
      if (a > asc) asc = a;
      if (dsc > desc) desc = dsc;
    }
    if (y > 0 && y + asc + desc > AREA_H) {
      page++;
      y = 0;
    }
    L.linePage[i] = page;
    L.lineBase[i] = (uint16_t)(y + asc);
    y += asc + desc + LINE_GAP;
  }
  L.nPages = (uint8_t)(page + 1);
}

// Verse number that word `w` belongs to.
inline uint16_t verseOfWord(const Layout &L, uint16_t w) {
  uint16_t v = 1;
  while (v < L.nVerses && L.verseWord[v + 1] <= w) v++;
  return v;
}

inline uint8_t firstLineOfPage(const Layout &L, uint8_t page) {
  for (uint8_t i = 0; i < L.nLines; i++)
    if (L.linePage[i] == page) return i;
  return 0;
}

inline uint8_t lastLineOfPage(const Layout &L, uint8_t page) {
  uint8_t last = 0;
  for (uint8_t i = 0; i < L.nLines; i++)
    if (L.linePage[i] == page) last = i;
  return last;
}

// Page that shows the beginning of `verse`.
inline uint8_t pageOfVerse(const Layout &L, uint16_t verse) {
  if (verse < 1) verse = 1;
  if (verse > L.nVerses) verse = L.nVerses;
  uint16_t w = L.verseWord[verse];
  for (uint8_t i = 0; i < L.nLines; i++) {
    if (w < (uint16_t)(L.lineFirst[i] + L.lineCount[i])) return L.linePage[i];
  }
  return (uint8_t)(L.nPages - 1);
}

// First verse that is on `page` (the one the page starts in).
inline uint16_t firstVerseOfPage(const Layout &L, uint8_t page) {
  return verseOfWord(L, L.lineFirst[firstLineOfPage(L, page)]);
}

inline uint8_t pageOfPosition(uint8_t unit, uint16_t verse) {
  Layout L;
  buildLayout(unit, L);
  return pageOfVerse(L, verse);
}

inline void drawWord(Adafruit_GFX &d, uint16_t id, int16_t x,
                     int16_t baseline) {
  const int16_t w = QR_WORD_W[id], h = QR_WORD_H[id];
  const int16_t top = baseline - QR_WORD_ASC[id];
  const int16_t rowBytes = (w + 7) >> 3;
  const uint8_t *p = &QR_WORD_BITS[QR_WORD_OFF[id]];
  const uint16_t ink = qwInk();
  for (int16_t r = 0; r < h; r++, p += rowBytes) {
    for (int16_t b = 0; b < rowBytes; b++) {
      uint8_t bits = p[b];
      if (!bits) continue;
      for (int16_t k = 0; k < 8; k++) {
        if (bits & (0x80 >> k)) d.drawPixel(x + b * 8 + k, top + r, ink);
      }
    }
  }
}

inline void drawLine(Adafruit_GFX &d, const Layout &L, uint8_t line,
                     int16_t baseline) {
  const uint8_t n = L.lineCount[line];
  const uint16_t first = L.lineFirst[line];

  int16_t total = 0;
  int16_t markGaps = 0, normalGaps = 0;
  for (uint8_t i = 0; i < n; i++) {
    uint16_t id = L.ids[first + i];
    total += QR_WORD_W[id];
    if (i > 0) {
      if (isMarker(id)) markGaps++;
      else normalGaps++;
    }
  }

  int16_t gap = SPACE;
  if (normalGaps > 0 && L.lineJustify[line]) {
    int16_t g = (AREA_W - total - markGaps * MARK_GAP) / normalGaps;
    if (g <= MAX_GAP) gap = g;
  }
  int16_t used = total + markGaps * MARK_GAP + normalGaps * gap;
  int16_t x = AREA_X0 + AREA_W;
  if (L.lineCenter[line]) x -= (AREA_W - used) / 2;

  for (uint8_t i = 0; i < n; i++) {
    uint16_t id = L.ids[first + i];
    if (i > 0) x -= isMarker(id) ? MARK_GAP : gap;
    x -= QR_WORD_W[id];
    drawWord(d, id, x, AREA_TOP + baseline);
  }
}

// Header: where we are (left) and the page (right).
inline void drawHeader(Adafruit_GFX &d, uint8_t unit, uint16_t v1,
                       uint16_t v2, uint8_t page, uint8_t nPages) {
  d.setTextColor(qwInk());
  qwSetText(d, 1);
  char buf[24];
  if (unit == UNIT_KURSI) {
    snprintf(buf, sizeof(buf), "AYAT AL-KURSI 2:255");
  } else if (v1 == v2) {
    snprintf(buf, sizeof(buf), "%u:%u", (unsigned)unitSurah(unit),
             (unsigned)v1);
  } else {
    snprintf(buf, sizeof(buf), "%u:%u-%u", (unsigned)unitSurah(unit),
             (unsigned)v1, (unsigned)v2);
  }
  d.setCursor(4, 3);
  d.print(buf);

  snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(page + 1), (unsigned)nPages);
  d.setCursor(196 - qwTextWidth((uint8_t)strlen(buf), 1) + 1, 3);
  d.print(buf);
  d.drawFastHLine(0, HEADER_H - 1, 200, qwInk());
}

inline void drawPage(Adafruit_GFX &d, uint8_t unit, uint8_t page) {
  if (unit >= QR_UNIT_COUNT) unit = 0;
  Layout L;
  buildLayout(unit, L);
  if (page >= L.nPages) page = L.nPages - 1;

  d.fillScreen(qwPaper());
  uint8_t first = firstLineOfPage(L, page), last = lastLineOfPage(L, page);
  uint16_t v1 = firstVerseOfPage(L, page);
  uint16_t v2 = verseOfWord(L, L.lineFirst[last] + L.lineCount[last] - 1);
  (void)first;
  drawHeader(d, unit, v1, v2, page, L.nPages);
  for (uint8_t i = 0; i < L.nLines; i++) {
    if (L.linePage[i] == page) drawLine(d, L, i, (int16_t)L.lineBase[i]);
  }
}

// Opens the reader at `verse`: returns the page that starts it.
inline void openAt(uint8_t &unit, uint16_t &verse, uint8_t &page) {
  clampPosition(unit, verse);
  page = pageOfPosition(unit, verse);
}

// dir = +1: next page, dir = -1: previous page (the button handler
// maps DOWN to +1 and UP to -1). Stops at the ends of the surah. `verse` follows: it is the first verse that is
// on the new page.
inline void step(uint8_t &unit, uint16_t &verse, uint8_t &page, int8_t dir) {
  if (unit >= QR_UNIT_COUNT) unit = 0;
  Layout L;
  buildLayout(unit, L);
  if (page >= L.nPages) page = L.nPages - 1;
  if (dir > 0 && page + 1 < L.nPages) page++;
  else if (dir < 0 && page > 0) page--;
  verse = firstVerseOfPage(L, page);
}

}  // namespace QReader