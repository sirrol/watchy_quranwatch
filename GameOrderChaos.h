#pragma once
/*
  GameOrderChaos.h
  ------------------------------------------------------------
  ORDER AND CHAOS for QuranWatch (included by Games.h).

  A 6x6 board. Both players place an X OR an O (their choice, every
  turn) on any empty cell. ORDER moves first and wins by making five
  (or more) identical symbols in a row - horizontally, vertically or
  diagonally. CHAOS wins if that becomes impossible (for instance when
  the board is full without such a line).

  1 player: you pick your side, the watch plays the other one.
  Robot levels: EASY (wins / blocks when it sees it, otherwise mostly
  random) and HARD (looks several moves ahead, at most ~2.5 seconds).

  Fast moving: a move is made in two short steps. Step 1: UP/DOWN walk
  through the FREE cells only (hold the button to jump 3 or 6 cells),
  MENU confirms. Step 2: UP/DOWN choose X, O or "back to cells", MENU
  places the symbol.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Oc {

constexpr int N = 6, NC = 36, NW = 32;

// ---------------- the 32 possible lines of five ----------------
inline uint8_t WIN_CELLS[NW][5];
inline uint8_t CELL_WINS[NC][8];
inline uint8_t CELL_NW[NC];
inline bool tablesReady = false;

inline void initTables() {
  if (tablesReady) return;
  for (int c = 0; c < NC; c++) CELL_NW[c] = 0;
  int w = 0;
  static const int8_t D[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      for (const auto &d : D) {
        int er = r + 4 * d[0], ec = c + 4 * d[1];
        if (er < 0 || er >= N || ec < 0 || ec >= N) continue;
        for (int k = 0; k < 5; k++) {
          int cell = (r + k * d[0]) * N + (c + k * d[1]);
          WIN_CELLS[w][k] = (uint8_t)cell;
          CELL_WINS[cell][CELL_NW[cell]++] = (uint8_t)w;
        }
        w++;
      }
  tablesReady = true;
}

// ---------------- position for the robot ----------------
constexpr int WIN_V = 100000;
static const int VT[6] = {0, 1, 6, 30, 150, WIN_V};

inline int wval(int nx, int no) {
  if (nx && no) return 0;  // dead line: both symbols in it
  return VT[nx + no];
}

struct Pos {
  uint8_t cells[NC];  // 0 empty, 1 X, 2 O
  uint8_t wx[NW], wo[NW];
  int16_t alive;      // lines of five that still can be completed
  int32_t score;      // sum of line values, good for ORDER
  uint8_t filled;
};

inline void buildPos(Pos &p, const uint8_t *cells) {
  initTables();
  for (int i = 0; i < NC; i++) p.cells[i] = cells[i];
  p.filled = 0;
  for (int i = 0; i < NC; i++) if (cells[i]) p.filled++;
  p.alive = 0;
  p.score = 0;
  for (int w = 0; w < NW; w++) {
    int nx = 0, no = 0;
    for (int k = 0; k < 5; k++) {
      uint8_t v = cells[WIN_CELLS[w][k]];
      if (v == 1) nx++;
      else if (v == 2) no++;
    }
    p.wx[w] = (uint8_t)nx;
    p.wo[w] = (uint8_t)no;
    if (!(nx && no)) p.alive++;
    p.score += wval(nx, no);
  }
}

struct Saved { int32_t score; int16_t alive; };

// Places symbol s on cell c. Returns true if it makes five in a row.
inline bool place(Pos &p, int c, uint8_t s, Saved &sv) {
  sv.score = p.score;
  sv.alive = p.alive;
  bool win = false;
  for (int i = 0; i < CELL_NW[c]; i++) {
    int w = CELL_WINS[c][i];
    int oldV = wval(p.wx[w], p.wo[w]);
    bool wasAlive = !(p.wx[w] && p.wo[w]);
    if (s == 1) p.wx[w]++; else p.wo[w]++;
    int nx = p.wx[w], no = p.wo[w];
    p.score += wval(nx, no) - oldV;
    if (wasAlive && nx && no) p.alive--;
    if ((s == 1 && nx == 5 && !no) || (s == 2 && no == 5 && !nx)) win = true;
  }
  p.cells[c] = s;
  p.filled++;
  return win;
}

inline void unplace(Pos &p, int c, uint8_t s, const Saved &sv) {
  for (int i = 0; i < CELL_NW[c]; i++) {
    int w = CELL_WINS[c][i];
    if (s == 1) p.wx[w]--; else p.wo[w]--;
  }
  p.cells[c] = 0;
  p.filled--;
  p.score = sv.score;
  p.alive = sv.alive;
}

// Change of p.score if s were placed on c (no modification).
inline int deltaOf(const Pos &p, int c, uint8_t s) {
  int d = 0;
  for (int i = 0; i < CELL_NW[c]; i++) {
    int w = CELL_WINS[c][i];
    int nx = p.wx[w] + (s == 1), no = p.wo[w] + (s == 2);
    d += wval(nx, no) - wval(p.wx[w], p.wo[w]);
  }
  return d;
}

// ---------------- robot ----------------
inline uint32_t rngState = 88172645u;
inline uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

inline unsigned long hardLimitMs = 2500;  // time the HARD robot may think

struct Cand { uint8_t move; int key; };  // move = cell * 2 + (symbol - 1)

// The `maxN` most promising moves for the side to move, best first.
// key = what the mover wants to be large (ORDER: score up, CHAOS: down).
inline int genCands(const Pos &p, bool orderToMove, Cand *out, int maxN) {
  int n = 0;
  for (int c = 0; c < NC; c++) {
    if (p.cells[c]) continue;
    for (uint8_t s = 1; s <= 2; s++) {
      int d = deltaOf(p, c, s);
      Cand cd = {(uint8_t)(c * 2 + s - 1), orderToMove ? d : -d};
      // insert into the sorted top-maxN list
      if (n < maxN) {
        int i = n++;
        while (i > 0 && out[i - 1].key < cd.key) { out[i] = out[i - 1]; i--; }
        out[i] = cd;
      } else if (cd.key > out[n - 1].key) {
        int i = n - 1;
        while (i > 0 && out[i - 1].key < cd.key) { out[i] = out[i - 1]; i--; }
        out[i] = cd;
      }
    }
  }
  return n;
}

struct Search {
  unsigned long t0 = 0, limitMs = 2500;
  uint32_t nodes = 0;
  bool timeUp = false;
};

constexpr int BEAM = 8;

// Value from ORDER's point of view (minimax with alpha-beta).
inline int search(Pos &p, bool orderToMove, int depth, int alpha, int beta,
                  Search &S) {
  if ((++S.nodes & 63) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  Cand cand[BEAM];
  int n = genCands(p, orderToMove, cand, BEAM);
  int best = orderToMove ? -2000000 : 2000000;
  for (int i = 0; i < n; i++) {
    int c = cand[i].move / 2;
    uint8_t s = (uint8_t)(cand[i].move % 2 + 1);
    Saved sv;
    bool win = place(p, c, s, sv);
    int v;
    if (win) v = WIN_V + depth;
    else if (p.alive == 0 || p.filled == NC) v = -WIN_V - depth;
    else if (depth <= 1) v = p.score;
    else v = search(p, !orderToMove, depth - 1, alpha, beta, S);
    unplace(p, c, s, sv);
    if (S.timeUp) return 0;
    if (orderToMove) {
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

// Iterative deepening; the last COMPLETE depth decides.
inline int chooseHard(const uint8_t *cells, bool orderToMove,
                      unsigned long limitMs, int maxDepth = 9) {
  Pos p;
  buildPos(p, cells);
  Cand root[16];
  int n = genCands(p, orderToMove, root, 14);
  if (n == 0) return -1;
  int bestMove = root[0].move;
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  for (int depth = 1; depth <= maxDepth; depth++) {
    int bestV = orderToMove ? -2000000 : 2000000;
    int cnt = 0;
    uint8_t list[16];
    for (int i = 0; i < n; i++) {
      int c = root[i].move / 2;
      uint8_t s = (uint8_t)(root[i].move % 2 + 1);
      Saved sv;
      bool win = place(p, c, s, sv);
      int v;
      if (win) v = WIN_V + depth;
      else if (p.alive == 0 || p.filled == NC) v = -WIN_V - depth;
      else if (depth <= 1) v = p.score;
      else v = search(p, !orderToMove, depth - 1, -2000000, 2000000, S);
      unplace(p, c, s, sv);
      if (S.timeUp) break;
      bool better = orderToMove ? (v > bestV) : (v < bestV);
      if (better) { bestV = v; cnt = 0; }
      if (v == bestV) list[cnt++] = root[i].move;
    }
    if (S.timeUp) break;
    bestMove = list[rnd() % cnt];
    if (bestV >= WIN_V || bestV <= -WIN_V) break;  // game decided
  }
  return bestMove;
}

// Moves by which ORDER would win at once (any cell, any symbol).
inline int findWinningMove(const uint8_t *cells) {
  Pos p;
  buildPos(p, cells);
  for (int c = 0; c < NC; c++) {
    if (cells[c]) continue;
    for (uint8_t s = 1; s <= 2; s++)
      for (int i = 0; i < CELL_NW[c]; i++) {
        int w = CELL_WINS[c][i];
        if (s == 1 && p.wx[w] == 4 && p.wo[w] == 0) return c * 2;
        if (s == 2 && p.wo[w] == 4 && p.wx[w] == 0) return c * 2 + 1;
      }
  }
  return -1;
}

inline int chooseEasy(const uint8_t *cells, bool orderToMove) {
  Pos p;
  buildPos(p, cells);
  int win = findWinningMove(cells);
  if (orderToMove) {
    if (win >= 0) return win;
    Cand cand[4];  // one of the 4 best by the simple line count
    int n = genCands(p, true, cand, 4);
    return n ? cand[rnd() % n].move : -1;
  }
  if (win >= 0) {  // block: the other symbol on the winning cell
    int c = win / 2;
    uint8_t other = (uint8_t)(win % 2 == 0 ? 2 : 1);
    return c * 2 + other - 1;
  }
  int freeCells[NC], nf = 0;
  for (int c = 0; c < NC; c++) if (!cells[c]) freeCells[nf++] = c;
  if (!nf) return -1;
  return freeCells[rnd() % nf] * 2 + (int)(rnd() % 2);
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_CELL, PH_SYM, PH_OVER };

RTC_DATA_ATTR uint8_t cells[NC];
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanOrder = 1;  // 1P: you play ORDER (else CHAOS)
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t moves = 0;       // moves played; even = ORDER to move
RTC_DATA_ATTR uint8_t cellSel = 0;
RTC_DATA_ATTR uint8_t symSel = 0;      // 0 = X, 1 = O, 2 = back
RTC_DATA_ATTR uint8_t result = 0;      // 0 going, 1 ORDER wins, 2 CHAOS wins
RTC_DATA_ATTR uint8_t lastMove = 255;
RTC_DATA_ATTR uint8_t winLine = 255;   // index of the winning line

inline bool orderToMove() { return (moves % 2) == 0; }
inline bool robotTurn() { return vsRobot && (orderToMove() != (humanOrder != 0)); }

template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

inline void firstFreeCell() {
  if (!cells[cellSel]) return;  // keep the cursor where it was
  for (int c = 0; c < NC; c++) if (!cells[c]) { cellSel = (uint8_t)c; return; }
}

inline void playMove(int move) {
  initTables();
  int c = move / 2;
  cells[c] = (uint8_t)(move % 2 + 1);
  moves++;
  lastMove = (uint8_t)c;
  int alive = 0;
  for (int w = 0; w < NW; w++) {
    int nx = 0, no = 0;
    for (int k = 0; k < 5; k++) {
      uint8_t v = cells[WIN_CELLS[w][k]];
      if (v == 1) nx++;
      else if (v == 2) no++;
    }
    if (nx == 5 || no == 5) { winLine = (uint8_t)w; result = 1; }
    if (!(nx && no)) alive++;
  }
  if (!result && (alive == 0 || moves >= NC)) result = 2;
  if (result) phase = PH_OVER;
}

inline void robotPlay() {
  rngState ^= (uint32_t)millis() * 2654435761u;
  bool o = orderToMove();
  int m = hard ? chooseHard(cells, o, hardLimitMs) : chooseEasy(cells, o);
  if (m >= 0) playMove(m);
}

inline void beginTurn() {
  if (phase == PH_OVER) return;
  firstFreeCell();
  phase = PH_CELL;
}

inline void newGame() {
  for (auto &c : cells) c = 0;
  moves = 0;
  result = 0;
  lastMove = 255;
  winLine = 255;
  cellSel = 14;
  phase = PH_CELL;
  if (robotTurn()) robotPlay();  // the watch is ORDER and starts
  beginTurn();
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
    for (int s = qwGameSteps(); s > 0; s--)
      cellSel = (uint8_t)cycle(cellSel, dir, NC,
                               [](int i) { return cells[i] == 0; });
  } else if (phase == PH_SYM) {
    symSel = (uint8_t)cycle(symSel, dir, 3, [](int) { return true; });
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
      humanOrder = (menuSel == 0);
      phase = PH_LEVEL;
      menuSel = 0;
      break;
    case PH_LEVEL:
      hard = menuSel;
      newGame();
      break;
    case PH_CELL:
      symSel = 0;
      phase = PH_SYM;
      break;
    case PH_SYM:
      if (symSel == 2) {
        phase = PH_CELL;
      } else {
        playMove(cellSel * 2 + symSel);
        if (phase != PH_OVER && robotTurn()) robotPlay();
        beginTurn();
      }
      break;
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t CELL = 28, X0 = 16, Y0 = 30;

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

inline void drawMark(Adafruit_GFX &d, int16_t cx, int16_t cy, uint8_t s) {
  if (s == 1) {
    for (int t = -1; t <= 1; t++) {
      d.drawLine(cx - 8 + t, cy - 8, cx + 8 + t, cy + 8, qwInk());
      d.drawLine(cx - 8 + t, cy + 8, cx + 8 + t, cy - 8, qwInk());
    }
  } else if (s == 2) {
    for (int r = 7; r <= 9; r++) d.drawCircle(cx, cy, r, qwInk());
  }
}

inline void drawBoardScreen(Adafruit_GFX &d) {
  initTables();
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());

  // who is to move, in words
  char who[16];
  bool o = orderToMove();
  if (vsRobot) snprintf(who, sizeof(who), "YOU (%s)", humanOrder ? "ORDER" : "CHAOS");
  else snprintf(who, sizeof(who), "%s", o ? "ORDER" : "CHAOS");

  char l1[40], l2[40];
  if (phase == PH_OVER) {
    const char *r;
    if (vsRobot) r = ((result == 1) == (humanOrder != 0)) ? "YOU WIN!" : "I WIN";
    else r = (result == 1) ? "ORDER WINS!" : "CHAOS WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    snprintf(l1, sizeof(l1), "MENU=new game  BACK=exit");
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 19);
    d.print(l1);
  } else {
    if (phase == PH_CELL) {
      snprintf(l1, sizeof(l1), "%s: CHOOSE A CELL", who);
      snprintf(l2, sizeof(l2), "UP/DOWN=pick (hold=fast)");
    } else if (symSel == 2) {
      snprintf(l1, sizeof(l1), "<< BACK TO CELLS");
      snprintf(l2, sizeof(l2), "MENU=choose another cell");
    } else {
      snprintf(l1, sizeof(l1), "%s: PLACE %c?", who, symSel == 0 ? 'X' : 'O');
      snprintf(l2, sizeof(l2), "UP/DOWN=X/O/back  MENU=ok");
    }
    qwSetText(d, 1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l1), 1) / 2, 3);
    d.print(l1);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(l2), 1) / 2, 14);
    d.print(l2);
  }

  // grid
  d.drawRect(X0 - 2, Y0 - 2, N * CELL + 4, N * CELL + 4, qwInk());
  d.drawRect(X0 - 1, Y0 - 1, N * CELL + 2, N * CELL + 2, qwInk());
  for (int i = 1; i < N; i++) {
    d.drawFastVLine(X0 + i * CELL, Y0, N * CELL, qwInk());
    d.drawFastHLine(X0, Y0 + i * CELL, N * CELL, qwInk());
  }
  for (int c = 0; c < NC; c++) {
    int16_t cx = X0 + (c % N) * CELL + CELL / 2;
    int16_t cy = Y0 + (c / N) * CELL + CELL / 2;
    if (cells[c]) drawMark(d, cx, cy, cells[c]);
  }
  // last move: a dot in the corner of its cell
  if (lastMove != 255 && cells[lastMove]) {
    d.fillRect(X0 + (lastMove % N) * CELL + 3, Y0 + (lastMove / N) * CELL + 3,
               3, 3, qwInk());
  }
  // winning line of five: frame its cells
  if (phase == PH_OVER && winLine != 255) {
    for (int k = 0; k < 5; k++) {
      int c = WIN_CELLS[winLine][k];
      d.drawRect(X0 + (c % N) * CELL + 2, Y0 + (c / N) * CELL + 2, CELL - 3,
                 CELL - 3, qwInk());
    }
  }
  // cursor
  if (phase == PH_CELL || phase == PH_SYM) {
    int16_t bx = X0 + (cellSel % N) * CELL, by = Y0 + (cellSel / N) * CELL;
    int16_t cx = bx + CELL / 2, cy = by + CELL / 2;
    if (phase == PH_CELL) {
      d.fillRect(cx - 3, cy - 3, 7, 7, qwInk());
    } else if (symSel < 2) {
      drawMark(d, cx, cy, (uint8_t)(symSel + 1));  // preview
    }
    d.drawRect(bx + 2, by + 2, CELL - 3, CELL - 3, qwInk());
    d.drawRect(bx + 3, by + 3, CELL - 5, CELL - 5, qwInk());
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "ORDER & CHAOS", "1 PLAYER", "2 PLAYERS",
                     "ORDER: 5 in a row, X or O");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOUR SIDE", "ORDER", "CHAOS",
                     "ORDER plays first");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                     "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Oc
