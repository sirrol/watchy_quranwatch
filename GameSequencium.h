#pragma once
/*
  GameSequencium.h
  ------------------------------------------------------------
  SEQUENCIUM for QuranWatch (included by Games.h).

  A 6x6 board. Each player starts with the chain 1-2-3 on the main
  diagonal: the first player (white) in the top-left corner, the
  second player (black) in the bottom-right corner. On your turn you
  pick one of YOUR numbers, add one to it and write the result in an
  EMPTY square touching it (sideways or diagonally). The play goes on
  until the board is full, even if one player can no longer move (he
  passes and the other keeps playing). The player with the HIGHEST
  number wins (equal = draw, the rules do not say).

  1 player: you pick first or second, the watch plays the other one.
  Robot levels: EASY (picks among its 3 most natural moves) and HARD
  (looks several moves ahead for at most ~2.5 seconds).

  Moving: little dots mark the squares where you may play. UP/DOWN
  walk through those squares only (hold the button to jump 3 or 6),
  MENU chooses one. If the square touches several of your numbers,
  you then choose which one to extend (the highest is proposed, its
  square is circled), or go back.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Sq {

constexpr int MAXN = 6, MAXC = MAXN * MAXN;

// ---------------- board and rules ----------------
struct Bd {
  uint8_t val[MAXC];  // 0 empty, else the number written
  uint8_t own[MAXC];  // 1 = first player, 2 = second player
  uint8_t n;          // board side (always 6)
};

// Neighbours (8 directions) of cell c. Returns how many.
inline int neighbours(int n, int c, uint8_t *out) {
  int r = c / n, col = c % n, k = 0;
  for (int dr = -1; dr <= 1; dr++)
    for (int dc = -1; dc <= 1; dc++) {
      if (!dr && !dc) continue;
      int rr = r + dr, cc = col + dc;
      if (rr < 0 || rr >= n || cc < 0 || cc >= n) continue;
      out[k++] = (uint8_t)(rr * n + cc);
    }
  return k;
}

// Distinct values of w's numbers touching cell c, ascending.
inline int sourceValues(const Bd &b, int c, int w, uint8_t *vals) {
  uint8_t nb[8];
  int k = neighbours(b.n, c, nb), cnt = 0;
  for (int i = 0; i < k; i++) {
    int q = nb[i];
    if (b.val[q] && b.own[q] == w) {
      uint8_t v = b.val[q];
      int j = cnt;
      bool dup = false;
      for (int m = 0; m < cnt; m++) if (vals[m] == v) dup = true;
      if (dup) continue;
      vals[cnt++] = v;
      while (j > 0 && vals[j - 1] > v) { vals[j] = vals[j - 1]; j--; }
      vals[j] = v;
    }
  }
  return cnt;
}

inline bool legalTarget(const Bd &b, int c, int w) {
  if (b.val[c]) return false;
  uint8_t nb[8];
  int k = neighbours(b.n, c, nb);
  for (int i = 0; i < k; i++)
    if (b.val[nb[i]] && b.own[nb[i]] == w) return true;
  return false;
}

inline bool hasMove(const Bd &b, int w) {
  for (int c = 0; c < b.n * b.n; c++) if (legalTarget(b, c, w)) return true;
  return false;
}

inline int maxOf(const Bd &b, int w) {
  int m = 0;
  for (int c = 0; c < b.n * b.n; c++)
    if (b.val[c] && b.own[c] == w && b.val[c] > m) m = b.val[c];
  return m;
}

inline void clearBoard(Bd &b, int n) {
  b.n = (uint8_t)n;
  for (int i = 0; i < MAXC; i++) { b.val[i] = 0; b.own[i] = 0; }
  for (int k = 0; k < 3; k++) {
    b.val[k * (n + 1)] = (uint8_t)(k + 1);                  // 1-2-3 from the top-left
    b.own[k * (n + 1)] = 1;
    b.val[(n - 1 - k) * (n + 1)] = (uint8_t)(k + 1);        // 1-2-3 from the bottom-right
    b.own[(n - 1 - k) * (n + 1)] = 2;
  }
}

// ---------------- robot ----------------
struct Move { uint8_t cell; uint8_t newVal; int key; };

// The `maxN` most natural moves of w, best first.
inline int genMoves(const Bd &b, int w, Move *out, int maxN) {
  int total = b.n * b.n, cnt = 0;
  int opp = 3 - w, oppMax = maxOf(b, opp);
  for (int c = 0; c < total; c++) {
    if (b.val[c]) continue;
    uint8_t vals[8];
    int nv = sourceValues(b, c, w, vals);
    if (!nv) continue;
    uint8_t nb[8];
    int k = neighbours(b.n, c, nb);
    int freeN = 0, blocks = 0;
    for (int i = 0; i < k; i++) {
      if (!b.val[nb[i]]) freeN++;
      else if (b.own[nb[i]] == opp && b.val[nb[i]] == oppMax) blocks++;
    }
    for (int j = 0; j < nv; j++) {
      Move m = {(uint8_t)c, (uint8_t)(vals[j] + 1),
                (vals[j] + 1) * 10 + 2 * freeN + 15 * blocks};
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
  }
  return cnt;
}

// Empty squares touching w's highest number(s) / any of w's numbers.
inline void freedom(const Bd &b, int w, int mx, int &heads, int &any) {
  heads = 0; any = 0;
  for (int c = 0; c < b.n * b.n; c++) {
    if (b.val[c]) continue;
    uint8_t nb[8];
    int k = neighbours(b.n, c, nb);
    bool a = false, h = false;
    for (int i = 0; i < k; i++) {
      int q = nb[i];
      if (b.val[q] && b.own[q] == w) {
        a = true;
        if (b.val[q] == mx) h = true;
      }
    }
    if (a) any++;
    if (h) heads++;
  }
}

inline int evaluate(const Bd &b, int w) {
  int o = 3 - w, mw = maxOf(b, w), mo = maxOf(b, o);
  int hw, aw, ho, ao;
  freedom(b, w, mw, hw, aw);
  freedom(b, o, mo, ho, ao);
  return 100 * (mw - mo) + 4 * (hw - ho) + (aw - ao);
}

inline int terminal(const Bd &b, int w) {
  int mw = maxOf(b, w), mo = maxOf(b, 3 - w);
  if (mw > mo) return 10000 + (mw - mo);
  if (mw < mo) return -10000 - (mo - mw);
  return 0;
}

inline uint32_t rngState = 521288629u;
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

constexpr int BEAM = 8;

// Negamax with alpha-beta. Value from the point of view of w (to move).
inline int negamax(Bd &b, int w, int depth, int alpha, int beta,
                   bool prevPassed, Search &S) {
  if ((++S.nodes & 63) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  Move mv[BEAM];
  int n = genMoves(b, w, mv, BEAM);
  if (n == 0) {
    if (prevPassed) return terminal(b, w);
    return -negamax(b, 3 - w, depth, -beta, -alpha, true, S);
  }
  if (depth == 0) return evaluate(b, w);
  int best = -1000000;
  for (int i = 0; i < n; i++) {
    b.val[mv[i].cell] = mv[i].newVal;
    b.own[mv[i].cell] = (uint8_t)w;
    int v = -negamax(b, 3 - w, depth - 1, -beta, -alpha, false, S);
    b.val[mv[i].cell] = 0;
    b.own[mv[i].cell] = 0;
    if (S.timeUp) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

// Returns cell | (newVal << 8), or -1 when w cannot move.
inline int chooseHard(Bd &b, int w, unsigned long limitMs) {
  Move root[14];
  int n = genMoves(b, w, root, 14);
  if (n == 0) return -1;
  int best = root[0].cell | (root[0].newVal << 8);
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  int empties = 0;
  for (int c = 0; c < b.n * b.n; c++) if (!b.val[c]) empties++;
  for (int depth = 1; depth <= empties; depth++) {
    int bestV = -1000000, cnt = 0;
    int list[14];
    for (int i = 0; i < n; i++) {
      b.val[root[i].cell] = root[i].newVal;
      b.own[root[i].cell] = (uint8_t)w;
      int v = -negamax(b, 3 - w, depth - 1, -1000000, 1000000, false, S);
      b.val[root[i].cell] = 0;
      b.own[root[i].cell] = 0;
      if (S.timeUp) break;
      if (v > bestV) { bestV = v; cnt = 0; }
      if (v == bestV) list[cnt++] = i;
    }
    if (S.timeUp) break;
    int pick = list[rnd() % cnt];
    best = root[pick].cell | (root[pick].newVal << 8);
    if (bestV >= 10000 || bestV <= -10000) break;  // game decided
  }
  return best;
}

inline int chooseEasy(Bd &b, int w) {
  Move mv[3];
  int n = genMoves(b, w, mv, 3);
  if (n == 0) return -1;
  Move m = mv[rnd() % n];
  return m.cell | (m.newVal << 8);
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t {
  PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_CELL, PH_SRC, PH_OVER
};

RTC_DATA_ATTR Bd bd;
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanFirst = 1;
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t turnP = 1;       // player to move (1 or 2)
RTC_DATA_ATTR uint8_t cursor = 0;
RTC_DATA_ATTR uint8_t srcSel = 0;      // index in the list of numbers to extend
RTC_DATA_ATTR uint8_t passNote = 0;    // player who just had to pass
RTC_DATA_ATTR uint8_t result = 0;      // 1 first wins, 2 second wins, 3 draw
RTC_DATA_ATTR uint8_t lastMove = 255;
RTC_DATA_ATTR uint8_t max1 = 0, max2 = 0;

inline bool robotTurn() {
  return vsRobot && ((turnP == 1) != (humanFirst != 0));
}

template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

inline void placeMove(int cell, int newVal) {
  bd.val[cell] = (uint8_t)newVal;
  bd.own[cell] = turnP;
  lastMove = (uint8_t)cell;
  turnP = (uint8_t)(3 - turnP);
}

inline void robotPlay() {
  rngState ^= (uint32_t)millis() * 2654435761u;
  int m = hard ? chooseHard(bd, turnP, hardLimitMs) : chooseEasy(bd, turnP);
  if (m >= 0) placeMove(m & 255, m >> 8);
}

inline void keepCursorLegal() {
  int total = bd.n * bd.n;
  if (legalTarget(bd, cursor, turnP)) return;
  for (int c = 0; c < total; c++)
    if (legalTarget(bd, c, turnP)) { cursor = (uint8_t)c; return; }
}

// After every move: skip passes, let the robot play, detect the end.
inline void advance() {
  for (int guard = 0; guard < 200; guard++) {
    if (!hasMove(bd, 1) && !hasMove(bd, 2)) {
      max1 = (uint8_t)maxOf(bd, 1);
      max2 = (uint8_t)maxOf(bd, 2);
      result = (max1 > max2) ? 1 : (max2 > max1 ? 2 : 3);
      phase = PH_OVER;
      return;
    }
    if (!hasMove(bd, turnP)) {
      passNote = turnP;
      turnP = (uint8_t)(3 - turnP);
      continue;
    }
    if (robotTurn()) { robotPlay(); continue; }
    break;
  }
  phase = PH_CELL;
  keepCursorLegal();
}

inline void newGame() {
  clearBoard(bd, 6);
  turnP = 1;
  result = 0;
  lastMove = 255;
  passNote = 0;
  cursor = 0;
  advance();
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

// ---------------- buttons ----------------
inline void move(int dir) {
  if (phase == PH_MODE || phase == PH_ROLE || phase == PH_LEVEL) {
    menuSel = (uint8_t)(1 - menuSel);
  } else if (phase == PH_CELL) {
    int total = bd.n * bd.n;
    for (int s = qwGameSteps(); s > 0; s--)
      cursor = (uint8_t)cycle(cursor, dir, total, [](int i) {
        return legalTarget(bd, i, turnP);
      });
  } else if (phase == PH_SRC) {
    uint8_t vals[8];
    int cnt = sourceValues(bd, cursor, turnP, vals);
    srcSel = (uint8_t)cycle(srcSel, dir, cnt + 1, [](int) { return true; });
  }
}

inline void up() { move(-1); }
inline void down() { move(+1); }

inline void select() {
  switch (phase) {
    case PH_MODE:
      vsRobot = (menuSel == 0);
      menuSel = 0;
      if (vsRobot) phase = PH_ROLE;
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
    case PH_CELL: {
      uint8_t vals[8];
      int cnt = sourceValues(bd, cursor, turnP, vals);
      if (cnt == 0) break;
      passNote = 0;
      if (cnt == 1) {
        placeMove(cursor, vals[0] + 1);
        advance();
      } else {
        srcSel = (uint8_t)(cnt - 1);  // propose the highest number
        phase = PH_SRC;
      }
      break;
    }
    case PH_SRC: {
      uint8_t vals[8];
      int cnt = sourceValues(bd, cursor, turnP, vals);
      if (srcSel >= cnt) {
        phase = PH_CELL;  // "back"
      } else {
        placeMove(cursor, vals[srcSel] + 1);
        advance();
      }
      break;
    }
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
  const uint8_t textSize = 2;

  const char *n1 = vsRobot ? (humanFirst ? "YOU" : "ME") : "P1";
  const char *n2 = vsRobot ? (humanFirst ? "ME" : "YOU") : "P2";
  const char *who = (turnP == 1) ? n1 : n2;

  char l1[40], l2[40];
  if (phase == PH_OVER) {
    const char *r = "DRAW";
    if (result == 1) r = vsRobot ? (humanFirst ? "YOU WIN!" : "I WIN") : "P1 WINS!";
    else if (result == 2) r = vsRobot ? (humanFirst ? "I WIN" : "YOU WIN!") : "P2 WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    snprintf(l1, sizeof(l1), "HIGHEST: %s %d  %s %d", n1, max1, n2, max2);
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 19);
    d.print(l1);
  } else {
    char tmp[16];
    if (phase == PH_SRC) {
      uint8_t vals[8];
      int cnt = sourceValues(bd, cursor, turnP, vals);
      if (srcSel >= cnt) {
        snprintf(l1, sizeof(l1), "<< BACK TO SQUARES");
        snprintf(l2, sizeof(l2), "MENU=choose another square");
      } else {
        snprintf(l1, sizeof(l1), "%s: EXTEND %d -> %d", who, vals[srcSel],
                 vals[srcSel] + 1);
        snprintf(l2, sizeof(l2), "UP/DOWN=choose  MENU=ok");
      }
    } else {
      snprintf(l1, sizeof(l1), "%s (%s): CHOOSE A SQUARE", who,
               turnP == 1 ? "WHITE" : "BLACK");
      if (passNote) {
        snprintf(tmp, sizeof(tmp), "%s", passNote == 1 ? n1 : n2);
        snprintf(l2, sizeof(l2), "%s HAD NO MOVE: PASSED", tmp);
      } else {
        snprintf(l2, sizeof(l2), "UP/DOWN=pick (hold=fast)");
      }
    }
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 3);
    d.print(l1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l2), 1) / 2, 14);
    d.print(l2);
  }

  // grid
  d.drawRect(X0 - 2, Y0 - 2, n * cs + 4, n * cs + 4, qwInk());
  for (int i = 1; i < n; i++) {
    d.drawFastVLine(X0 + i * cs, Y0, n * cs, qwInk());
    d.drawFastHLine(X0, Y0 + i * cs, n * cs, qwInk());
  }
  for (int c = 0; c < n * n; c++) {
    int16_t bx = X0 + (c % n) * cs, by = Y0 + (c / n) * cs;
    bool legal = (phase == PH_CELL || phase == PH_SRC) &&
                 legalTarget(bd, c, turnP);
    if (bd.val[c]) {
      char t[4];
      snprintf(t, sizeof(t), "%d", bd.val[c]);
      qwSetText(d, textSize);
      int16_t w = qwTextInkWidth((uint8_t)strlen(t), textSize);
      int16_t tx = bx + cs / 2 - w / 2, ty = by + cs / 2 - (textSize == 2 ? 7 : 3);
      if (bd.own[c] == 2) {  // second player: black tile, white number
        d.fillRect(bx + 2, by + 2, cs - 3, cs - 3, qwInk());
        d.setTextColor(qwPaper());
      } else {               // first player: white tile with a frame
        d.drawRect(bx + 2, by + 2, cs - 3, cs - 3, qwInk());
        d.setTextColor(qwInk());
      }
      d.setCursor(tx, ty);
      d.print(t);
      d.setTextColor(qwInk());
      if (c == lastMove && phase != PH_OVER)
        d.fillRect(bx + cs - 6, by + 3, 3, 3,
                   bd.own[c] == 2 ? qwPaper() : qwInk());
    } else if (legal) {
      d.fillRect(bx + cs / 2 - 1, by + cs / 2 - 1, 3, 3, qwInk());
    }
  }
  // end of game: frame the highest numbers
  if (phase == PH_OVER) {
    for (int c = 0; c < n * n; c++) {
      if (!bd.val[c]) continue;
      int mx = (bd.own[c] == 1) ? max1 : max2;
      if (bd.val[c] != mx) continue;
      int16_t bx = X0 + (c % n) * cs, by = Y0 + (c / n) * cs;
      d.drawRect(bx, by, cs + 1, cs + 1, qwInk());
      if (bd.own[c] == 2) d.drawRect(bx + 4, by + 4, cs - 7, cs - 7, qwPaper());
    }
  }
  // cursor, and in step 2 the squares being extended + the new number
  if (phase == PH_CELL || phase == PH_SRC) {
    int16_t bx = X0 + (cursor % n) * cs, by = Y0 + (cursor / n) * cs;
    if (phase == PH_SRC) {
      uint8_t vals[8];
      int cnt = sourceValues(bd, cursor, turnP, vals);
      if (srcSel < cnt) {
        uint8_t nb[8];
        int k = neighbours(n, cursor, nb);
        for (int i = 0; i < k; i++) {
          int q = nb[i];
          if (bd.val[q] && bd.own[q] == turnP && bd.val[q] == vals[srcSel]) {
            int16_t qx = X0 + (q % n) * cs, qy = Y0 + (q / n) * cs;
            d.drawRect(qx, qy, cs + 1, cs + 1, turnP == 2 ? qwPaper() : qwInk());
            d.drawRect(qx + 1, qy + 1, cs - 1, cs - 1, turnP == 2 ? qwPaper() : qwInk());
          }
        }
        char t[4];
        snprintf(t, sizeof(t), "%d", vals[srcSel] + 1);
        qwSetText(d, textSize);
        int16_t w = qwTextInkWidth((uint8_t)strlen(t), textSize);
        d.setCursor(bx + cs / 2 - w / 2, by + cs / 2 - (textSize == 2 ? 7 : 3));
        d.print(t);
      }
    }
    d.drawRect(bx, by, cs + 1, cs + 1, qwInk());
    d.drawRect(bx + 1, by + 1, cs - 1, cs - 1, qwInk());
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "SEQUENCIUM", "1 PLAYER", "2 PLAYERS",
                     "Highest number wins");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOU PLAY", "FIRST (WHITE)", "SECOND (BLACK)",
                     "Start: chains 1-2-3 on the diagonal");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                     "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Sq
