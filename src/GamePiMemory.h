#pragma once
/*
  GamePiMemory.h
  ------------------------------------------------------------
  PI MEMORY - train your memory on the first 1000 decimals of Pi
  (part of the GAMES menu, solo only).

  Four modes:
    LEARN         : read the decimals, 50 per page, in groups of 5
                    (DOWN = next page, UP = previous page, hold = faster)
    RECITE        : type the decimals from the first one, up to the first
                    mistake. Your best run is saved.
    FROM DIGIT N  : choose where to start (multiples of 10); the 10
                    decimals before it are shown as a reminder, then type
                    on from there.
    10-DIGIT BLOCK: a block of 10 decimals is shown, you hide it with MENU
                    and type it from memory. A perfect block moves you on to
                    the next one; otherwise you see it again.

  Typing a decimal: UP / DOWN turn the digit 0-9 (hold = faster), MENU
  confirms it. The last digit you typed is the starting point of the next
  one. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "Storage.h"

namespace Pm {

constexpr int N = 1000;
static const char PI_DIGITS[N + 1] =
    "14159265358979323846264338327950288419716939937510"
    "58209749445923078164062862089986280348253421170679"
    "82148086513282306647093844609550582231725359408128"
    "48111745028410270193852110555964462294895493038196"
    "44288109756659334461284756482337867831652712019091"
    "45648566923460348610454326648213393607260249141273"
    "72458700660631558817488152092096282925409171536436"
    "78925903600113305305488204665213841469519415116094"
    "33057270365759591953092186117381932611793105118548"
    "07446237996274956735188575272489122793818301194912"
    "98336733624406566430860213949463952247371907021798"
    "60943702770539217176293176752384674818467669405132"
    "00056812714526356082778577134275778960917363717872"
    "14684409012249534301465495853710507922796892589235"
    "42019956112129021960864034418159813629774771309960"
    "51870721134999999837297804995105973173281609631859"
    "50244594553469083026425223082533446850352619311881"
    "71010003137838752886587533208381420617177669147303"
    "59825349042875546873115956286388235378759375195778"
    "18577805321712268066130019278766111959092164201989";

inline int dg(int i) { return PI_DIGITS[i] - '0'; }

enum Phase : uint8_t { PH_MENU = 0, PH_LEARN, PH_START, PH_IN, PH_FAIL, PH_BSHOW, PH_BTYPE, PH_BRES };
enum Mode : uint8_t { M_LEARN = 0, M_RECITE, M_FROM, M_BLOCK };

RTC_DATA_ATTR uint8_t phase = PH_MENU;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t mode = M_RECITE;
RTC_DATA_ATTR uint16_t page = 0;       // LEARN page (50 decimals each)
RTC_DATA_ATTR uint16_t startPos = 0;   // first decimal to type (0 = the first)
RTC_DATA_ATTR uint16_t pos = 0;        // decimals typed so far
RTC_DATA_ATTR uint8_t cur = 0;         // digit being chosen
RTC_DATA_ATTR uint8_t wrongGot = 0;    // typed digit that was wrong
RTC_DATA_ATTR uint8_t newRecord = 0;
RTC_DATA_ATTR uint16_t best = 0;       // best RECITE run (saved)
RTC_DATA_ATTR uint16_t blockBest = 0;  // blocks cleared, 0..100 (saved)
RTC_DATA_ATTR uint16_t blk = 0;        // block being trained
RTC_DATA_ATTR uint8_t bpos = 0;        // decimals typed in the block
RTC_DATA_ATTR uint8_t btyped[10];

struct Rec {
  uint16_t best, blockBest;
};

inline void loadRec() {
  Rec r;
  if (QWStorage::loadBlob("piMem", &r, sizeof(r)) && r.best <= N && r.blockBest <= N / 10) {
    best = r.best;
    blockBest = r.blockBest;
  }
}

inline void saveRec() {
  Rec r = {best, blockBest};
  QWStorage::saveBlob("piMem", &r, sizeof(r));
}

inline bool busy() { return phase == PH_IN || phase == PH_BTYPE; }

inline void start() {
  loadRec();
  phase = PH_MENU;
  menuSel = 1;
}

inline void beginRun(int from) {
  startPos = (uint16_t)from;
  pos = (uint16_t)from;
  cur = (from > 0) ? (uint8_t)dg(from - 1) : 0;
  newRecord = 0;
  phase = PH_IN;
}

inline void bump(uint8_t &v, int delta, int mod) {
  v = (uint8_t)(((int)v + delta % mod + mod) % mod);
}

// ---------------- buttons ----------------
// DOWN = next page / next block / later start, UP = the previous one.
inline int wrapAdd(int v, int delta, int count) { return ((v + delta) % count + count) % count; }

inline void move(int dir) {  // dir = +1 (DOWN) or -1 (UP)
  int s = qwGameSteps();
  switch (phase) {
    case PH_LEARN: page = (uint16_t)wrapAdd(page, dir * s, N / 50); break;
    case PH_START: startPos = (uint16_t)(10 * (1 + wrapAdd(startPos / 10 - 1, dir * s, N / 10 - 1))); break;
    case PH_BSHOW: blk = (uint16_t)wrapAdd(blk, dir * s, N / 10); break;
    default: break;
  }
}

inline void up() {
  switch (phase) {
    case PH_MENU: menuSel = (menuSel + 3) % 4; break;
    case PH_IN: case PH_BTYPE: bump(cur, qwGameSteps(), 10); break;
    default: move(-1); break;
  }
}

inline void down() {
  switch (phase) {
    case PH_MENU: menuSel = (menuSel + 1) % 4; break;
    case PH_IN: case PH_BTYPE: bump(cur, -qwGameSteps(), 10); break;
    default: move(+1); break;
  }
}

inline void select() {
  switch (phase) {
    case PH_MENU:
      mode = menuSel;
      if (mode == M_LEARN) phase = PH_LEARN;
      else if (mode == M_RECITE) beginRun(0);
      else if (mode == M_FROM) { startPos = startPos < 10 ? 10 : startPos; phase = PH_START; }
      else phase = PH_BSHOW;
      break;
    case PH_LEARN: phase = PH_MENU; break;
    case PH_START: beginRun(startPos); break;
    case PH_IN:
      if (cur == dg(pos)) {
        pos++;
        if (mode == M_RECITE && pos > best) { best = pos; newRecord = 1; saveRec(); }
        if (pos >= N) { wrongGot = 255; phase = PH_FAIL; }
      } else {
        wrongGot = cur;
        phase = PH_FAIL;
      }
      break;
    case PH_FAIL: phase = PH_MENU; break;
    case PH_BSHOW: bpos = 0; cur = 0; phase = PH_BTYPE; break;
    case PH_BTYPE:
      btyped[bpos++] = cur;
      if (bpos >= 10) {
        bool ok = true;
        for (int k = 0; k < 10; k++) if (btyped[k] != dg(blk * 10 + k)) ok = false;
        if (ok && blk + 1 > blockBest) { blockBest = blk + 1; saveRec(); }
        phase = PH_BRES;
      }
      break;
    case PH_BRES: {
      bool ok = true;
      for (int k = 0; k < 10; k++) if (btyped[k] != dg(blk * 10 + k)) ok = false;
      if (ok) blk = (uint16_t)((blk + 1) % (N / 10));
      phase = PH_BSHOW;
      break;
    }
  }
}

// ---------------- drawing ----------------
inline void centerText(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  int16_t w = qwTextWidth((uint8_t)strlen(t), size) - size;
  d.setCursor((200 - w) / 2, y);
  d.print(t);
}

inline void leftText(Adafruit_GFX &d, const char *t, int16_t x, int16_t y, uint8_t size) {
  qwSetText(d, size);
  d.setCursor(x, y);
  d.print(t);
}

// One 12 x 16 digit cell. inv = shown white on black (the digit being chosen).
inline void cell(Adafruit_GFX &d, int16_t x, int16_t y, char ch, bool inv, bool ghost) {
  uint16_t ink = qwInk(), paper = qwPaper();
  if (inv) d.fillRect(x - 1, y - 1, 13, 18, ink);
  if (ghost) {  // a decimal still to come: just a short bar
    d.drawFastHLine(x + 2, y + 13, 7, ink);
    return;
  }
  d.setTextColor(inv ? paper : ink);
  qwSetText(d, 2);
  d.setCursor(x, y);
  d.print(ch);
}

constexpr int16_t GX = 34;  // left edge of the digit grid
inline int16_t cellX(int i) { return GX + (i % 10) * 12 + ((i % 10) >= 5 ? 8 : 0); }

// Row prefix: "3." on the first row, otherwise the number of the first decimal.
inline void rowLabel(Adafruit_GFX &d, int row, int16_t y) {
  d.setTextColor(qwInk());
  if (row == 0) leftText(d, "3.", 6, y, 2);
  else {
    char b[8];
    snprintf(b, sizeof(b), "%d", row * 10 + 1);
    leftText(d, b, 4, y + 4, 1);
  }
}

inline void header(Adafruit_GFX &d, const char *t) {
  d.setTextColor(qwInk());
  leftText(d, t, 4, 3, 1);
  d.drawFastHLine(0, 13, 200, qwInk());
}

inline void drawMenu(Adafruit_GFX &d) {
  static const char *const NAMES[4] = {"LEARN", "RECITE", "FROM DIGIT N", "10-DIGIT BLOCK"};
  d.setTextColor(qwInk());
  centerText(d, "PI MEMORY", 6, 2);
  d.drawFastHLine(0, 26, 200, qwInk());
  for (int i = 0; i < 4; i++) {
    int16_t y = 40 + i * 28;
    if (i == menuSel) { d.fillRect(6, y - 4, 188, 24, qwInk()); d.setTextColor(qwPaper()); }
    else d.setTextColor(qwInk());
    leftText(d, NAMES[i], 14, y, 2);
  }
  d.setTextColor(qwInk());
  char b[40];
  snprintf(b, sizeof(b), "BEST RUN: %d   BLOCKS: %d/100", best, blockBest);
  leftText(d, b, 4, 160, 1);
  leftText(d, "UP/DOWN = select   MENU = ok", 4, 176, 1);
  leftText(d, "BACK = leave", 4, 188, 1);
}

inline void drawLearn(Adafruit_GFX &d) {
  char b[40];
  snprintf(b, sizeof(b), "LEARN  DECIMALS %d-%d  (%d/%d)", page * 50 + 1, page * 50 + 50, page + 1, N / 50);
  header(d, b);
  for (int r = 0; r < 5; r++) {
    int row = page * 5 + r;
    int16_t y = 26 + r * 26;
    rowLabel(d, row, y);
    for (int k = 0; k < 10; k++) cell(d, cellX(k), y, (char)('0' + dg(row * 10 + k)), false, false);
  }
  d.setTextColor(qwInk());
  leftText(d, "DOWN = next page  UP = previous", 4, 164, 1);
  leftText(d, "MENU = modes   BACK = leave", 4, 178, 1);
}

inline void drawStart(Adafruit_GFX &d) {
  header(d, "FROM DIGIT N");
  centerText(d, "START AFTER DECIMAL", 40, 1);
  char b[8];
  snprintf(b, sizeof(b), "%d", startPos);
  centerText(d, b, 64, 5);
  // the 10 decimals before the start, as a reminder
  int16_t y = 120;
  for (int k = 0; k < 10; k++) cell(d, cellX(k), y, (char)('0' + dg(startPos - 10 + k)), false, false);
  d.setTextColor(qwInk());
  snprintf(b, sizeof(b), "%d", startPos - 9);
  leftText(d, b, 4, y + 4, 1);
  centerText(d, "DOWN = later  UP = earlier (+/-10)", 152, 1);
  centerText(d, "MENU = start typing", 168, 1);
}

inline void drawInput(Adafruit_GFX &d) {
  char b[40];
  snprintf(b, sizeof(b), mode == M_RECITE ? "RECITE" : "FROM DIGIT %d", startPos);
  header(d, b);
  int curRow = pos / 10;
  int firstRow = curRow >= 4 ? curRow - 4 : 0;
  int visFrom = (mode == M_FROM && startPos >= 10) ? startPos - 10 : 0;
  for (int r = 0; r < 5; r++) {
    int row = firstRow + r;
    if (row >= N / 10) break;
    int16_t y = 24 + r * 24;
    rowLabel(d, row, y);
    for (int k = 0; k < 10; k++) {
      int i = row * 10 + k;
      if (i == pos) cell(d, cellX(k), y, (char)('0' + cur), true, false);
      else if (i < pos && i >= visFrom) cell(d, cellX(k), y, (char)('0' + dg(i)), false, false);
      else cell(d, cellX(k), y, ' ', false, true);
    }
  }
  d.setTextColor(qwInk());
  snprintf(b, sizeof(b), "DECIMAL %d / %d   RUN %d", pos + 1, N, pos - startPos);
  leftText(d, b, 4, 150, 1);
  snprintf(b, sizeof(b), "BEST RUN: %d", best);
  leftText(d, b, 4, 162, 1);
  leftText(d, "UP/DOWN = digit   MENU = confirm", 4, 180, 1);
  leftText(d, "BACK = stop", 4, 190, 1);
}

inline void drawFail(Adafruit_GFX &d) {
  d.setTextColor(qwInk());
  char b[40];
  if (wrongGot == 255) {
    centerText(d, "PERFECT!", 20, 3);
    snprintf(b, sizeof(b), "ALL %d DECIMALS", N);
    centerText(d, b, 70, 2);
  } else {
    centerText(d, "WRONG", 20, 3);
    snprintf(b, sizeof(b), "DECIMAL %d:", pos + 1);
    centerText(d, b, 62, 2);
    snprintf(b, sizeof(b), "YOU  %d   IT  %d", wrongGot, dg(pos));
    centerText(d, b, 86, 2);
  }
  snprintf(b, sizeof(b), "RUN: %d DECIMALS", pos - startPos);
  centerText(d, b, 118, 2);
  if (mode == M_RECITE) {
    if (newRecord) centerText(d, "NEW RECORD!", 142, 2);
    else { snprintf(b, sizeof(b), "BEST: %d", best); centerText(d, b, 142, 2); }
  }
  d.drawRect(30, 166, 140, 24, qwInk());
  centerText(d, "MENU = MODES", 172, 1);
}

// A block of 10 as big digits, two groups of 5.
inline void bigRow(Adafruit_GFX &d, int16_t y, const uint8_t *v, int n, int hi, bool hide) {
  for (int k = 0; k < 10; k++) {
    int16_t x = 6 + k * 18 + (k >= 5 ? 8 : 0);
    uint16_t ink = qwInk(), paper = qwPaper();
    bool inv = (k == hi);
    if (inv) d.fillRect(x - 1, y - 1, 19, 26, ink);
    if (k >= n && !inv) { d.drawFastHLine(x + 2, y + 21, 12, ink); continue; }
    d.setTextColor(inv ? paper : ink);
    qwSetText(d, 3);
    d.setCursor(x, y);
    d.print((char)('0' + v[k]));
    (void)hide;
  }
}

inline void drawBlockShow(Adafruit_GFX &d) {
  char b[40];
  snprintf(b, sizeof(b), "BLOCK %d   DECIMALS %d-%d", blk + 1, blk * 10 + 1, blk * 10 + 10);
  header(d, b);
  uint8_t v[10];
  for (int k = 0; k < 10; k++) v[k] = (uint8_t)dg(blk * 10 + k);
  bigRow(d, 56, v, 10, -1, false);
  d.setTextColor(qwInk());
  centerText(d, "LEARN THIS BLOCK", 100, 1);
  snprintf(b, sizeof(b), "CLEARED: %d / 100", blockBest);
  centerText(d, b, 120, 1);
  centerText(d, "DOWN = next block  UP = previous", 150, 1);
  centerText(d, "MENU = hide it and type it", 164, 1);
}

inline void drawBlockType(Adafruit_GFX &d) {
  char b[40];
  snprintf(b, sizeof(b), "BLOCK %d - TYPE IT", blk + 1);
  header(d, b);
  uint8_t shown[10];
  for (int k = 0; k < 10; k++) shown[k] = (k < bpos) ? btyped[k] : cur;
  bigRow(d, 56, shown, bpos + 1, bpos, true);
  d.setTextColor(qwInk());
  snprintf(b, sizeof(b), "DECIMAL %d / 10", bpos + 1);
  centerText(d, b, 100, 1);
  centerText(d, "UP/DOWN = digit   MENU = confirm", 160, 1);
  centerText(d, "BACK = stop", 174, 1);
}

inline void drawBlockResult(Adafruit_GFX &d) {
  char b[40];
  uint8_t v[10];
  int ok = 0;
  for (int k = 0; k < 10; k++) { v[k] = (uint8_t)dg(blk * 10 + k); if (btyped[k] == v[k]) ok++; }
  header(d, "BLOCK RESULT");
  d.setTextColor(qwInk());
  centerText(d, "YOU TYPED", 22, 1);
  bigRow(d, 34, btyped, 10, -1, false);
  // mark each wrong digit with a bar under it
  for (int k = 0; k < 10; k++)
    if (btyped[k] != v[k]) d.fillRect(6 + k * 18 + (k >= 5 ? 8 : 0), 62, 16, 3, qwInk());
  d.setTextColor(qwInk());
  centerText(d, "IT WAS", 76, 1);
  bigRow(d, 88, v, 10, -1, false);
  if (ok == 10) centerText(d, "PERFECT!", 128, 1);
  else { snprintf(b, sizeof(b), "%d / 10 RIGHT - TRY AGAIN", ok); centerText(d, b, 128, 1); }
  d.drawRect(30, 160, 140, 24, qwInk());
  centerText(d, ok == 10 ? "MENU = NEXT" : "MENU = RETRY", 166, 1);
}

inline void draw(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  switch (phase) {
    case PH_MENU: drawMenu(d); break;
    case PH_LEARN: drawLearn(d); break;
    case PH_START: drawStart(d); break;
    case PH_IN: drawInput(d); break;
    case PH_FAIL: drawFail(d); break;
    case PH_BSHOW: drawBlockShow(d); break;
    case PH_BTYPE: drawBlockType(d); break;
    default: drawBlockResult(d); break;
  }
}

}  // namespace Pm
