#pragma once
/*
  GameBlackHole.h
  ------------------------------------------------------------
  BLACK HOLE for QuranWatch (included by Games.h).

  21 circles arranged in a triangle (6 rows). Players take turns
  placing their next number in any empty circle: each player has the
  numbers 1 to 10, placed in increasing order (1, 1, 2, 2, 3, 3...).
  After 20 placements one circle is still empty: it is the BLACK HOLE.
  It swallows the numbers of the circles touching it. Each player adds
  up his swallowed numbers - the LOWER total wins (equal = draw).

  The first player is drawn as white circles, the second player as
  black circles. The number to place is chosen for you.

  1 player: you pick first or second, the watch plays the other one.
  Robot levels: EASY (greedy, a little random) and HARD (looks ahead
  as far as it can in ~2.5 seconds, and plays perfectly once about 9
  circles are left).

  Moving: UP/DOWN walk through the EMPTY circles only (hold the button
  to jump 3 or 6 circles), MENU places the number.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Bh {

constexpr int NCELL = 21, NMOVES = 20;

// ---------------- the triangle ----------------
inline uint8_t NB[NCELL][6];
inline uint8_t NBN[NCELL];
inline bool tablesReady = false;

inline int idx(int r, int c) { return r * (r + 1) / 2 + c; }

inline void initTables() {
  if (tablesReady) return;
  for (int r = 0; r < 6; r++)
    for (int c = 0; c <= r; c++) {
      int i = idx(r, c), n = 0;
      if (c > 0) NB[i][n++] = (uint8_t)idx(r, c - 1);
      if (c < r) NB[i][n++] = (uint8_t)idx(r, c + 1);
      if (r > 0 && c > 0) NB[i][n++] = (uint8_t)idx(r - 1, c - 1);
      if (r > 0 && c < r) NB[i][n++] = (uint8_t)idx(r - 1, c);
      if (r < 5) {
        NB[i][n++] = (uint8_t)idx(r + 1, c);
        NB[i][n++] = (uint8_t)idx(r + 1, c + 1);
      }
      NBN[i] = (uint8_t)n;
    }
  tablesReady = true;
}

// ---------------- evaluation / search ----------------
struct Board {
  uint8_t val[NCELL];  // 0 empty, else the number 1..10
  uint8_t own[NCELL];  // 1 = first player, 2 = second player
};

// Value for the FIRST player when `hole` is the black hole.
inline int holeValue(const Board &b, int hole) {
  int s1 = 0, s2 = 0;
  for (int i = 0; i < NBN[hole]; i++) {
    int c = NB[hole][i];
    if (!b.val[c]) continue;
    if (b.own[c] == 1) s1 += b.val[c]; else s2 += b.val[c];
  }
  int diff = s2 - s1;  // > 0: first player swallowed less
  return diff > 0 ? 1000 + diff : (diff < 0 ? -1000 + diff : 0);
}

// Guess for an unfinished game: average over the possible holes.
inline int evalLeaf(const Board &b) {
  int sum = 0, n = 0;
  for (int h = 0; h < NCELL; h++)
    if (!b.val[h]) { sum += holeValue(b, h); n++; }
  return n ? sum / n : 0;
}

inline uint32_t rngState = 362436069u;
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

// k = number of placements already made. Value for the first player.
inline int search(Board &b, int k, int depth, int alpha, int beta, Search &S) {
  if ((++S.nodes & 255) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  if (k == NMOVES) {
    for (int h = 0; h < NCELL; h++) if (!b.val[h]) return holeValue(b, h);
    return 0;
  }
  if (depth == 0) return evalLeaf(b);
  bool firstMoves = (k % 2 == 0);
  uint8_t num = (uint8_t)(k / 2 + 1), who = firstMoves ? 1 : 2;
  int best = firstMoves ? -100000 : 100000;
  for (int c = 0; c < NCELL; c++) {
    if (b.val[c]) continue;
    b.val[c] = num;
    b.own[c] = who;
    int v = search(b, k + 1, depth - 1, alpha, beta, S);
    b.val[c] = 0;
    b.own[c] = 0;
    if (S.timeUp) return 0;
    if (firstMoves) {
      if (v > best) best = v;
      if (best > alpha) alpha = best;
    } else {
      if (v < best) best = v;
      if (best < beta) beta = best;
    }
    if (alpha >= beta) break;
  }
  return best;
}

inline int chooseHard(Board &b, int k, unsigned long limitMs) {
  bool firstMoves = (k % 2 == 0);
  uint8_t num = (uint8_t)(k / 2 + 1), who = firstMoves ? 1 : 2;
  int freeCells[NCELL], n = 0;
  for (int c = 0; c < NCELL; c++) if (!b.val[c]) freeCells[n++] = c;
  if (n == 0) return -1;
  int bestMove = freeCells[rnd() % n];
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  int maxDepth = NMOVES - k;
  for (int depth = 1; depth <= maxDepth; depth++) {
    int bestV = firstMoves ? -100000 : 100000;
    int list[NCELL], cnt = 0;
    for (int i = 0; i < n; i++) {
      int c = freeCells[i];
      b.val[c] = num;
      b.own[c] = who;
      int v = search(b, k + 1, depth - 1, -100000, 100000, S);
      b.val[c] = 0;
      b.own[c] = 0;
      if (S.timeUp) break;
      bool better = firstMoves ? (v > bestV) : (v < bestV);
      if (better) { bestV = v; cnt = 0; }
      if (v == bestV) list[cnt++] = c;
    }
    if (S.timeUp) break;  // incomplete depth: ignore it
    bestMove = list[rnd() % cnt];
  }
  return bestMove;
}

// Greedy: best guess one move ahead, picked among the 4 best.
inline int chooseEasy(Board &b, int k) {
  bool firstMoves = (k % 2 == 0);
  uint8_t num = (uint8_t)(k / 2 + 1), who = firstMoves ? 1 : 2;
  int cell[NCELL], key[NCELL], n = 0;
  for (int c = 0; c < NCELL; c++) {
    if (b.val[c]) continue;
    b.val[c] = num;
    b.own[c] = who;
    int v = evalLeaf(b);
    b.val[c] = 0;
    b.own[c] = 0;
    // insertion sort, best for the mover first
    int kv = firstMoves ? v : -v, i = n++;
    while (i > 0 && key[i - 1] < kv) { key[i] = key[i - 1]; cell[i] = cell[i - 1]; i--; }
    key[i] = kv;
    cell[i] = c;
  }
  if (n == 0) return -1;
  int top = n < 4 ? n : 4;
  return cell[rnd() % top];
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_PLAY, PH_OVER };

RTC_DATA_ATTR Board bd;
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanFirst = 1;  // 1P: you place the first number
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t placed = 0;      // placements made (0..20)
RTC_DATA_ATTR uint8_t cursor = 10;
RTC_DATA_ATTR uint8_t result = 0;      // 1 first wins, 2 second wins, 3 draw
RTC_DATA_ATTR uint8_t hole = 255;
RTC_DATA_ATTR uint8_t sum1 = 0, sum2 = 0;
RTC_DATA_ATTR uint8_t lastMove = 255;

inline bool robotTurn() {
  bool firstMoves = (placed % 2 == 0);
  return vsRobot && (firstMoves != (humanFirst != 0));
}

template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

inline void keepCursorFree() {
  if (!bd.val[cursor]) return;
  for (int c = 0; c < NCELL; c++) if (!bd.val[c]) { cursor = (uint8_t)c; return; }
}

inline void playMove(int c) {
  initTables();
  bool firstMoves = (placed % 2 == 0);
  bd.val[c] = (uint8_t)(placed / 2 + 1);
  bd.own[c] = firstMoves ? 1 : 2;
  lastMove = (uint8_t)c;
  placed++;
  if (placed == NMOVES) {
    for (int h = 0; h < NCELL; h++) if (!bd.val[h]) hole = (uint8_t)h;
    int s1 = 0, s2 = 0;
    for (int i = 0; i < NBN[hole]; i++) {
      int n = NB[hole][i];
      if (bd.own[n] == 1) s1 += bd.val[n]; else s2 += bd.val[n];
    }
    sum1 = (uint8_t)s1;
    sum2 = (uint8_t)s2;
    result = (s1 < s2) ? 1 : (s2 < s1 ? 2 : 3);
    phase = PH_OVER;
  }
}

inline void robotPlay() {
  rngState ^= (uint32_t)millis() * 2654435761u;
  int c = hard ? chooseHard(bd, placed, hardLimitMs) : chooseEasy(bd, placed);
  if (c >= 0) playMove(c);
}

inline void newGame() {
  initTables();
  for (int i = 0; i < NCELL; i++) { bd.val[i] = 0; bd.own[i] = 0; }
  placed = 0;
  result = 0;
  hole = 255;
  lastMove = 255;
  cursor = 10;
  phase = PH_PLAY;
  if (robotTurn()) robotPlay();  // the watch plays first
  keepCursorFree();
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

// ---------------- buttons ----------------
inline void move(int dir) {
  if (phase == PH_MODE || phase == PH_ROLE || phase == PH_LEVEL) {
    menuSel = (uint8_t)(1 - menuSel);
  } else if (phase == PH_PLAY) {
    for (int s = qwGameSteps(); s > 0; s--)
      cursor = (uint8_t)cycle(cursor, dir, NCELL,
                              [](int i) { return bd.val[i] == 0; });
  }
}

inline void up() { move(-1); }
inline void down() { move(+1); }

inline void select() {
  switch (phase) {
    case PH_MODE:
      vsRobot = (menuSel == 0);
      if (vsRobot) { phase = PH_ROLE; menuSel = 0; }
      else newGame();
      break;
    case PH_ROLE:
      humanFirst = (menuSel == 0);
      phase = PH_LEVEL;
      menuSel = 0;
      break;
    case PH_LEVEL:
      hard = menuSel;
      newGame();
      break;
    case PH_PLAY:
      if (bd.val[cursor]) break;
      playMove(cursor);
      if (phase != PH_OVER && robotTurn()) robotPlay();
      if (phase != PH_OVER) keepCursorFree();
      break;
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t DX = 30, DY = 26, TOP_Y = 50, RAD = 13;

inline int16_t cx(int r, int c) { return (int16_t)(100 + (2 * c - r) * DX / 2); }
inline int16_t cy(int r) { return (int16_t)(TOP_Y + r * DY); }

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

inline void drawNumber(Adafruit_GFX &d, int16_t x, int16_t y, int n,
                       uint16_t color) {
  char t[4];
  snprintf(t, sizeof(t), "%d", n);
  d.setTextColor(color);
  qwSetText(d, 2);
  int16_t w = qwTextInkWidth((uint8_t)strlen(t), 2);
  d.setCursor(x - w / 2, y - 7);
  d.print(t);
}

inline void drawBoardScreen(Adafruit_GFX &d) {
  initTables();
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());

  // who is who
  const char *n1 = vsRobot ? (humanFirst ? "YOU" : "ME") : "P1";
  const char *n2 = vsRobot ? (humanFirst ? "ME" : "YOU") : "P2";

  char l1[40];
  if (phase == PH_OVER) {
    const char *r = "DRAW";
    if (result == 1) r = vsRobot ? (humanFirst ? "YOU WIN!" : "I WIN") : "P1 WINS!";
    else if (result == 2) r = vsRobot ? (humanFirst ? "I WIN" : "YOU WIN!") : "P2 WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    snprintf(l1, sizeof(l1), "SWALLOWED: %s %d  %s %d", n1, sum1, n2, sum2);
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 19);
    d.print(l1);
    centerText(d, "MENU=new game  BACK=exit", 28, 1);
  } else {
    bool firstMoves = (placed % 2 == 0);
    snprintf(l1, sizeof(l1), "%s (%s): PLACE %d", firstMoves ? n1 : n2,
             firstMoves ? "WHITE" : "BLACK", placed / 2 + 1);
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 3);
    d.print(l1);
    centerText(d, "UP/DOWN=pick (hold=fast)", 14, 1);
  }

  // swallowed circles: extra ring
  if (phase == PH_OVER && hole != 255) {
    for (int i = 0; i < NBN[hole]; i++) {
      int c = NB[hole][i];
      int r = 0;
      while (idx(r + 1, 0) <= c) r++;
      int col = c - idx(r, 0);
      d.drawCircle(cx(r, col), cy(r), RAD + 2, qwInk());
      d.drawCircle(cx(r, col), cy(r), RAD + 3, qwInk());
    }
  }

  for (int r = 0; r < 6; r++)
    for (int col = 0; col <= r; col++) {
      int c = idx(r, col);
      int16_t x = cx(r, col), y = cy(r);
      if (bd.val[c]) {
        if (bd.own[c] == 1) {
          d.drawCircle(x, y, RAD, qwInk());
          d.drawCircle(x, y, RAD - 1, qwInk());
          drawNumber(d, x, y, bd.val[c], qwInk());
        } else {
          d.fillCircle(x, y, RAD, qwInk());
          drawNumber(d, x, y, bd.val[c], qwPaper());
        }
        if (c == lastMove && phase != PH_OVER) {  // last move: dot above
          d.fillRect(x - 1, y - RAD - 4, 3, 3, qwInk());
        }
      } else if (phase == PH_OVER && c == hole) {  // the black hole
        d.fillCircle(x, y, RAD, qwInk());
        d.fillCircle(x, y, RAD - 4, qwPaper());
        d.fillCircle(x, y, RAD - 8, qwInk());
      } else {
        d.drawCircle(x, y, RAD, qwInk());
      }
      if (phase == PH_PLAY && c == cursor) {
        d.drawCircle(x, y, RAD + 2, qwInk());
        d.drawCircle(x, y, RAD + 3, qwInk());
        d.fillRect(x - 2, y - 2, 5, 5, qwInk());
      }
    }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "BLACK HOLE", "1 PLAYER", "2 PLAYERS",
                     "Lowest swallowed total wins");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOU PLAY", "FIRST (WHITE)", "SECOND (BLACK)",
                     "Each player places 1 to 10");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                     "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Bh
