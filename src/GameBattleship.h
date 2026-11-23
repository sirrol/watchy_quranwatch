#pragma once
/*
  GameBattleship.h
  ------------------------------------------------------------
  BATTLESHIP for QuranWatch (included by Games.h).

  Two fleets of five ships hide on two 10 x 10 grids:
      CARRIER 5, BATTLESHIP 4, CRUISER 3, SUBMARINE 3, DESTROYER 2.
  Ships are straight (sideways or up/down), never overlap, and may touch.
  Players take turns firing at one cell of the enemy grid. The shot says
  MISS, HIT, or SUNK (a ship is sunk when all its cells are hit).
  The first player to sink the whole enemy fleet wins.

  1 player : you against the watch (levels EASY and HARD).
             EASY fires at random. HARD works out, from what it already
             knows, where your ships can still be and shoots there.
  2 players: pass the watch. A "PLAYER n" screen hides the grids between
             turns, so hand the watch over BEFORE pressing MENU.

  Placing the ships: RANDOM FLEET (UP/DOWN shuffle, MENU keeps it) or
  PLACE MYSELF (column, row, then sideways / up-down for each ship).

  Aiming needs only a few presses: UP/DOWN choose the COLUMN (letters A-J),
  MENU, then UP/DOWN choose the ROW (1-10; the "<" stop goes back to the
  column), MENU fires. Hold the button to move 3 or 6 steps at a time.

  Screen: the big grid is the one you fire at (X = hit, dot = miss, sunk
  ships are shown in black). Top right: your own fleet in miniature.
  Under it: the two fleets as small bars (filled = damaged / sunk).
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Bs {

constexpr int N = 10, CELLS = 100, SHIPS = 5, SHIP_CELLS = 17;
static const uint8_t SIZE[SHIPS] = {5, 4, 3, 3, 2};
static const char *const NAME[SHIPS] = {"CARRIER", "BATTLESHIP", "CRUISER", "SUBMARINE",
                                        "DESTROYER"};

// cell: bits 0-2 = ship number + 1 (0 = water), bit 3 = already shot.
struct Fleet {
  uint8_t ox[SHIPS], oy[SHIPS], ori[SHIPS];  // ori 0 = sideways, 1 = up/down
  uint8_t cell[CELLS];
};

inline bool canPlace(const Fleet &f, int len, int x, int y, int o) {
  for (int k = 0; k < len; k++) {
    int xx = x + (o == 0 ? k : 0), yy = y + (o == 1 ? k : 0);
    if (xx > 9 || yy > 9 || (f.cell[yy * N + xx] & 7)) return false;
  }
  return true;
}

inline void placeShip(Fleet &f, int i, int x, int y, int o) {
  f.ox[i] = (uint8_t)x;
  f.oy[i] = (uint8_t)y;
  f.ori[i] = (uint8_t)o;
  for (int k = 0; k < SIZE[i]; k++)
    f.cell[(y + (o == 1 ? k : 0)) * N + x + (o == 0 ? k : 0)] = (uint8_t)(i + 1);
}

inline int shipCell(const Fleet &f, int i, int k) {
  return (f.oy[i] + (f.ori[i] == 1 ? k : 0)) * N + f.ox[i] + (f.ori[i] == 0 ? k : 0);
}

inline int hitsOn(const Fleet &f, int i) {
  int h = 0;
  for (int k = 0; k < SIZE[i]; k++)
    if (f.cell[shipCell(f, i, k)] & 8) h++;
  return h;
}
inline bool sunk(const Fleet &f, int i) { return hitsOn(f, i) == SIZE[i]; }

inline bool allSunk(const Fleet &f) {
  int h = 0;
  for (int c = 0; c < CELLS; c++)
    if ((f.cell[c] & 7) && (f.cell[c] & 8)) h++;
  return h == SHIP_CELLS;
}

// Fires at cell c: 0 = miss, 1 = hit, 2 = sunk (ship = the ship hit).
inline int fire(Fleet &f, int c, int &ship) {
  f.cell[c] |= 8;
  int id = f.cell[c] & 7;
  ship = id - 1;
  if (!id) return 0;
  return sunk(f, ship) ? 2 : 1;
}

// ---------------- random numbers ----------------
RTC_DATA_ATTR uint32_t rngState = 123456789u;  // kept across deep sleep
inline uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}
inline void stirRng() {
  rngState ^= (uint32_t)micros() * 2654435761u;
  rngState ^= (uint32_t)millis() * 2246822519u;
  if (!rngState) rngState = 123456789u;
  for (int i = 0; i < 3; i++) rnd();
}

inline void randomFleet(Fleet &f) {
  memset(&f, 0, sizeof(f));
  for (int i = 0; i < SHIPS; i++) {
    for (;;) {
      int o = rnd() & 1, x = rnd() % N, y = rnd() % N;
      if (canPlace(f, SIZE[i], x, y, o)) {
        placeShip(f, i, x, y, o);
        break;
      }
    }
  }
}

// ---------------- the robot ----------------
// EASY: random shots, but next to a hit it finishes the ship (hunt and
// target). HARD: every way the ships still afloat could
// lie on the grid is counted (placements covering known hits count much
// more); it shoots the cell that appears in the most of them.
inline int robotPick(const Fleet &f, bool hard) {
  int w[CELLS];
  memset(w, 0, sizeof(w));
  if (!hard) {  // cells next to a hit on a ship that is still afloat
    int near[CELLS], m = 0;
    for (int c = 0; c < CELLS; c++) {
      uint8_t v = f.cell[c];
      if (!(v & 8) || !(v & 7) || sunk(f, (v & 7) - 1)) continue;
      static const int8_t D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (int k = 0; k < 4; k++) {
        int x = c % N + D[k][0], y = c / N + D[k][1];
        if (x < 0 || x > 9 || y < 0 || y > 9 || (f.cell[y * N + x] & 8)) continue;
        near[m++] = y * N + x;
      }
    }
    if (m) return near[rnd() % m];
  }
  if (hard) {
    for (int i = 0; i < SHIPS; i++) {
      if (sunk(f, i)) continue;
      for (int o = 0; o < 2; o++)
        for (int y = 0; y < N; y++)
          for (int x = 0; x < N; x++) {
            int len = SIZE[i], cov = 0;
            bool ok = true;
            for (int k = 0; k < len && ok; k++) {
              int xx = x + (o == 0 ? k : 0), yy = y + (o == 1 ? k : 0);
              if (xx > 9 || yy > 9) { ok = false; break; }
              uint8_t v = f.cell[yy * N + xx];
              if (v & 8) {
                if (!(v & 7) || sunk(f, (v & 7) - 1)) ok = false;  // a miss, or a sunk ship
                else cov++;
              }
            }
            if (!ok) continue;
            int wt = 1 + 40 * cov * cov;
            for (int k = 0; k < len; k++) {
              int c = (y + (o == 1 ? k : 0)) * N + x + (o == 0 ? k : 0);
              if (!(f.cell[c] & 8)) w[c] += wt;
            }
          }
    }
  }
  int best = 0;
  for (int c = 0; c < CELLS; c++)
    if (!(f.cell[c] & 8) && (hard ? w[c] : 1) > best) best = hard ? w[c] : 1;
  int list[CELLS], n = 0;
  for (int c = 0; c < CELLS; c++)
    if (!(f.cell[c] & 8) && (hard ? w[c] : 1) == best) list[n++] = c;
  if (n == 0) {  // (cannot happen unless the grid is full)
    for (int c = 0; c < CELLS; c++)
      if (!(f.cell[c] & 8)) list[n++] = c;
  }
  return n ? list[rnd() % n] : 0;
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_LEVEL, PH_STYLE, PH_PASS, PH_PLACE, PH_PLAY, PH_RESULT, PH_OVER };

RTC_DATA_ATTR Fleet fl[2];
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t style = 0;       // 0 = random fleet, 1 = place myself
RTC_DATA_ATTR uint8_t turn = 0;        // who fires (0 = you / player 1)
RTC_DATA_ATTR uint8_t placeP = 0;      // who is placing ships
RTC_DATA_ATTR uint8_t placeIdx = 0;    // ship being placed
RTC_DATA_ATTR uint8_t st = 0;          // 0 = column, 1 = row, 2 = direction
RTC_DATA_ATTR uint8_t col = 0;
RTC_DATA_ATTR uint8_t rowSel = 0;      // 0-9 = row, 10 = "<" (back to the column)
RTC_DATA_ATTR uint8_t ori = 0;         // 0 sideways, 1 up/down, 2 cancel
RTC_DATA_ATTR uint8_t passNext = PH_PLAY;
RTC_DATA_ATTR uint8_t passFor = 0;
RTC_DATA_ATTR uint8_t myRes = 0, myCell = 0, myShip = 0;  // last shot: 0 none, 1 miss, 2 hit, 3 sunk
RTC_DATA_ATTR uint8_t rbRes = 0, rbCell = 0, rbShip = 0;  // last robot shot
RTC_DATA_ATTR uint8_t warn = 0;        // 1 = "already shot", 2 = "no room"
RTC_DATA_ATTR uint8_t winner = 0;
RTC_DATA_ATTR uint8_t shots[2];

inline int wrap(int v, int n) { return ((v % n) + n) % n; }

inline void startPlacement(int p) {
  placeP = (uint8_t)p;
  placeIdx = 0;
  st = 0;
  col = 0;
  rowSel = 0;
  warn = 0;
  if (style == 0) {
    randomFleet(fl[p]);
    placeIdx = SHIPS;
  } else {
    memset(&fl[p], 0, sizeof(Fleet));
  }
}

inline void beginBattle() {
  turn = 0;
  st = 0;
  col = 4;
  rowSel = 4;
  myRes = rbRes = 0;
  if (vsRobot) {
    phase = PH_PLAY;
  } else {
    passFor = 0;
    passNext = PH_PLAY;
    phase = PH_PASS;
  }
}

inline void donePlacement() {
  if (vsRobot || placeP == 1) {
    beginBattle();
  } else {
    startPlacement(1);
    passFor = 1;
    passNext = PH_PLACE;
    phase = PH_PASS;
  }
}

inline void newGame() {
  stirRng();
  memset(fl, 0, sizeof(fl));
  shots[0] = shots[1] = 0;
  winner = 0;
  if (vsRobot) randomFleet(fl[1]);
  startPlacement(0);
  phase = PH_PLACE;
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

// the next valid direction (0 sideways, 1 up/down, 2 cancel) in direction dir
inline void cycleOri(int dir) {
  for (int k = 1; k <= 3; k++) {
    int o = wrap((int)ori + dir * k, 3);
    if (o == 2 || canPlace(fl[placeP], SIZE[placeIdx], col, rowSel, o)) { ori = (uint8_t)o; return; }
  }
}

// ---------------- buttons ----------------
inline void move(int dir) {
  warn = 0;
  if (phase == PH_MODE || phase == PH_LEVEL || phase == PH_STYLE) {
    menuSel = (uint8_t)(1 - menuSel);
  } else if (phase == PH_PLACE && placeIdx >= SHIPS) {
    randomFleet(fl[placeP]);  // shuffle
  } else if (phase == PH_PLACE || phase == PH_PLAY) {
    int steps = qwGameSteps();
    if (st == 0) col = (uint8_t)wrap(col + dir * steps, N);
    else if (st == 1) rowSel = (uint8_t)wrap(rowSel + dir * steps, N + 1);
    else cycleOri(dir);
  }
}
inline void up() { move(-1); }
inline void down() { move(+1); }

inline void doFire() {
  Fleet &def = fl[1 - turn];
  int c = rowSel * N + col;
  if (def.cell[c] & 8) { warn = 1; return; }
  int ship = 0;
  int r = fire(def, c, ship);
  shots[turn]++;
  myRes = (uint8_t)(r + 1);
  myCell = (uint8_t)c;
  myShip = (uint8_t)(ship < 0 ? 0 : ship);
  if (allSunk(def)) { winner = turn; phase = PH_OVER; return; }
  if (vsRobot) {
    int s2 = 0, rc = robotPick(fl[0], hard);
    int rr = fire(fl[0], rc, s2);
    shots[1]++;
    rbRes = (uint8_t)(rr + 1);
    rbCell = (uint8_t)rc;
    rbShip = (uint8_t)(s2 < 0 ? 0 : s2);
    if (allSunk(fl[0])) { winner = 1; phase = PH_OVER; return; }
    st = 0;
  } else {
    phase = PH_RESULT;
  }
}

inline void select() {
  warn = 0;
  switch (phase) {
    case PH_MODE:
      vsRobot = (menuSel == 0);
      menuSel = 0;
      phase = vsRobot ? PH_LEVEL : PH_STYLE;
      break;
    case PH_LEVEL:
      hard = menuSel;
      menuSel = 0;
      phase = PH_STYLE;
      break;
    case PH_STYLE:
      style = menuSel;
      newGame();
      break;
    case PH_PASS:
      phase = passNext;
      st = 0;
      break;
    case PH_PLACE:
      if (placeIdx >= SHIPS) {  // random fleet accepted
        donePlacement();
      } else if (st == 0) {
        st = 1;
        rowSel = 0;
      } else if (st == 1) {
        if (rowSel == N) {
          st = 0;
        } else {
          ori = 0;
          if (!canPlace(fl[placeP], SIZE[placeIdx], col, rowSel, 0)) {
            if (canPlace(fl[placeP], SIZE[placeIdx], col, rowSel, 1)) ori = 1;
            else { warn = 2; break; }
          }
          st = 2;
        }
      } else if (ori == 2) {
        st = 1;
      } else {
        placeShip(fl[placeP], placeIdx, col, rowSel, ori);
        placeIdx++;
        st = 0;
        if (placeIdx >= SHIPS) donePlacement();
      }
      break;
    case PH_PLAY:
      if (st == 0) {
        st = 1;
        if (rowSel > 9) rowSel = 0;
      } else if (rowSel == N) {
        st = 0;
      } else {
        doFire();
      }
      break;
    case PH_RESULT:
      turn = (uint8_t)(1 - turn);
      passFor = turn;
      passNext = PH_PLAY;
      phase = PH_PASS;
      break;
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t CS = 14, X0 = 16, Y0 = 24;

inline void centerText(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(t), size) / 2, y);
  d.print(t);
}

inline void leftText(Adafruit_GFX &d, const char *t, int16_t x, int16_t y) {
  qwSetText(d, 1);
  d.setCursor(x, y);
  d.print(t);
}

inline void drawOptionScreen(Adafruit_GFX &d, const char *title, const char *a, const char *b,
                             const char *note) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(6, 4);
  d.print(title);
  d.drawFastHLine(0, 24, 200, qwInk());
  const char *opt[2] = {a, b};
  for (int i = 0; i < 2; i++) {
    int16_t y = 50 + i * 36;
    bool sel = (i == menuSel);
    if (sel) {
      d.fillRect(10, y - 6, 180, 28, qwInk());
      d.setTextColor(qwPaper());
    } else {
      d.drawRect(10, y - 6, 180, 28, qwInk());
      d.setTextColor(qwInk());
    }
    centerText(d, opt[i], y, 2);
  }
  d.setTextColor(qwInk());
  centerText(d, note, 150, 1);
  centerText(d, "UP/DOWN=select  MENU=ok", 186, 1);
}

inline void drawX(Adafruit_GFX &d, int16_t x, int16_t y, uint16_t c) {
  d.drawLine(x + 3, y + 3, x + CS - 4, y + CS - 4, c);
  d.drawLine(x + 4, y + 3, x + CS - 3, y + CS - 4, c);
  d.drawLine(x + CS - 4, y + 3, x + 3, y + CS - 4, c);
  d.drawLine(x + CS - 3, y + 3, x + 4, y + CS - 4, c);
}

// A ship as a black bar with the corners cut.
inline void drawShip(Adafruit_GFX &d, const Fleet &f, int i) {
  uint16_t ink = qwInk(), paper = qwPaper();
  int16_t x = X0 + f.ox[i] * CS + 1, y = Y0 + f.oy[i] * CS + 1;
  int16_t w = (f.ori[i] == 0 ? SIZE[i] * CS : CS) - 2, h = (f.ori[i] == 1 ? SIZE[i] * CS : CS) - 2;
  d.fillRect(x, y, w, h, ink);
  d.drawPixel(x, y, paper);
  d.drawPixel(x + w - 1, y, paper);
  d.drawPixel(x, y + h - 1, paper);
  d.drawPixel(x + w - 1, y + h - 1, paper);
}

// The 10 x 10 grid. reveal = show every ship (your own fleet, game over),
// otherwise only the ships that are sunk. upto = ships placed so far.
inline void drawFleetGrid(Adafruit_GFX &d, const Fleet &f, bool reveal, int upto) {
  uint16_t ink = qwInk(), paper = qwPaper();
  for (int i = 0; i <= N; i++) {
    d.drawFastHLine(X0, Y0 + i * CS, N * CS + 1, ink);
    d.drawFastVLine(X0 + i * CS, Y0, N * CS + 1, ink);
  }
  bool shown[SHIPS];
  for (int i = 0; i < SHIPS; i++) {
    shown[i] = (i < upto) && (reveal || sunk(f, i));
    if (shown[i]) drawShip(d, f, i);
  }
  for (int c = 0; c < CELLS; c++) {
    uint8_t v = f.cell[c];
    if (!(v & 8)) continue;
    int16_t x = X0 + (c % N) * CS, y = Y0 + (c / N) * CS;
    int id = v & 7;
    if (!id) d.fillRect(x + CS / 2 - 1, y + CS / 2 - 1, 3, 3, ink);
    else drawX(d, x, y, shown[id - 1] ? paper : ink);
  }
}

inline void drawLabels(Adafruit_GFX &d) {
  d.setTextColor(qwInk());
  qwSetText(d, 1);
  char b[4];
  for (int i = 0; i < N; i++) {
    b[0] = (char)('A' + i);
    b[1] = 0;
    d.setCursor(X0 + i * CS + 4, 14);
    d.print(b);
    snprintf(b, sizeof(b), "%d", i + 1);
    d.setCursor(i < 9 ? 7 : 1, Y0 + i * CS + 4);
    d.print(b);
  }
}

// Your fleet in miniature (cells of 4 px) at (x0, y0).
inline void drawMini(Adafruit_GFX &d, const Fleet &f, int16_t x0, int16_t y0) {
  uint16_t ink = qwInk();
  d.drawRect(x0, y0, N * 4 + 1, N * 4 + 1, ink);
  for (int c = 0; c < CELLS; c++) {
    uint8_t v = f.cell[c];
    int16_t x = x0 + 1 + (c % N) * 4, y = y0 + 1 + (c / N) * 4;
    if (v & 7) {
      if (v & 8) d.drawRect(x, y, 3, 3, ink);  // hit: hollow
      else d.fillRect(x, y, 3, 3, ink);        // ship
    } else if (v & 8) {
      d.drawPixel(x + 1, y + 1, ink);          // miss
    }
  }
}

// Fleet status as small bars: filled = damaged (own fleet) / sunk (enemy).
inline void drawBars(Adafruit_GFX &d, const Fleet &f, bool own, int16_t x0, int16_t y0) {
  uint16_t ink = qwInk();
  for (int i = 0; i < SHIPS; i++)
    for (int k = 0; k < SIZE[i]; k++) {
      int16_t x = x0 + k * 5, y = y0 + i * 7;
      bool dmg = own ? (f.cell[shipCell(f, i, k)] & 8) : sunk(f, i);
      if (dmg) d.fillRect(x, y, 5, 4, ink);
      else d.drawRect(x, y, 5, 4, ink);
    }
}

inline void thickRect(Adafruit_GFX &d, int16_t x, int16_t y, int16_t w, int16_t h) {
  d.drawRect(x - 1, y - 1, w + 2, h + 2, qwInk());
  d.drawRect(x, y, w, h, qwInk());
  d.drawRect(x + 1, y + 1, w - 2, h - 2, qwInk());
}

inline void cellName(char *b, int c) { snprintf(b, 4, "%c%d", 'A' + c % N, c / N + 1); }

inline void shotText(char *b, size_t n, const char *who, int res, int cell, int ship) {
  char cn[4];
  cellName(cn, cell);
  if (res == 1) snprintf(b, n, "%s MISS %s", who, cn);
  else if (res == 2) snprintf(b, n, "%s HIT %s", who, cn);
  else if (res == 3) snprintf(b, n, "%s SUNK %s (%s)", who, NAME[ship], cn);
  else b[0] = 0;
}

inline void line(Adafruit_GFX &d, const char *t, int n) {  // status line n = 0..2
  d.setTextColor(qwInk());
  leftText(d, t, 2, 167 + n * 10);
}

// Your fleet / enemy fleet panel on the right (att = who is looking).
inline void drawSide(Adafruit_GFX &d, int att) {
  d.setTextColor(qwInk());
  drawMini(d, fl[att], 157, 24);
  leftText(d, "ENEMY", 160, 68);
  drawBars(d, fl[1 - att], false, 160, 77);
  leftText(d, "YOURS", 160, 116);
  drawBars(d, fl[att], true, 160, 125);
}

inline void drawPlayScreen(Adafruit_GFX &d, bool cursor) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[40];
  if (vsRobot) snprintf(b, sizeof(b), "BATTLESHIP - YOUR SHOTS");
  else snprintf(b, sizeof(b), "PLAYER %d - YOUR SHOTS", turn + 1);
  leftText(d, b, 2, 2);
  drawLabels(d);
  drawFleetGrid(d, fl[1 - turn], false, SHIPS);
  drawSide(d, turn);
  if (cursor) {
    thickRect(d, X0 + col * CS, Y0, CS, N * CS);  // the column
    if (st == 1 && rowSel < N) thickRect(d, X0 + col * CS, Y0 + rowSel * CS, CS, CS);
  }
  char m[40];
  if (vsRobot) {
    shotText(m, sizeof(m), "YOU:", myRes, myCell, myShip);
    line(d, m, 0);
    shotText(m, sizeof(m), "ROBOT:", rbRes, rbCell, rbShip);
    line(d, m, 1);
  } else if (!cursor) {  // PH_RESULT
    shotText(m, sizeof(m), "RESULT:", myRes, myCell, myShip);
    line(d, m, 0);
  }
  if (!cursor) {
    line(d, "MENU = NEXT PLAYER", 2);
  } else if (warn == 1) {
    line(d, "ALREADY SHOT - PICK ANOTHER", 2);
  } else if (st == 0) {
    line(d, "COLUMN: UP/DOWN, MENU=ok", 2);
  } else if (rowSel == N) {
    snprintf(m, sizeof(m), "< BACK TO COLUMN %c", 'A' + col);
    line(d, m, 2);
  } else {
    snprintf(m, sizeof(m), "ROW %d: MENU = FIRE AT %c%d", rowSel + 1, 'A' + col, rowSel + 1);
    line(d, m, 2);
  }
}

inline void drawPlaceScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[40];
  if (vsRobot) snprintf(b, sizeof(b), "PLACE YOUR SHIPS");
  else snprintf(b, sizeof(b), "PLAYER %d - YOUR SHIPS", placeP + 1);
  leftText(d, b, 2, 2);
  drawLabels(d);
  const Fleet &f = fl[placeP];
  drawFleetGrid(d, f, true, placeIdx);
  if (placeIdx < SHIPS) {
    if (st == 2 && ori < 2) {  // preview of the ship
      for (int k = 0; k < SIZE[placeIdx]; k++) {
        int xx = col + (ori == 0 ? k : 0), yy = rowSel + (ori == 1 ? k : 0);
        thickRect(d, X0 + xx * CS + 1, Y0 + yy * CS + 1, CS - 2, CS - 2);
      }
    } else if (st < 2) {
      thickRect(d, X0 + col * CS, Y0, CS, N * CS);
      if (st == 1 && rowSel < N) thickRect(d, X0 + col * CS, Y0 + rowSel * CS, CS, CS);
    }
    // ships still to place
    leftText(d, "TO", 160, 20);
    leftText(d, "PLACE", 160, 30);
    for (int i = placeIdx; i < SHIPS; i++) {
      int16_t y = 42 + (i - placeIdx) * 12;
      if (i == placeIdx) d.fillRect(160, y, SIZE[i] * 5, 8, qwInk());
      else d.drawRect(160, y, SIZE[i] * 5, 8, qwInk());
    }
    snprintf(b, sizeof(b), "%s (%d)", NAME[placeIdx], SIZE[placeIdx]);
    line(d, b, 0);
    if (warn == 2) line(d, "NO ROOM HERE", 1);
    if (st == 0) line(d, "COLUMN: UP/DOWN, MENU=ok", 2);
    else if (st == 1) {
      if (rowSel == N) line(d, "< BACK TO COLUMN", 2);
      else line(d, "ROW: UP/DOWN, MENU=ok", 2);
    } else if (ori == 2) line(d, "CANCEL: MENU = BACK", 2);
    else line(d, ori == 0 ? "SIDEWAYS: UP/DOWN, MENU=PLACE" : "UP-DOWN: UP/DOWN, MENU=PLACE", 2);
  } else {
    line(d, "RANDOM FLEET", 0);
    line(d, "UP/DOWN = shuffle", 1);
    line(d, "MENU = keep this fleet", 2);
  }
}

inline void drawPassScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[16];
  snprintf(b, sizeof(b), "PLAYER %d", passFor + 1);
  centerText(d, b, 50, 3);
  centerText(d, "Hand over the watch", 100, 1);
  centerText(d, passNext == PH_PLACE ? "to place the ships" : "to fire", 112, 1);
  d.drawRect(30, 140, 140, 28, qwInk());
  centerText(d, "MENU = READY", 148, 2);
}

inline void drawOverScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[24];
  if (vsRobot) snprintf(b, sizeof(b), winner == 0 ? "YOU WIN!" : "YOU LOSE");
  else snprintf(b, sizeof(b), "PLAYER %d WINS", winner + 1);
  qwSetText(d, 2);
  d.setCursor(4, 3);
  d.print(b);
  drawFleetGrid(d, fl[1 - winner], true, SHIPS);  // the loser's fleet
  d.setTextColor(qwInk());
  leftText(d, "LOSER'S", 158, 30);
  leftText(d, "FLEET", 158, 40);
  snprintf(b, sizeof(b), "SHOTS: %d", shots[winner]);
  line(d, b, 0);
  line(d, "MENU = NEW GAME", 2);
}

inline void draw(Adafruit_GFX &d) {
  switch (phase) {
    case PH_MODE: drawOptionScreen(d, "BATTLESHIP", "1 PLAYER", "2 PLAYERS", "Sink all 5 enemy ships"); break;
    case PH_LEVEL: drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD", "HARD works out where you hide"); break;
    case PH_STYLE: drawOptionScreen(d, "YOUR FLEET", "RANDOM FLEET", "PLACE MYSELF", "5 ships: 5, 4, 3, 3 and 2 long"); break;
    case PH_PASS: drawPassScreen(d); break;
    case PH_PLACE: drawPlaceScreen(d); break;
    case PH_PLAY: drawPlayScreen(d, true); break;
    case PH_RESULT: drawPlayScreen(d, false); break;
    default: drawOverScreen(d); break;
  }
}

}  // namespace Bs
