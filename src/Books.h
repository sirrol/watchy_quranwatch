#pragma once
/*
  Books.h
  ------------------------------------------------------------
  BOOKMARKS - a small app of the APPS menu: keep the page you reached in
  the books you are reading (up to 12 books, 3 per screen).

  For every book: a title (12 characters), the current page, an optional
  total number of pages (shows a percentage and a progress bar) and the
  date of the last reading ("Read: 3 days ago", updated whenever the page
  changes).

  LIST      UP/DOWN choose a book (or "+ ADD A BOOK"), MENU opens it.
  BOOK      SET PAGE / SET TOTAL PAGES / RENAME / DELETE / BACK.
  NUMBER    UP/DOWN change the value (hold = faster); MENU cycles the step
            (1, 10, 100) and then leaves.
  TITLE     UP/DOWN choose a letter, MENU validates it; choose ">" then MENU
            to finish early.

  Everything is saved in flash at every change (one small blob), so BACK
  can leave at any time. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "Storage.h"
#include "NotoMono13.h"

namespace Bk {

constexpr int MAXB = 12, PER = 3, TL = 12, NCH = 40;
static const char CS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.'";   // NCH characters
static const uint16_t STEPS[4] = {1, 10, 100, 1};

struct Book { char title[TL + 1]; uint16_t page, total, last; };
struct Store { uint8_t n; Book b[MAXB]; };
enum Ph : uint8_t { PH_LIST = 0, PH_BMENU, PH_NUM, PH_TITLE, PH_DEL };

RTC_DATA_ATTR uint8_t phase = PH_LIST, sel = 0, mSel = 0, numWhich = 0, stepIdx = 0, tpos = 0, tchar = 0, eIdx = 0;
RTC_DATA_ATTR char tbuf[TL + 1];

static Store st;
static bool ld = false;

inline void ensure() {
  if (ld) return;
  memset(&st, 0, sizeof(st));
  Store t;
  memset(&t, 0, sizeof(t));
  if (QWStorage::loadBlob("bookS", &t, sizeof(t)) && t.n <= MAXB) st = t;
  ld = true;
}
inline void save() { QWStorage::saveBlob("bookS", &st, sizeof(st)); }
inline int items() { return st.n + (st.n < MAXB ? 1 : 0); }
inline void clampSel() { int it = items(); if (sel >= it) sel = (uint8_t)(it - 1); }

inline void agoText(char *b, size_t n, uint16_t last) {
  uint32_t today = qwGameDay;
  if (today == 0) { b[0] = 0; return; }
  uint32_t d = today > last ? today - last : 0;
  if (d == 0) snprintf(b, n, "Read: today");
  else if (d == 1) snprintf(b, n, "Read: yesterday");
  else if (d < 14) snprintf(b, n, "Read: %lu days ago", (unsigned long)d);
  else if (d < 60) snprintf(b, n, "Read: %lu weeks ago", (unsigned long)(d / 7));
  else snprintf(b, n, "Read: %lu months ago", (unsigned long)(d / 30));
}
inline void pageText(char *b, size_t n, const Book &k) {
  if (k.total > 0) {
    unsigned pc = (unsigned)((uint32_t)k.page * 100 / k.total);
    if (pc > 100) pc = 100;
    snprintf(b, n, "Page %u / %u  %u%%", k.page, k.total, pc);
  } else snprintf(b, n, "Page %u", k.page);
}

// ---------------------------------------------------------------- actions
inline void start() {
  ensure();
  phase = PH_LIST; sel = 0;
  uint16_t best = 0;
  for (int i = 0; i < st.n; i++) if (st.b[i].last >= best) { best = st.b[i].last; sel = (uint8_t)i; }
}
inline bool busy() { return true; }   // leaving the app always asks first
// BACK inside the app: go up one level. false = already at the list (leave).
inline bool back() {
  ensure();
  switch (phase) {
    case PH_BMENU: phase = PH_LIST; return true;
    case PH_NUM: case PH_DEL: phase = PH_BMENU; return true;
    case PH_TITLE: phase = (eIdx == 255) ? PH_LIST : PH_BMENU; return true;
    default: return false;
  }
}

inline void startTitle(uint8_t idx) {
  eIdx = idx; tbuf[0] = 0; tpos = 0; tchar = 0; phase = PH_TITLE;
}
inline void finishTitle() {
  int n = (int)strlen(tbuf);
  while (n > 0 && tbuf[n - 1] == ' ') tbuf[--n] = 0;
  if (n == 0) snprintf(tbuf, sizeof(tbuf), "BOOK");
  if (eIdx == 255) {
    if (st.n >= MAXB) { phase = PH_LIST; return; }
    Book &k = st.b[st.n];
    memset(&k, 0, sizeof(k));
    memcpy(k.title, tbuf, TL); k.title[TL] = 0;
    k.last = (uint16_t)qwGameDay;
    sel = st.n; st.n++;
    save();
    numWhich = 0; stepIdx = 0; phase = PH_NUM;
  } else {
    memcpy(st.b[sel].title, tbuf, TL); st.b[sel].title[TL] = 0;
    save();
    phase = PH_BMENU;
  }
}

inline uint16_t &numRef() { return numWhich ? st.b[sel].total : st.b[sel].page; }
inline void numChange(int dir) {
  int s = qwGameSteps();
  uint32_t step = (uint32_t)STEPS[stepIdx] * s;
  uint16_t &v = numRef();
  uint16_t old = v;
  if (dir > 0) v = (uint16_t)(v + step > 9999 ? 9999 : v + step);
  else v = (uint16_t)(v > step ? v - step : 0);
  if (v != old) {
    if (!numWhich) st.b[sel].last = (uint16_t)qwGameDay;
    save();
  }
}

inline void up() {
  ensure();
  int s = qwGameSteps();
  if (phase == PH_LIST) {
    int it = items();
    if (sel >= s) sel -= (uint8_t)s; else sel = (s == 1) ? (uint8_t)(it - 1) : 0;
  } else if (phase == PH_BMENU) mSel = (uint8_t)((mSel + 4) % 5);
  else if (phase == PH_NUM) numChange(+1);
  else if (phase == PH_TITLE) tchar = (uint8_t)((tchar + s) % (NCH + 1));
  else if (phase == PH_DEL) mSel ^= 1;
}
inline void down() {
  ensure();
  int s = qwGameSteps();
  if (phase == PH_LIST) {
    int it = items();
    if (sel + s < it) sel += (uint8_t)s; else sel = (s == 1 && sel == it - 1) ? 0 : (uint8_t)(it - 1);
  } else if (phase == PH_BMENU) mSel = (uint8_t)((mSel + 1) % 5);
  else if (phase == PH_NUM) numChange(-1);
  else if (phase == PH_TITLE) tchar = (uint8_t)((tchar + (NCH + 1) - (s % (NCH + 1))) % (NCH + 1));
  else if (phase == PH_DEL) mSel ^= 1;
}
inline void select() {
  ensure();
  if (phase == PH_LIST) {
    clampSel();
    if (sel < st.n) { mSel = 0; phase = PH_BMENU; }
    else startTitle(255);
  } else if (phase == PH_BMENU) {
    if (mSel == 0) { numWhich = 0; stepIdx = 0; phase = PH_NUM; }
    else if (mSel == 1) { numWhich = 1; stepIdx = 0; phase = PH_NUM; }
    else if (mSel == 2) startTitle(sel);
    else if (mSel == 3) { mSel = 1; phase = PH_DEL; }
    else phase = PH_LIST;
  } else if (phase == PH_NUM) {
    if (stepIdx < 3) stepIdx++; else { phase = PH_BMENU; }
  } else if (phase == PH_TITLE) {
    if (tchar >= NCH) { finishTitle(); return; }
    tbuf[tpos++] = CS[tchar]; tbuf[tpos] = 0; tchar = 0;
    if (tpos >= TL) finishTitle();
  } else if (phase == PH_DEL) {
    if (mSel == 0) {
      for (int i = sel; i + 1 < st.n; i++) st.b[i] = st.b[i + 1];
      if (st.n) { st.n--; memset(&st.b[st.n], 0, sizeof(Book)); }
      save();
      clampSel();
      phase = PH_LIST;
    } else { mSel = 0; phase = PH_BMENU; }
  }
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d, int size = 1) { d.setTextSize(size); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
inline void txt(Adafruit_GFX &d, const char *s, int16_t x, int16_t base) { d.setCursor(x, base); d.print(s); }
inline void centered(Adafruit_GFX &d, const char *s, int16_t base) { txt(d, s, (200 - 8 * (int)strlen(s)) / 2, base); }
inline void header(Adafruit_GFX &d, const char *l, const char *r) {
  setup13(d);
  if (l) txt(d, l, 6, 12);
  if (r) txt(d, r, 194 - 8 * (int)strlen(r), 12);
  d.drawFastHLine(4, 15, 192, qwInk());
}
inline void bar(Adafruit_GFX &d, const char *s, int16_t y, int16_t h, bool inv) {
  if (inv) d.fillRect(4, y, 192, h, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  centered(d, s, y + h - 6);
  d.setTextColor(qwInk());
}
inline void drawBlock(Adafruit_GFX &d, int idx, int16_t y0, bool inv) {
  uint16_t fg = inv ? qwPaper() : qwInk();
  if (inv) d.fillRect(4, y0, 192, 56, qwInk());
  d.setTextColor(fg);
  if (idx >= st.n) {
    centered(d, "+ ADD A BOOK", y0 + 33);
  } else {
    const Book &k = st.b[idx];
    char b[32];
    txt(d, k.title, 8, y0 + 14);
    pageText(b, sizeof(b), k);
    txt(d, b, 8, y0 + 29);
    agoText(b, sizeof(b), k.last);
    txt(d, b, 8, y0 + 44);
    if (k.total > 0) {
      uint32_t pc = (uint32_t)k.page * 100 / k.total; if (pc > 100) pc = 100;
      d.drawRect(8, y0 + 49, 184, 5, fg);
      d.fillRect(9, y0 + 50, (int16_t)(182 * pc / 100), 3, fg);
    }
  }
  d.setTextColor(qwInk());
}

inline void drawList(Adafruit_GFX &d) {
  int it = items();
  int pages = (it + PER - 1) / PER, pg = sel / PER;
  char r[10];
  snprintf(r, sizeof(r), "%d/%d", pg + 1, pages);
  header(d, "BOOKMARKS", r);
  setup13(d);
  for (int k = 0; k < PER; k++) {
    int i = pg * PER + k;
    if (i >= it) break;
    drawBlock(d, i, 20 + k * 59, i == sel);
  }
  if (st.n == 0) {
    centered(d, "Track the page you", 100);
    centered(d, "reached in the books", 116);
    centered(d, "you are reading.", 132);
  }
}
inline void drawBMenu(Adafruit_GFX &d) {
  const Book &k = st.b[sel];
  header(d, k.title, nullptr);
  setup13(d);
  char b[32];
  pageText(b, sizeof(b), k); txt(d, b, 8, 34);
  agoText(b, sizeof(b), k.last); txt(d, b, 8, 50);
  static const char *const L[5] = {"SET PAGE", "SET TOTAL PAGES", "RENAME", "DELETE", "BACK"};
  for (int i = 0; i < 5; i++) bar(d, L[i], 66 + i * 26, 23, i == mSel);
}
inline void drawNum(Adafruit_GFX &d) {
  header(d, numWhich ? "TOTAL PAGES" : "PAGE", nullptr);
  setup13(d);
  centered(d, st.b[sel].title, 36);
  char b[8];
  snprintf(b, sizeof(b), "%u", numRef());
  d.setFont(nullptr);
  qwSetText(d, 5);
  d.setTextColor(qwInk());
  d.setCursor((200 - 30 * (int)strlen(b)) / 2, 54);
  d.print(b);
  setup13(d);
  if (numWhich) centered(d, "0 = unknown", 112);
  static const char *const S[4] = {"STEP 1", "STEP 10", "STEP 100", "DONE"};
  bar(d, S[stepIdx], 124, 24, true);
  centered(d, "UP/DOWN: change value", 168);
  centered(d, stepIdx < 3 ? "MENU: next step" : "MENU: finish", 186);
}
inline void drawTitle(Adafruit_GFX &d) {
  header(d, eIdx == 255 ? "NEW BOOK" : "RENAME", nullptr);
  setup13(d, 2);
  for (int i = 0; i < TL; i++) {
    int16_t x = 4 + i * 16;
    char c[2] = {0, 0};
    bool cur = (i == tpos);
    if (i < tpos) c[0] = tbuf[i];
    else if (cur) c[0] = tchar < NCH ? CS[tchar] : '>';
    if (cur) d.fillRect(x, 40, 16, 26, qwInk());
    d.setTextColor(cur ? qwPaper() : qwInk());
    if (c[0]) { d.setCursor(x, 60); d.print(c); }
    d.drawFastHLine(x + 1, 70, 14, qwInk());
  }
  setup13(d);
  centered(d, "UP/DOWN: change letter", 104);
  centered(d, "MENU: validate letter", 122);
  centered(d, "Pick '>' then MENU", 148);
  centered(d, "to finish the title.", 164);
}
inline void drawDel(Adafruit_GFX &d) {
  header(d, "DELETE BOOK?", nullptr);
  setup13(d);
  centered(d, st.b[sel].title, 50);
  centered(d, "This cannot be undone.", 76);
  bar(d, "YES, DELETE", 96, 24, mSel == 0);
  bar(d, "NO", 126, 24, mSel == 1);
}
inline void draw(Adafruit_GFX &d) {
  ensure();
  clampSel();
  d.fillScreen(qwPaper());
  switch (phase) {
    case PH_BMENU: if (sel < st.n) drawBMenu(d); else drawList(d); break;
    case PH_NUM: if (sel < st.n) drawNum(d); else drawList(d); break;
    case PH_TITLE: drawTitle(d); break;
    case PH_DEL: if (sel < st.n) drawDel(d); else drawList(d); break;
    default: drawList(d);
  }
  d.setFont(nullptr);
}

}  // namespace Bk
