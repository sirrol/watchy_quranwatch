#pragma once
/*
  GameDomineering.h
  ------------------------------------------------------------
  DOMINEERING for QuranWatch (included by Games.h).

  Two players place dominoes (two squares) on a grid, on empty squares
  only. VERTICAL places upright dominoes and plays first, HORIZONTAL
  places lying dominoes. The first player who cannot place a domino
  LOSES - so every domino you place also takes room from the other
  player, and you want to keep room for yourself.

  Board: 6x6 or 8x8 (your choice).

  1 player: you pick Vertical (first) or Horizontal (second), the watch
  plays the other one. Robot levels: EASY (random legal moves) and HARD
  (looks several moves ahead for at most ~2.5 seconds, trying to keep
  more moves for itself than for you).

  Moving: UP/DOWN walk through the places where a domino fits (hold the
  button to jump 3 or 6 places), MENU places it. The frame shows the
  domino that would be placed.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Dm {

constexpr int MAXN = 8, MAXC = MAXN * MAXN;

// ---------------- board and rules ----------------
struct Bd {
  uint8_t occ[MAXC];   // 0 empty, 1 = vertical domino, 2 = horizontal domino
  uint8_t half[MAXC];  // 1 = first square (top / left), 2 = second square
  uint8_t n;
};

// Can player w (1 vertical, 2 horizontal) place a domino whose first
// square (top / left) is c?
inline bool legalAt(const Bd &b, int w, int c) {
  int n = b.n;
  if (b.occ[c]) return false;
  if (w == 1) return (c / n) < n - 1 && !b.occ[c + n];
  return (c % n) < n - 1 && !b.occ[c + 1];
}

inline int countMoves(const Bd &b, int w) {
  int k = 0;
  for (int c = 0; c < b.n * b.n; c++) if (legalAt(b, w, c)) k++;
  return k;
}

inline bool hasMove(const Bd &b, int w) {
  for (int c = 0; c < b.n * b.n; c++) if (legalAt(b, w, c)) return true;
  return false;
}

inline void put(Bd &b, int w, int c) {
  int c2 = (w == 1) ? c + b.n : c + 1;
  b.occ[c] = b.occ[c2] = (uint8_t)w;
  b.half[c] = 1;
  b.half[c2] = 2;
}

inline void take(Bd &b, int w, int c) {
  int c2 = (w == 1) ? c + b.n : c + 1;
  b.occ[c] = b.occ[c2] = 0;
  b.half[c] = b.half[c2] = 0;
}

inline void clearBoard(Bd &b, int n) {
  b.n = (uint8_t)n;
  for (int i = 0; i < MAXC; i++) { b.occ[i] = 0; b.half[i] = 0; }
}

// ---------------- robot ----------------
struct Move { uint8_t cell; int key; };

// The `maxN` most promising moves of w, best first. key = how many
// more moves w has than the opponent once the domino is placed.
inline int genMoves(Bd &b, int w, Move *out, int maxN) {
  int cnt = 0;
  for (int c = 0; c < b.n * b.n; c++) {
    if (!legalAt(b, w, c)) continue;
    put(b, w, c);
    Move m = {(uint8_t)c, countMoves(b, w) - countMoves(b, 3 - w)};
    take(b, w, c);
    if (cnt < maxN) {
      int i = cnt++;
      while (i > 0 && out[i - 1].key < m.key) { out[i] = out[i - 1]; i--; }
      out[i] = m;
    } else if (m.key > out[cnt - 1].key) {
      int i = cnt - 1;
      while (i > 0 && out[i - 1].key < m.key) { out[i] = out[i - 1]; i--; }
      out[i] = m;
    }
  }
  return cnt;
}

inline uint32_t rngState = 123456789u;
inline uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}
inline unsigned long hardLimitMs = 2500;  // time the HARD robot may think

struct Search {
  unsigned long t0 = 0, limitMs = 2500;
  uint32_t nodes = 0;
  bool timeUp = false;
};

constexpr int BEAM = 8, WIN = 10000;

// Negamax with alpha-beta; value from the point of view of w (to move).
inline int negamax(Bd &b, int w, int depth, int alpha, int beta, Search &S) {
  if ((++S.nodes & 31) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  Move mv[BEAM];
  int n = genMoves(b, w, mv, BEAM);
  if (n == 0) return -WIN - depth;  // w cannot move: w loses
  if (depth <= 1) return mv[0].key;
  int best = -1000000;
  for (int i = 0; i < n; i++) {
    put(b, w, mv[i].cell);
    int v = -negamax(b, 3 - w, depth - 1, -beta, -alpha, S);
    take(b, w, mv[i].cell);
    if (S.timeUp) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

inline int chooseHard(Bd &b, int w, unsigned long limitMs) {
  Move root[14];
  int n = genMoves(b, w, root, 14);
  if (n == 0) return -1;
  int best = root[0].cell;
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  for (int depth = 1; depth <= 40; depth++) {
    int bestV = -1000000, cnt = 0;
    int list[14];
    for (int i = 0; i < n; i++) {
      put(b, w, root[i].cell);
      int v = (depth <= 1) ? root[i].key
                           : -negamax(b, 3 - w, depth - 1, -1000000, 1000000, S);
      take(b, w, root[i].cell);
      if (S.timeUp) break;
      if (v > bestV) { bestV = v; cnt = 0; }
      if (v == bestV) list[cnt++] = root[i].cell;
    }
    if (S.timeUp) break;  // incomplete depth: ignore it
    best = list[rnd() % cnt];
    if (bestV >= WIN || bestV <= -WIN) break;  // game decided
  }
  return best;
}

inline int chooseEasy(Bd &b, int w) {
  int cells[MAXC], n = 0;
  for (int c = 0; c < b.n * b.n; c++) if (legalAt(b, w, c)) cells[n++] = c;
  return n ? cells[rnd() % n] : -1;
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_SIZE, PH_PLAY, PH_OVER };

RTC_DATA_ATTR Bd bd;
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanVertical = 1;  // 1P: you play Vertical (first)
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t turn = 1;           // 1 = vertical to move, 2 = horizontal
RTC_DATA_ATTR uint8_t cursor = 0;         // first square of the domino to place
RTC_DATA_ATTR uint8_t result = 0;         // 1 vertical wins, 2 horizontal wins
RTC_DATA_ATTR uint8_t lastMove = 255;

inline bool robotTurn() {
  return vsRobot && ((turn == 1) != (humanVertical != 0));
}

template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

inline void keepCursorLegal() {
  if (legalAt(bd, turn, cursor)) return;
  for (int c = 0; c < bd.n * bd.n; c++)
    if (legalAt(bd, turn, c)) { cursor = (uint8_t)c; return; }
}

inline void placeMove(int c) {
  put(bd, turn, c);
  lastMove = (uint8_t)c;
  turn = (uint8_t)(3 - turn);
}

// After every move: the player who cannot move loses; the robot plays.
inline void advance() {
  for (int guard = 0; guard < 100; guard++) {
    if (!hasMove(bd, turn)) {
      result = (uint8_t)(3 - turn);  // the player who just moved wins
      phase = PH_OVER;
      return;
    }
    if (!robotTurn()) break;
    rngState ^= (uint32_t)millis() * 2654435761u;
    int m = hard ? chooseHard(bd, turn, hardLimitMs) : chooseEasy(bd, turn);
    if (m < 0) break;
    placeMove(m);
  }
  phase = PH_PLAY;
  keepCursorLegal();
}

inline void newGame(int size) {
  clearBoard(bd, size);
  turn = 1;
  result = 0;
  lastMove = 255;
  cursor = (uint8_t)((size / 2) * size + size / 2 - 1);
  advance();
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

// ---------------- buttons ----------------
inline void move(int dir) {
  if (phase == PH_MODE || phase == PH_ROLE || phase == PH_LEVEL ||
      phase == PH_SIZE) {
    menuSel = (uint8_t)(1 - menuSel);
  } else if (phase == PH_PLAY) {
    for (int s = qwGameSteps(); s > 0; s--)
      cursor = (uint8_t)cycle(cursor, dir, bd.n * bd.n, [](int i) {
        return legalAt(bd, turn, i);
      });
  }
}

inline void up() { move(-1); }
inline void down() { move(+1); }

inline void select() {
  switch (phase) {
    case PH_MODE:
      vsRobot = (menuSel == 0);
      menuSel = 0;
      phase = vsRobot ? PH_ROLE : PH_SIZE;
      break;
    case PH_ROLE:
      humanVertical = (menuSel == 0);
      phase = PH_LEVEL;
      menuSel = 0;
      break;
    case PH_LEVEL:
      hard = menuSel;
      phase = PH_SIZE;
      menuSel = 0;
      break;
    case PH_SIZE:
      newGame(menuSel == 0 ? 6 : 8);
      break;
    case PH_PLAY:
      if (!legalAt(bd, turn, cursor)) break;
      placeMove(cursor);
      advance();
      break;
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t X0 = 16, Y0 = 30, BOARD_PX = 168;

inline void centerText(Adafruit_GFX &d, const char *t, int16_t y,
                       uint8_t size) {
  qwSetText(d, size);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(t), size) / 2, y);
  d.print(t);
}

inline void drawOptionScreen(Adafruit_GFX &d, const char *title,
                             const char *a, const char *b, const char *note) {
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

inline void drawBoardScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  int n = bd.n, cs = BOARD_PX / n;

  const char *nV = vsRobot ? (humanVertical ? "YOU" : "ME") : "VERTICAL";
  const char *nH = vsRobot ? (humanVertical ? "ME" : "YOU") : "HORIZONTAL";

  char l1[40];
  if (phase == PH_OVER) {
    const char *r;
    if (vsRobot) r = ((result == 1) == (humanVertical != 0)) ? "YOU WIN!" : "I WIN";
    else r = (result == 1) ? "VERTICAL WINS" : "HORIZ. WINS";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    snprintf(l1, sizeof(l1), "%s HAS NO MOVE LEFT",
             result == 1 ? nH : nV);
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 19);
    d.print(l1);
  } else {
    snprintf(l1, sizeof(l1), "%s (%s): PLACE A DOMINO",
             turn == 1 ? nV : nH, turn == 1 ? "UPRIGHT" : "LYING");
    if (strlen(l1) > 33)
      snprintf(l1, sizeof(l1), "%s: PLACE A DOMINO", turn == 1 ? nV : nH);
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 3);
    d.print(l1);
    centerText(d, "UP/DOWN=pick (hold=fast)", 14, 1);
  }

  // grid
  d.drawRect(X0 - 2, Y0 - 2, n * cs + 4, n * cs + 4, qwInk());
  for (int i = 1; i < n; i++) {
    d.drawFastVLine(X0 + i * cs, Y0, n * cs, qwInk());
    d.drawFastHLine(X0, Y0 + i * cs, n * cs, qwInk());
  }
  // dominoes
  for (int c = 0; c < n * n; c++) {
    if (!bd.occ[c] || bd.half[c] != 1) continue;
    int16_t bx = X0 + (c % n) * cs, by = Y0 + (c / n) * cs;
    if (bd.occ[c] == 1) {  // upright: black with a white middle line
      d.fillRect(bx + 2, by + 2, cs - 3, 2 * cs - 3, qwInk());
      d.drawFastHLine(bx + 5, by + cs, cs - 9, qwPaper());
      if (c == lastMove && phase != PH_OVER)
        d.fillRect(bx + cs / 2 - 1, by + 5, 3, 3, qwPaper());
    } else {               // lying: white, double frame, black middle line
      d.fillRect(bx + 1, by + 1, 2 * cs - 1, cs - 1, qwPaper());
      d.drawRect(bx + 2, by + 2, 2 * cs - 3, cs - 3, qwInk());
      d.drawRect(bx + 3, by + 3, 2 * cs - 5, cs - 5, qwInk());
      d.drawFastVLine(bx + cs, by + 6, cs - 11, qwInk());
      if (c == lastMove && phase != PH_OVER)
        d.fillRect(bx + 6, by + cs / 2 - 1, 3, 3, qwInk());
    }
  }
  // cursor: frame around the two squares of the domino to place
  if (phase == PH_PLAY && legalAt(bd, turn, cursor)) {
    int16_t bx = X0 + (cursor % n) * cs, by = Y0 + (cursor / n) * cs;
    int16_t w = (turn == 1) ? cs : 2 * cs, h = (turn == 1) ? 2 * cs : cs;
    d.drawRect(bx, by, w + 1, h + 1, qwInk());
    d.drawRect(bx + 1, by + 1, w - 1, h - 1, qwInk());
    d.drawRect(bx + 2, by + 2, w - 3, h - 3, qwInk());
    // small dot in the middle of each square
    d.fillRect(bx + cs / 2 - 1, by + cs / 2 - 1, 3, 3, qwInk());
    if (turn == 1) d.fillRect(bx + cs / 2 - 1, by + cs + cs / 2 - 1, 3, 3, qwInk());
    else d.fillRect(bx + cs + cs / 2 - 1, by + cs / 2 - 1, 3, 3, qwInk());
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "DOMINEERING", "1 PLAYER", "2 PLAYERS",
                     "No room left for you = you lose");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOU PLAY", "VERTICAL", "HORIZONTAL",
                     "Vertical plays first");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                     "HARD thinks up to 2-3 seconds");
  } else if (phase == PH_SIZE) {
    drawOptionScreen(d, "BOARD SIZE", "6 x 6", "8 x 8",
                     "8 x 8 makes a longer game");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Dm
