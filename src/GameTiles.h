#pragma once
/*
  GameTiles.h
  ------------------------------------------------------------
  TILE MEMORY for QuranWatch (included by Apps.h).

  A memory game whose cards show TILINGS of the plane: regular ones
  (made of regular polygons: triangles, squares, hexagons, octagons,
  dodecagons...) and irregular ones (rhombille, basket weave, brick
  bond, the Pythagorean tiling). Every tiling is drawn by code from a
  few line segments repeated on a lattice - no bitmaps.

  Find all the pairs. 1 or 2 players. Three board sizes.
    UP / DOWN : move the cursor (hold to move 3 or 6 cards at once;
                matched cards are skipped)
    MENU      : turn the card over
  After a pair is turned, the name of the tiling is shown when it is a
  match; any button goes on. 2 players: a match keeps your turn.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include "UiCommon.h"
#include "Storage.h"

namespace Tl {

constexpr int NT = 12;       // number of tilings
constexpr int MAXC = 20;     // cards on the biggest board

static const char *const TNAME[NT] = {
  "TRIANGLES 3^6", "SQUARES 4^4", "HEXAGONS 6^3", "TRUNC SQUARE 4.8.8",
  "KAGOME 3.6.3.6", "RHOMBI-HEX 3.4.6.4", "DODECAGONS 3.12.12", "ELONG TRIANGLES",
  "RHOMBILLE", "BASKET WEAVE", "BRICK BOND", "PYTHAGOREAN"};

// ---------------- drawing one tiling into a card ----------------
inline int gx0, gy0, gx1, gy1;       // clip rectangle (inclusive)
inline int gcx, gcy;                 // pixel position of the lattice origin
inline float gox, goy, gsc;          // current cell offset (units) and pixels per unit
inline Adafruit_GFX *gd;

inline void px(int x, int y) {
  if (x >= gx0 && x <= gx1 && y >= gy0 && y <= gy1) gd->drawPixel(x, y, qwInk());
}
inline void seg(float x0, float y0, float x1, float y1) {
  int ax = gcx + (int)lroundf((x0 + gox) * gsc), ay = gcy - (int)lroundf((y0 + goy) * gsc);
  int bx = gcx + (int)lroundf((x1 + gox) * gsc), by = gcy - (int)lroundf((y1 + goy) * gsc);
  if ((ax < gx0 && bx < gx0) || (ax > gx1 && bx > gx1) || (ay < gy0 && by < gy0) || (ay > gy1 && by > gy1)) return;
  int dx = abs(bx - ax), dy = -abs(by - ay), sx = ax < bx ? 1 : -1, sy = ay < by ? 1 : -1, e = dx + dy;
  for (;;) {
    px(ax, ay);
    if (ax == bx && ay == by) break;
    int e2 = 2 * e;
    if (e2 >= dy) { e += dy; ax += sx; }
    if (e2 <= dx) { e += dx; ay += sy; }
  }
}

constexpr float H3 = 0.8660254f, S3 = 1.7320508f;

// lattice vectors (units) and scale (pixels per unit) of every tiling
struct Lat { float ax, ay, bx, by, sc; };
static const Lat LAT[NT] = {
  {1, 0, 0.5f, H3, 12},                  // 0 triangles
  {1, 0, 0, 1, 10},                       // 1 squares
  {1.5f, H3, 0, S3, 9},                  // 2 hexagons
  {2.4142f, 0, 0, 2.4142f, 8},           // 3 4.8.8
  {2, 0, 1, S3, 10},                      // 4 kagome
  {2.732f, 0, 1.366f, 2.366f, 10},        // 5 3.4.6.4
  {3.732f, 0, 1.866f, 3.232f, 8},        // 6 3.12.12
  {1, 0, 0.5f, 1 + H3, 11},              // 7 elongated triangular
  {1.5f, H3, 0, S3, 10},                  // 8 rhombille
  {4, 0, 2, 2, 6},                       // 9 basket weave
  {2, 0, 1, 1, 10},                       // 10 brick bond
  {3, 2, -2, 3, 8},                      // 11 Pythagorean
};

// the segments of ONE lattice cell
inline void cell(int id) {
  switch (id) {
    case 0: seg(0, 0, 1, 0); seg(0, 0, .5f, H3); seg(1, 0, .5f, H3); break;
    case 1: seg(0, 0, 1, 0); seg(0, 0, 0, 1); break;
    case 2: seg(1, 0, .5f, H3); seg(.5f, H3, -.5f, H3); seg(-.5f, H3, -1, 0); break;
    case 3: {
      const float a = 1.2071f, b = 0.5f;
      seg(-b, a, b, a); seg(a, -b, a, b);
      seg(a, b, b, a); seg(-b, a, -a, b); seg(-a, -b, -b, -a); seg(b, -a, a, -b);
      break;
    }
    case 4:
      seg(1, 0, .5f, H3); seg(1, 0, 1.5f, H3); seg(.5f, H3, 1.5f, H3);
      seg(2, S3, 1.5f, H3); seg(2, S3, 2.5f, H3); seg(1.5f, H3, 2.5f, H3);
      break;
    case 5: {
      float vx[6], vy[6], nx[6], ny[6];
      for (int k = 0; k < 6; k++) {
        float a = (30 + 60 * k) * 0.0174533f, n = (60 + 60 * k) * 0.0174533f;
        vx[k] = cosf(a); vy[k] = sinf(a); nx[k] = cosf(n); ny[k] = sinf(n);
      }
      for (int k = 0; k < 6; k++) {
        int k1 = (k + 1) % 6, kp = (k + 5) % 6;
        seg(vx[k], vy[k], vx[k1], vy[k1]);                                       // hexagon
        seg(vx[k], vy[k], vx[k] + nx[k], vy[k] + ny[k]);                         // square sides
        seg(vx[k1], vy[k1], vx[k1] + nx[k], vy[k1] + ny[k]);
        seg(vx[k] + nx[kp], vy[k] + ny[kp], vx[k] + nx[k], vy[k] + ny[k]);       // triangle bases
      }
      break;
    }
    case 6: {
      const float R = 1.93185f;
      for (int k = 0; k < 12; k++) {
        float a = (15 + 30 * k) * 0.0174533f, b = (45 + 30 * k) * 0.0174533f;
        seg(R * cosf(a), R * sinf(a), R * cosf(b), R * sinf(b));
      }
      break;
    }
    case 7: seg(0, 0, 1, 0); seg(0, 0, 0, 1); seg(0, 1, 1, 1); seg(0, 1, .5f, 1 + H3); seg(.5f, 1 + H3, 1, 1); break;
    case 8:
      seg(1, 0, .5f, H3); seg(.5f, H3, -.5f, H3); seg(-.5f, H3, -1, 0);
      seg(0, 0, .5f, H3); seg(0, 0, -1, 0); seg(0, 0, .5f, -H3);
      break;
    case 9: seg(0, 0, 4, 0); seg(0, 0, 0, 2); seg(2, 0, 2, 2); seg(0, 1, 2, 1); seg(3, 0, 3, 2); break;
    case 10: seg(0, 0, 2, 0); seg(0, 0, 0, 1); break;
    case 11:
      seg(0, 0, 3, 0); seg(3, 0, 3, 3); seg(3, 3, 0, 3); seg(0, 3, 0, 0);
      seg(3, 0, 5, 0); seg(5, 0, 5, 2); seg(5, 2, 3, 2);
      break;
  }
}

// draw tiling `id` inside the rectangle (x, y, w, h); the lattice origin is the centre
inline void drawTiling(Adafruit_GFX &d, int id, int x, int y, int w, int h) {
  gd = &d; gx0 = x; gy0 = y; gx1 = x + w - 1; gy1 = y + h - 1;
  gcx = x + w / 2; gcy = y + h / 2; gsc = LAT[id].sc;
  const Lat &L = LAT[id];
  for (int i = -5; i <= 5; i++)
    for (int j = -5; j <= 5; j++) {
      gox = i * L.ax + j * L.bx; goy = i * L.ay + j * L.by;
      float cx = gox * gsc, cy = goy * gsc;     // quick reject: cell far outside the card
      if (fabsf(cx) > w * 0.5f + 3.2f * gsc || fabsf(cy) > h * 0.5f + 3.2f * gsc) continue;
      cell(id);
    }
}

// ---------------- game state ----------------
enum { PH_MENU, PH_PLAY, PH_RESULT, PH_END };
RTC_DATA_ATTR uint8_t phase = PH_MENU;
RTC_DATA_ATTR uint8_t menuRow = 2;        // 0 players, 1 grid, 2 start
RTC_DATA_ATTR uint8_t players = 1;        // 1 or 2
RTC_DATA_ATTR uint8_t gridSel = 1;        // 0: 4x3, 1: 4x4, 2: 5x4
RTC_DATA_ATTR uint8_t card[MAXC];         // tiling on each card
RTC_DATA_ATTR uint8_t cs[MAXC];           // 0 face down, 1 matched, 2 face up (being compared)
RTC_DATA_ATTR uint8_t cur = 0;
RTC_DATA_ATTR int8_t first = -1, second = -1;
RTC_DATA_ATTR uint8_t turn = 0;           // player 0 or 1
RTC_DATA_ATTR uint8_t score[2] = {0, 0};
RTC_DATA_ATTR uint8_t pairsFound = 0;
RTC_DATA_ATTR uint16_t moves = 0;
RTC_DATA_ATTR bool lastMatch = false;
RTC_DATA_ATTR uint8_t best[3] = {0, 0, 0};   // fewest moves per board (solo), 0 = none
RTC_DATA_ATTR bool bestLoaded = false;
RTC_DATA_ATTR uint32_t rng = 2463534242u;

constexpr uint8_t COLS[3] = {4, 4, 5}, ROWS[3] = {3, 4, 4};
inline int nCards() { return COLS[gridSel] * ROWS[gridSel]; }
inline int nPairs() { return nCards() / 2; }

inline uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

inline void loadBest() {
  if (bestLoaded) return;
  bestLoaded = true;
  uint8_t b[4];
  if (QWStorage::loadBlob("tilB", b, 4) && b[0] == 0x54) { best[0] = b[1]; best[1] = b[2]; best[2] = b[3]; }
}
inline void saveBest() { uint8_t b[4] = {0x54, best[0], best[1], best[2]}; QWStorage::saveBlob("tilB", b, 4); }

inline void start() {
  loadBest();
  if (phase == PH_MENU) return;
  // a game in progress is kept (the watch was only sleeping)
}

inline void deal() {
  rng ^= (uint32_t)micros() * 2654435761u; rng ^= (uint32_t)millis() * 2246822519u; if (!rng) rng = 1;
  uint8_t pool[NT];
  for (int i = 0; i < NT; i++) pool[i] = i;
  for (int i = NT - 1; i > 0; i--) { int j = rnd() % (i + 1); uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t; }
  int n = nCards(), np = n / 2;
  for (int i = 0; i < np; i++) { card[2 * i] = pool[i]; card[2 * i + 1] = pool[i]; }
  for (int i = n - 1; i > 0; i--) { int j = rnd() % (i + 1); uint8_t t = card[i]; card[i] = card[j]; card[j] = t; }
  memset(cs, 0, sizeof(cs));
  cur = 0; first = second = -1; turn = 0; score[0] = score[1] = 0; pairsFound = 0; moves = 0;
  phase = PH_PLAY;
}

// move the cursor `steps` unmatched cards in direction dir
inline void moveCur(int dir, int steps) {
  int n = nCards();
  for (int s = 0; s < steps; s++) {
    for (int k = 0; k < n; k++) {
      cur = (cur + n + dir) % n;
      if (cs[cur] != 1) break;
    }
  }
}
inline void fixCur() { int n = nCards(); for (int k = 0; k < n && cs[cur] == 1; k++) cur = (cur + 1) % n; }

// leave the RESULT screen. returns true when the game goes on.
inline void resolve() {
  if (lastMatch) {
    cs[first] = cs[second] = 1;
    pairsFound++; score[turn]++;
  } else {
    cs[first] = cs[second] = 0;
    if (players == 2) turn ^= 1;
  }
  first = second = -1;
  if (pairsFound >= nPairs()) {
    if (players == 1 && (best[gridSel] == 0 || moves < best[gridSel])) { best[gridSel] = moves > 255 ? 255 : moves; saveBest(); }
    phase = PH_END;
    return;
  }
  phase = PH_PLAY;
  fixCur();
}

inline void flip() {
  if (cs[cur] != 0) return;
  cs[cur] = 2;
  if (first < 0) { first = cur; return; }
  second = cur; moves++;
  lastMatch = card[first] == card[second];
  phase = PH_RESULT;
}

inline bool busy() { return phase == PH_PLAY || phase == PH_RESULT; }

inline void up() {
  if (phase == PH_MENU) { menuRow = (menuRow + 2) % 3; return; }
  if (phase == PH_RESULT) { bool m = lastMatch; resolve(); if (m && phase == PH_PLAY) moveCur(-1, qwGameSteps()); return; }
  if (phase == PH_PLAY) moveCur(-1, qwGameSteps());
}
inline void down() {
  if (phase == PH_MENU) { menuRow = (menuRow + 1) % 3; return; }
  if (phase == PH_RESULT) { bool m = lastMatch; resolve(); if (m && phase == PH_PLAY) moveCur(1, qwGameSteps()); return; }
  if (phase == PH_PLAY) moveCur(1, qwGameSteps());
}
inline void select() {
  if (phase == PH_MENU) {
    if (menuRow == 0) players = players == 1 ? 2 : 1;
    else if (menuRow == 1) gridSel = (gridSel + 1) % 3;
    else deal();
    return;
  }
  if (phase == PH_RESULT) { bool m = lastMatch; resolve(); if (m && phase == PH_PLAY) flip(); return; }
  if (phase == PH_PLAY) { flip(); return; }
  if (phase == PH_END) phase = PH_MENU;
}

// ---------------- drawing ----------------
inline void text(Adafruit_GFX &d, int x, int y, const char *s, int size) {
  qwSetText(d, size); d.setTextColor(qwInk()); d.setCursor(x, y); d.print(s);
}
inline void textC(Adafruit_GFX &d, int y, const char *s, int size) {
  text(d, 100 - qwTextInkWidth(strlen(s), size) / 2, y, s, size);
}

inline void cardRect(int i, int &x, int &y, int &w, int &h) {
  int cols = COLS[gridSel], rows = ROWS[gridSel];
  int pw = 192 / cols, ph = 174 / rows;
  x = 4 + (i % cols) * pw + 2; y = 23 + (i / cols) * ph + 2;
  w = pw - 4; h = ph - 4;
}

inline void drawMenu(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  textC(d, 6, "TILE MEMORY", 2);
  static const uint8_t sample[4] = {6, 4, 8, 11};
  for (int k = 0; k < 4; k++) {
    int x = 8 + k * 48;
    d.drawRect(x, 30, 40, 34, qwInk());
    drawTiling(d, sample[k], x + 1, 31, 38, 32);
  }
  char buf[20];
  for (int r = 0; r < 3; r++) {
    if (r == 0) snprintf(buf, sizeof(buf), "PLAYERS: %d", players);
    else if (r == 1) snprintf(buf, sizeof(buf), "BOARD: %dx%d", COLS[gridSel], ROWS[gridSel]);
    else snprintf(buf, sizeof(buf), "START");
    int y = 78 + r * 26;
    int tx = 100 - qwTextInkWidth(strlen(buf), 2) / 2;
    if (r == menuRow) {
      d.fillRect(14, y - 4, 172, 22, qwInk());
      qwSetText(d, 2); d.setTextColor(qwPaper()); d.setCursor(tx, y); d.print(buf);
    } else text(d, tx, y, buf, 2);
  }
  if (best[gridSel]) { snprintf(buf, sizeof(buf), "BEST (SOLO): %d", best[gridSel]); textC(d, 160, buf, 1); }
  textC(d, 176, "UP/DOWN: ROW   MENU: CHANGE", 1);
}

inline void drawCard(Adafruit_GFX &d, int i) {
  int x, y, w, h; cardRect(i, x, y, w, h);
  uint16_t ink = qwInk();
  if (cs[i] == 0) {                                  // face down: dotted back
    d.drawRect(x, y, w, h, ink);
    for (int yy = y + 2; yy < y + h - 2; yy += 3)
      for (int xx = x + 2 + ((yy - y) / 3 % 2) * 2; xx < x + w - 2; xx += 4) d.drawPixel(xx, yy, ink);
  } else {
    d.drawRect(x, y, w, h, ink);
    drawTiling(d, card[i], x + 1, y + 1, w - 2, h - 2);
    if (cs[i] == 1) d.drawRect(x + 1, y + 1, w - 2, h - 2, ink);
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MENU) { drawMenu(d); return; }
  d.fillScreen(qwPaper());
  char buf[40];
  if (phase == PH_END) {
    textC(d, 14, "ALL PAIRS FOUND", 1);
    if (players == 1) {
      snprintf(buf, sizeof(buf), "%d MOVES", moves); textC(d, 50, buf, 2);
      if (best[gridSel] == moves) textC(d, 80, "NEW BEST!", 2);
      else { snprintf(buf, sizeof(buf), "BEST: %d", best[gridSel]); textC(d, 80, buf, 2); }
    } else {
      snprintf(buf, sizeof(buf), "P1: %d  P2: %d", score[0], score[1]); textC(d, 50, buf, 2);
      textC(d, 80, score[0] == score[1] ? "DRAW" : score[0] > score[1] ? "PLAYER 1 WINS" : "PLAYER 2 WINS", 2);
    }
    for (int k = 0; k < 4; k++) {
      int x = 8 + k * 48;
      d.drawRect(x, 120, 40, 34, qwInk());
      drawTiling(d, (k * 3 + 1) % NT, x + 1, 121, 38, 32);
    }
    textC(d, 176, "PRESS MENU", 1);
    return;
  }
  if (players == 1) snprintf(buf, sizeof(buf), "MOVES %d   PAIRS %d/%d", moves, pairsFound, nPairs());
  else snprintf(buf, sizeof(buf), "%cP1 %d   %cP2 %d", turn == 0 ? '>' : ' ', score[0], turn == 1 ? '>' : ' ', score[1]);
  text(d, 4, 3, buf, 1);
  const char *msg;
  if (phase == PH_RESULT) {
    if (lastMatch) { snprintf(buf, sizeof(buf), "MATCH! %s", TNAME[card[first]]); msg = buf; }
    else msg = "NO MATCH - PRESS ANY";
  } else msg = first < 0 ? "FLIP A CARD" : "FLIP A 2ND CARD";
  text(d, 4, 12, msg, 1);
  for (int i = 0; i < nCards(); i++) drawCard(d, i);
  if (phase == PH_PLAY) {                           // cursor: a double frame around the card
    int x, y, w, h; cardRect(cur, x, y, w, h);
    d.drawRect(x - 1, y - 1, w + 2, h + 2, qwInk());
    d.drawRect(x - 2, y - 2, w + 4, h + 4, qwInk());
  }
}

}  // namespace Tl
