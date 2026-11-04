#pragma once
/*
  GameUttt.h
  ------------------------------------------------------------
  ULTIMATE TIC-TAC-TOE for QuranWatch (included by Games.h).

  Rules: 9 small boards (3x3) form one big 3x3 board. The cell you
  play inside a small board decides in which small board your
  opponent must play next. Three in a row wins a small board; three
  small boards in a row win the game. If you are sent to a board
  that is already won or full, you may play in any open board.

  1 player: you are X and play first, the watch (O) is the robot.
  2 players: pass the watch.
  Robot levels: EASY (wins or blocks a board, otherwise random) and
  HARD (looks several moves ahead for at most ~2.5 seconds).

  Fast moving (the e-ink screen is slow): a move is made in at most
  two short steps. Step 1 (only when you may play anywhere): UP/DOWN
  pick a board, MENU confirms. Step 2: UP/DOWN pick a cell - only the
  free cells are visited - and MENU plays it. In step 2 the last
  entry of the loop is "back to boards".
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Uttt {

// ---------------- pure game logic (no screen) ----------------
struct Pos {
  uint8_t cells[81];  // board b, cell c -> cells[b * 9 + c]; 0 empty, 1 X, 2 O
  uint8_t sb[9];      // small board state: 0 open, 1 X won, 2 O won, 3 full
  int8_t forced;      // board the next player must play in, -1 = anywhere
};

static const uint8_t L3[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                 {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};

inline uint8_t subState(const uint8_t *c) {
  for (const auto &l : L3) {
    uint8_t a = c[l[0]];
    if (a && a == c[l[1]] && a == c[l[2]]) return a;
  }
  for (int i = 0; i < 9; i++) if (!c[i]) return 0;
  return 3;
}

// 0 = game goes on, 1/2 = winner, 3 = draw.
inline uint8_t bigState(const uint8_t *sb) {
  for (const auto &l : L3) {
    uint8_t a = sb[l[0]];
    if ((a == 1 || a == 2) && a == sb[l[1]] && a == sb[l[2]]) return a;
  }
  for (int i = 0; i < 9; i++) if (sb[i] == 0) return 0;
  return 3;
}

inline void clearPos(Pos &p) {
  for (auto &c : p.cells) c = 0;
  for (auto &s : p.sb) s = 0;
  p.forced = -1;
}

// Legal moves (board * 9 + cell). Returns how many.
inline int genMoves(const Pos &p, uint8_t *mv) {
  int n = 0;
  if (p.forced >= 0 && p.sb[p.forced] == 0) {
    for (int c = 0; c < 9; c++)
      if (!p.cells[p.forced * 9 + c]) mv[n++] = (uint8_t)(p.forced * 9 + c);
  } else {
    for (int b = 0; b < 9; b++) {
      if (p.sb[b] != 0) continue;
      for (int c = 0; c < 9; c++)
        if (!p.cells[b * 9 + c]) mv[n++] = (uint8_t)(b * 9 + c);
    }
  }
  return n;
}

struct Undo { int8_t forced; uint8_t sbOld; };

inline Undo applyMove(Pos &p, int m, uint8_t who) {
  int b = m / 9, c = m % 9;
  Undo u = {p.forced, p.sb[b]};
  p.cells[m] = who;
  p.sb[b] = subState(p.cells + b * 9);
  p.forced = (p.sb[c] == 0) ? (int8_t)c : (int8_t)-1;
  return u;
}

inline void undoMove(Pos &p, int m, const Undo &u) {
  p.cells[m] = 0;
  p.sb[m / 9] = u.sbOld;
  p.forced = u.forced;
}

// Would `who` win the small board by playing m?
inline bool winsSmallBoard(Pos &p, int m, uint8_t who) {
  int b = m / 9;
  p.cells[m] = who;
  bool w = (subState(p.cells + b * 9) == who);
  p.cells[m] = 0;
  return w;
}

// ---------------- robot ----------------
inline uint32_t rngState = 2463534242u;
inline uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

// Static evaluation, positive = good for `me`.
inline int evaluate(const Pos &p, uint8_t me) {
  static const int8_t W[9] = {3, 2, 3, 2, 4, 2, 3, 2, 3};
  int score = 0;
  for (int b = 0; b < 9; b++) {
    uint8_t s = p.sb[b];
    if (s == 1 || s == 2) {
      score += (s == me ? 1 : -1) * 25 * W[b];
    } else if (s == 0) {
      const uint8_t *c = p.cells + b * 9;
      for (const auto &l : L3) {
        int mine = 0, theirs = 0;
        for (int k = 0; k < 3; k++) {
          if (c[l[k]] == me) mine++;
          else if (c[l[k]]) theirs++;
        }
        if (mine && !theirs) score += (mine == 1) ? 3 : 12;
        else if (theirs && !mine) score -= (theirs == 1) ? 3 : 12;
      }
      if (c[4] == me) score += 2;
      else if (c[4]) score -= 2;
    }
  }
  for (const auto &l : L3) {
    int mine = 0, theirs = 0;
    for (int k = 0; k < 3; k++) {
      uint8_t s = p.sb[l[k]];
      if (s == me) mine++;
      else if (s == 1 || s == 2) theirs++;
    }
    if (mine && !theirs) score += (mine == 1) ? 30 : 300;
    else if (theirs && !mine) score -= (theirs == 1) ? 30 : 300;
  }
  return score;
}

struct Search {
  unsigned long t0 = 0, limitMs = 2500;
  uint32_t nodes = 0;
  bool timeUp = false;
};

constexpr int WIN_SCORE = 100000;
inline unsigned long hardLimitMs = 2500;  // time the HARD robot may think

inline int negamax(Pos &p, uint8_t who, int depth, int alpha, int beta,
                   Search &S) {
  if ((++S.nodes & 255) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  uint8_t bs = bigState(p.sb);
  if (bs == 3) return 0;
  if (bs) return (bs == who) ? WIN_SCORE + depth : -(WIN_SCORE + depth);
  if (depth == 0) return evaluate(p, who);

  uint8_t mv[81];
  int n = genMoves(p, mv);
  // Moves that win a small board first (better alpha-beta cut-offs).
  int front = 0;
  for (int i = 0; i < n; i++) {
    if (winsSmallBoard(p, mv[i], who)) {
      uint8_t t = mv[front]; mv[front] = mv[i]; mv[i] = t;
      front++;
    }
  }
  int best = -1000000;
  for (int i = 0; i < n; i++) {
    Undo u = applyMove(p, mv[i], who);
    int v = -negamax(p, (uint8_t)(3 - who), depth - 1, -beta, -alpha, S);
    undoMove(p, mv[i], u);
    if (S.timeUp) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

// Iterative deepening: the result of the last COMPLETE depth is used.
inline int chooseHard(Pos &p, uint8_t who, unsigned long limitMs = 2500,
                      int maxDepth = 8) {
  uint8_t mv[81];
  int n = genMoves(p, mv);
  if (n == 0) return -1;
  if (n == 1) return mv[0];
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  int bestMove = mv[rnd() % n];
  for (int depth = 1; depth <= maxDepth; depth++) {
    int bestV = -1000000;
    int cnt = 0;
    int candList[81];
    for (int i = 0; i < n; i++) {
      Undo u = applyMove(p, mv[i], who);
      int v = -negamax(p, (uint8_t)(3 - who), depth - 1, -1000000, 1000000, S);
      undoMove(p, mv[i], u);
      if (S.timeUp) break;
      if (v > bestV) { bestV = v; cnt = 0; }
      if (v == bestV) candList[cnt++] = mv[i];
    }
    if (S.timeUp) break;  // incomplete depth: ignore it
    bestMove = candList[rnd() % cnt];
    if (bestV >= WIN_SCORE) break;  // forced win found
  }
  return bestMove;
}

inline int chooseEasy(Pos &p, uint8_t who) {
  uint8_t mv[81];
  int n = genMoves(p, mv);
  if (n == 0) return -1;
  for (int i = 0; i < n; i++)
    if (winsSmallBoard(p, mv[i], who)) return mv[i];
  for (int i = 0; i < n; i++)
    if (winsSmallBoard(p, mv[i], (uint8_t)(3 - who))) return mv[i];
  return mv[rnd() % n];
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_LEVEL, PH_BOARD, PH_CELL, PH_OVER };

RTC_DATA_ATTR Pos g;
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;     // PH_MODE / PH_LEVEL selection
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t turn = 1;        // 1 = X, 2 = O
RTC_DATA_ATTR uint8_t boardSel = 0;
RTC_DATA_ATTR uint8_t cellSel = 0;     // 0..8, 9 = "back to boards"
RTC_DATA_ATTR uint8_t result = 0;      // 0 going, 1 X wins, 2 O wins, 3 draw
RTC_DATA_ATTR uint8_t lastMove = 255;  // last move played (board * 9 + cell)

inline bool boardPlayable(int b) { return g.sb[b] == 0; }
inline bool forcedMode() { return g.forced >= 0 && g.sb[g.forced] == 0; }

// Next valid entry of the loop. valid(i) tells which entries exist.
template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

inline void firstFreeCell() {
  cellSel = 0;
  for (int c = 0; c < 9; c++)
    if (!g.cells[boardSel * 9 + c]) { cellSel = (uint8_t)c; return; }
}

// Start of a turn: go to the right selection step.
inline void beginTurn() {
  if (forcedMode()) {
    boardSel = (uint8_t)g.forced;
    firstFreeCell();
    phase = PH_CELL;
  } else {
    for (int b = 0; b < 9; b++)
      if (boardPlayable(b)) { boardSel = (uint8_t)b; break; }
    phase = PH_BOARD;
  }
}

inline void newGame() {
  clearPos(g);
  turn = 1;
  result = 0;
  lastMove = 255;
  beginTurn();
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

inline bool robotTurn() { return vsRobot && turn == 2; }

// Plays move m for the player to move; handles the end of the game.
inline void playMove(int m) {
  applyMove(g, m, turn);
  lastMove = (uint8_t)m;
  uint8_t bs = bigState(g.sb);
  if (bs) {
    result = bs;
    phase = PH_OVER;
    return;
  }
  turn = (uint8_t)(3 - turn);
}

inline void afterHumanMove() {
  if (phase == PH_OVER) return;
  if (robotTurn()) {
    rngState ^= (uint32_t)millis() * 2654435761u;
    int m = hard ? chooseHard(g, 2, hardLimitMs) : chooseEasy(g, 2);
    if (m >= 0) playMove(m);
  }
  if (phase != PH_OVER) beginTurn();
}

// ---------------- buttons ----------------
inline void move(int dir) {
  if (phase == PH_MODE || phase == PH_LEVEL) {
    menuSel = (uint8_t)(1 - menuSel);
  } else if (phase == PH_BOARD) {
    // Holding the button moves several entries at once.
    for (int s = qwGameSteps(); s > 0; s--)
      boardSel = (uint8_t)cycle(boardSel, dir, 9,
                                [](int i) { return boardPlayable(i); });
  } else if (phase == PH_CELL) {
    bool back = !forcedMode();
    for (int s = qwGameSteps(); s > 0; s--)
      cellSel = (uint8_t)cycle(cellSel, dir, 10, [&](int i) {
        return (i < 9) ? (g.cells[boardSel * 9 + i] == 0) : back;
      });
  }
}

inline void up() { move(-1); }
inline void down() { move(+1); }

inline void select() {
  switch (phase) {
    case PH_MODE:
      vsRobot = (menuSel == 0);
      if (vsRobot) { phase = PH_LEVEL; menuSel = 0; }
      else newGame();
      break;
    case PH_LEVEL:
      hard = menuSel;
      newGame();
      break;
    case PH_BOARD:
      firstFreeCell();
      phase = PH_CELL;
      break;
    case PH_CELL:
      if (cellSel == 9) {
        phase = PH_BOARD;
      } else {
        playMove(boardSel * 9 + cellSel);
        afterHumanMove();
      }
      break;
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t CELL = 19, SUB = 57, X0 = 14, Y0 = 28;

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

inline void drawBigMark(Adafruit_GFX &d, int16_t bx, int16_t by, uint8_t s) {
  d.fillRect(bx + 1, by + 1, SUB - 2, SUB - 2, qwPaper());
  int16_t cx = bx + SUB / 2, cy = by + SUB / 2;
  if (s == 1) {
    for (int t = -2; t <= 2; t++) {
      d.drawLine(bx + 8 + t, by + 8, bx + SUB - 9 + t, by + SUB - 9, qwInk());
      d.drawLine(bx + 8 + t, by + SUB - 9, bx + SUB - 9 + t, by + 8, qwInk());
    }
  } else if (s == 2) {
    for (int r = 19; r <= 23; r++) d.drawCircle(cx, cy, r, qwInk());
  } else {  // full board, nobody won: sparse dots
    for (int y = 4; y < SUB - 2; y += 4)
      for (int x = 4; x < SUB - 2; x += 4) d.drawPixel(bx + x, by + y, qwInk());
  }
}

inline void drawBoardScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());

  // --- two status lines on top ---
  char l1[40], l2[40];
  const char *who = "X";
  if (vsRobot) who = "YOU";
  else who = (turn == 1) ? "X" : "O";
  if (phase == PH_OVER) {
    const char *r = "DRAW";
    if (result == 1) r = vsRobot ? "YOU WIN!" : "X WINS!";
    else if (result == 2) r = vsRobot ? "I WIN" : "O WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    snprintf(l1, sizeof(l1), "MENU=new game  BACK=exit");
    l2[0] = 0;
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 19);
    d.print(l1);
  } else {
    if (phase == PH_BOARD) {
      snprintf(l1, sizeof(l1), "%s: CHOOSE A BOARD", who);
      snprintf(l2, sizeof(l2), "UP/DOWN=pick (hold=fast)");
    } else if (cellSel == 9) {
      snprintf(l1, sizeof(l1), "<< BACK TO BOARDS");
      snprintf(l2, sizeof(l2), "MENU=choose another board");
    } else {
      snprintf(l1, sizeof(l1), "%s: CHOOSE A CELL%s", who,
               forcedMode() ? " (FORCED)" : "");
      snprintf(l2, sizeof(l2), "UP/DOWN=pick (hold=fast)");
    }
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 3);
    d.print(l1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l2), 1) / 2, 14);
    d.print(l2);
  }

  // --- grids ---
  d.drawRect(X0 - 1, Y0 - 1, 3 * SUB + 2, 3 * SUB + 2, qwInk());
  for (int b = 0; b < 9; b++) {
    int16_t bx = X0 + (b % 3) * SUB, by = Y0 + (b / 3) * SUB;
    for (int i = 1; i < 3; i++) {  // thin lines inside a small board
      d.drawFastVLine(bx + i * CELL, by + 3, SUB - 6, qwInk());
      d.drawFastHLine(bx + 3, by + i * CELL, SUB - 6, qwInk());
    }
    for (int c = 0; c < 9; c++) {
      uint8_t v = g.cells[b * 9 + c];
      if (!v) continue;
      int16_t cx = bx + (c % 3) * CELL + CELL / 2;
      int16_t cy = by + (c / 3) * CELL + CELL / 2;
      if (v == 1) {
        for (int t = 0; t <= 1; t++) {
          d.drawLine(cx - 5 + t, cy - 5, cx + 5 + t, cy + 5, qwInk());
          d.drawLine(cx - 5 + t, cy + 5, cx + 5 + t, cy - 5, qwInk());
        }
      } else {
        d.drawCircle(cx, cy, 6, qwInk());
        d.drawCircle(cx, cy, 5, qwInk());
      }
    }
    if (g.sb[b] != 0) drawBigMark(d, bx, by, g.sb[b]);
  }
  // thick lines between the small boards
  for (int i = 1; i < 3; i++) {
    d.fillRect(X0 + i * SUB - 1, Y0, 3, 3 * SUB, qwInk());
    d.fillRect(X0, Y0 + i * SUB - 1, 3 * SUB, 3, qwInk());
  }

  // last move: small frame around the cell
  if (lastMove != 255 && g.sb[lastMove / 9] == 0) {
    int b = lastMove / 9, c = lastMove % 9;
    d.drawRect(X0 + (b % 3) * SUB + (c % 3) * CELL + 2,
               Y0 + (b / 3) * SUB + (c / 3) * CELL + 2, CELL - 3, CELL - 3,
               qwInk());
  }

  // active board frame + cell cursor
  if (phase == PH_BOARD || phase == PH_CELL) {
    int b = boardSel;
    int16_t bx = X0 + (b % 3) * SUB, by = Y0 + (b / 3) * SUB;
    d.drawRect(bx + 3, by + 3, SUB - 6, SUB - 6, qwInk());
    d.drawRect(bx + 4, by + 4, SUB - 8, SUB - 8, qwInk());
    if (phase == PH_CELL && cellSel < 9) {
      int16_t cx = bx + (cellSel % 3) * CELL + CELL / 2;
      int16_t cy = by + (cellSel / 3) * CELL + CELL / 2;
      d.fillRect(cx - 3, cy - 3, 7, 7, qwInk());
    }
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "ULTIMATE TTT", "1 PLAYER", "2 PLAYERS",
                     "3 small boards in a row win");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                     "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Uttt