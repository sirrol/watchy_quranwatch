#pragma once
/*
  GameTeeko.h
  ------------------------------------------------------------
  TEEKO for QuranWatch (included by Games.h).

  Teeko is a two-player game on a 5 x 5 board, invented by John Scarne.
  Each player has FOUR pieces (BLACK plays first).

    1. DROP PHASE : players take turns placing one piece on any empty
       cell, until all 8 pieces are on the board.
    2. MOVE PHASE : players take turns moving ONE of their pieces to an
       empty cell right next to it (sideways, up/down or diagonally).

  You win as soon as your four pieces form
       - a straight line of four (row, column or diagonal), or
       - a 2 x 2 square.
  This can already happen during the drop phase.

  Two small additions for the watch:
    - a player who has no legal move passes;
    - after 100 moves in the move phase the game is a DRAW.

  1 player: you pick BLACK (first) or WHITE, the watch plays the other
  colour. Robot levels: EASY (wins when it can, otherwise random) and
  HARD (looks several moves ahead for at most ~2.5 seconds).

  Moving: UP/DOWN walk through the cells you may choose (hold the button
  to jump 3 or 6 places), MENU confirms.
    - drop phase : pick an empty cell;
    - move phase : first pick the piece to move, then its destination
      (picking the same piece again cancels).
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Tk {

constexpr int N = 5, CELLS = 25, PIECES = 4;
constexpr int NWIN = 44;    // 28 lines of four + 16 squares
constexpr int NLINES = 28;  // winning masks below this index are lines
constexpr int MOVE_CAP = 100;
constexpr uint32_t FULL = (1u << CELLS) - 1;

// ---------------- tables (rebuilt after every wake-up) ----------------
inline uint32_t WIN[NWIN];  // the cells of every winning shape
inline uint32_t NB[CELLS];  // the 8 neighbours of every cell
inline bool tablesReady = false;

inline void initTables() {
  if (tablesReady) return;
  static const int8_t D[4][2] = {{0, 1}, {1, 0}, {1, 1}, {1, -1}};
  int n = 0;
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++)
      for (int d = 0; d < 4; d++) {
        int er = r + 3 * D[d][0], ec = c + 3 * D[d][1];
        if (er >= N || ec < 0 || ec >= N) continue;
        uint32_t m = 0;
        for (int k = 0; k < 4; k++) m |= 1u << ((r + k * D[d][0]) * N + c + k * D[d][1]);
        WIN[n++] = m;
      }
  for (int r = 0; r < N - 1; r++)
    for (int c = 0; c < N - 1; c++)
      WIN[n++] = (1u << (r * N + c)) | (1u << (r * N + c + 1)) |
                 (1u << ((r + 1) * N + c)) | (1u << ((r + 1) * N + c + 1));
  for (int r = 0; r < N; r++)
    for (int c = 0; c < N; c++) {
      uint32_t m = 0;
      for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
          if (!dr && !dc) continue;
          int rr = r + dr, cc = c + dc;
          if (rr >= 0 && rr < N && cc >= 0 && cc < N) m |= 1u << (rr * N + cc);
        }
      NB[r * N + c] = m;
    }
  tablesReady = true;
}

// ---------------- position and rules ----------------
struct Pos {
  uint32_t b[3];  // b[1] = black pieces, b[2] = white pieces (bit = cell)
};

struct Move {
  uint8_t from;  // 255 = a piece dropped on the board
  uint8_t to;
};

inline int pc(uint32_t x) { return __builtin_popcount(x); }
inline uint32_t occupied(const Pos &p) { return p.b[1] | p.b[2]; }

// Index of a winning shape completed by `bits`, or -1.
inline int winIndex(uint32_t bits) {
  for (int i = 0; i < NWIN; i++)
    if ((bits & WIN[i]) == WIN[i]) return i;
  return -1;
}

inline bool dropping(const Pos &p, int w) { return pc(p.b[w]) < PIECES; }

inline int genMoves(const Pos &p, int w, Move *out) {
  uint32_t empty = ~occupied(p) & FULL;
  int n = 0;
  if (dropping(p, w)) {
    for (uint32_t t = empty; t; t &= t - 1) out[n++] = {255, (uint8_t)__builtin_ctz(t)};
  } else {
    for (uint32_t m = p.b[w]; m; m &= m - 1) {
      int c = __builtin_ctz(m);
      for (uint32_t t = NB[c] & empty; t; t &= t - 1)
        out[n++] = {(uint8_t)c, (uint8_t)__builtin_ctz(t)};
    }
  }
  return n;
}

inline Pos applyMove(Pos p, int w, Move m) {
  if (m.from != 255) p.b[w] &= ~(1u << m.from);
  p.b[w] |= 1u << m.to;
  return p;
}

// ---------------- robot ----------------
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
inline unsigned long hardLimitMs = 2500;  // time the HARD robot may think

// Value of a position for the side owning `me`: every winning shape that
// holds no enemy piece counts more the more of my pieces it holds.
inline int evalSide(uint32_t me, uint32_t opp) {
  static const int W[5] = {0, 1, 6, 40, 0};
  int s = 0;
  for (int i = 0; i < NWIN; i++) {
    uint32_t m = WIN[i];
    if (m & opp) continue;
    s += W[pc(m & me)];
  }
  return s;
}
inline int evaluate(const Pos &p, int w) {
  return evalSide(p.b[w], p.b[3 - w]) - evalSide(p.b[3 - w], p.b[w]);
}

struct Search {
  unsigned long t0 = 0, limitMs = 2500;
  uint32_t nodes = 0;
  bool timeUp = false;
};

constexpr int WINV = 100000, MAXDEPTH = 8;

// Negamax with alpha-beta; value from the point of view of w (to move).
inline int negamax(const Pos &p, int w, int depth, int alpha, int beta, Search &S) {
  if ((++S.nodes & 63) == 0 && millis() - S.t0 > S.limitMs) S.timeUp = true;
  if (S.timeUp) return 0;
  Move mv[32];
  int n = genMoves(p, w, mv);
  if (n == 0) {  // no legal move: pass
    if (depth <= 1) return evaluate(p, w);
    return -negamax(p, 3 - w, depth - 1, -beta, -alpha, S);
  }
  int16_t key[32];
  uint8_t idx[32];
  for (int i = 0; i < n; i++) {
    Pos c = applyMove(p, w, mv[i]);
    if (winIndex(c.b[w]) >= 0) return WINV + depth;  // winning move
    key[i] = (int16_t)evaluate(c, w);
    idx[i] = (uint8_t)i;
  }
  if (depth <= 1) {
    int best = key[0];
    for (int i = 1; i < n; i++) if (key[i] > best) best = key[i];
    return best;
  }
  for (int i = 1; i < n; i++) {  // best-looking moves first
    uint8_t t = idx[i];
    int j = i;
    while (j > 0 && key[idx[j - 1]] < key[t]) { idx[j] = idx[j - 1]; j--; }
    idx[j] = t;
  }
  int best = -1000000;
  for (int i = 0; i < n; i++) {
    Pos c = applyMove(p, w, mv[idx[i]]);
    int v = -negamax(c, 3 - w, depth - 1, -beta, -alpha, S);
    if (S.timeUp) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

inline int lastDepth = 0;  // depth reached by the last HARD search (for tests)

inline Move chooseHard(const Pos &p, int w, unsigned long limitMs) {
  Move mv[32];
  int n = genMoves(p, w, mv);
  if (n == 0) return {255, 255};
  int16_t key[32];
  for (int i = 0; i < n; i++) {
    Pos c = applyMove(p, w, mv[i]);
    if (winIndex(c.b[w]) >= 0) return mv[i];  // win at once
    key[i] = (int16_t)evaluate(c, w);
  }
  uint8_t idx[32];
  for (int i = 0; i < n; i++) idx[i] = (uint8_t)i;
  for (int i = 1; i < n; i++) {
    uint8_t t = idx[i];
    int j = i;
    while (j > 0 && key[idx[j - 1]] < key[t]) { idx[j] = idx[j - 1]; j--; }
    idx[j] = t;
  }
  Move best = mv[idx[0]];
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  for (int depth = 2; depth <= MAXDEPTH; depth++) {
    int bestV = -1000000, cnt = 0;
    uint8_t list[32];
    for (int i = 0; i < n; i++) {
      Pos c = applyMove(p, w, mv[idx[i]]);
      int v = -negamax(c, 3 - w, depth - 1, -1000000, -(bestV - 1), S);
      if (S.timeUp) break;
      if (v > bestV) { bestV = v; cnt = 0; }
      if (v == bestV) list[cnt++] = idx[i];
    }
    if (S.timeUp) break;  // incomplete depth: ignore it
    lastDepth = depth;
    best = mv[list[rnd() % cnt]];
    if (bestV >= WINV || bestV <= -WINV) break;  // game decided
  }
  return best;
}

inline Move chooseEasy(const Pos &p, int w) {
  Move mv[32];
  int n = genMoves(p, w, mv);
  if (n == 0) return {255, 255};
  for (int i = 0; i < n; i++)
    if (winIndex(applyMove(p, w, mv[i]).b[w]) >= 0) return mv[i];
  return mv[rnd() % n];
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_PLAY, PH_OVER };
enum Result : uint8_t { R_NONE = 0, R_BLACK, R_WHITE, R_DRAW };

RTC_DATA_ATTR Pos pos;
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanBlack = 1;   // 1P: you play BLACK (first)
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t turn = 1;         // 1 = black to move, 2 = white
RTC_DATA_ATTR uint8_t cursor = 12;      // cell under the cursor
RTC_DATA_ATTR uint8_t from = 255;       // piece picked up (move phase)
RTC_DATA_ATTR uint8_t plies = 0;        // moves played in the move phase
RTC_DATA_ATTR uint8_t result = R_NONE;
RTC_DATA_ATTR uint8_t winKind = 0;      // 0 = line, 1 = square, 2 = move limit
RTC_DATA_ATTR uint32_t winCells = 0;    // the four winning cells
RTC_DATA_ATTR uint8_t lastTo = 255;     // last piece moved / dropped

inline bool robotTurn() {
  return vsRobot && ((turn == 1) != (humanBlack != 0));
}

// 0 = dropping a piece, 1 = picking a piece to move, 2 = choosing where
inline int stage() {
  if (dropping(pos, turn)) return 0;
  return from == 255 ? 1 : 2;
}

inline bool validCell(int c) {
  initTables();
  uint32_t empty = ~occupied(pos) & FULL;
  switch (stage()) {
    case 0:  return (empty >> c) & 1;
    case 1:  return ((pos.b[turn] >> c) & 1) && (NB[c] & empty);
    default: return c == from || ((NB[from] & empty) >> c & 1);
  }
}

inline void fixCursor() {
  if (validCell(cursor)) return;
  for (int c = 0; c < CELLS; c++)
    if (validCell(c)) { cursor = (uint8_t)c; return; }
}

template <typename F>
inline int cycle(int cur, int dir, int count, F valid) {
  for (int k = 1; k <= count; k++) {
    int i = ((cur + dir * k) % count + count) % count;
    if (valid(i)) return i;
  }
  return cur;
}

// Plays move m for the player to move; ends the game if it wins.
inline void playMove(Move m) {
  initTables();
  int w = turn;
  pos = applyMove(pos, w, m);
  lastTo = m.to;
  int wi = winIndex(pos.b[w]);
  if (wi >= 0) {
    winCells = WIN[wi];
    winKind = (wi >= NLINES) ? 1 : 0;
    result = (uint8_t)(w == 1 ? R_BLACK : R_WHITE);
    phase = PH_OVER;
    return;
  }
  if (m.from != 255 && ++plies >= MOVE_CAP) {
    winKind = 2;
    winCells = 0;
    result = R_DRAW;
    phase = PH_OVER;
    return;
  }
  turn = (uint8_t)(3 - w);
}

// After every move: skip a player who cannot move, let the robot play.
inline void advance() {
  initTables();
  for (int guard = 0; guard < 300 && phase != PH_OVER; guard++) {
    Move mv[32];
    if (genMoves(pos, turn, mv) == 0) {  // blocked: pass
      turn = (uint8_t)(3 - turn);
      if (genMoves(pos, turn, mv) == 0) {  // nobody can move: draw
        winKind = 2;
        winCells = 0;
        result = R_DRAW;
        phase = PH_OVER;
        return;
      }
    }
    if (!robotTurn()) break;
    stirRng();
    Move m = hard ? chooseHard(pos, turn, hardLimitMs) : chooseEasy(pos, turn);
    if (m.to == 255) break;
    playMove(m);
  }
  if (phase != PH_OVER) {
    phase = PH_PLAY;
    from = 255;
    fixCursor();
  }
}

inline void newGame() {
  initTables();
  pos.b[0] = pos.b[1] = pos.b[2] = 0;
  turn = 1;
  from = 255;
  plies = 0;
  result = R_NONE;
  winKind = 0;
  winCells = 0;
  lastTo = 255;
  cursor = 12;  // centre
  phase = PH_PLAY;
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
  } else if (phase == PH_PLAY) {
    for (int s = qwGameSteps(); s > 0; s--)
      cursor = (uint8_t)cycle(cursor, dir, CELLS, [](int i) { return validCell(i); });
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
      humanBlack = (menuSel == 0);
      phase = PH_LEVEL;
      menuSel = 0;
      break;
    case PH_LEVEL:
      hard = menuSel;
      newGame();
      break;
    case PH_PLAY: {
      if (!validCell(cursor)) break;
      int st = stage();
      if (st == 0) {
        playMove({255, cursor});
        advance();
      } else if (st == 1) {
        from = cursor;  // the destination is chosen next
      } else if (cursor == from) {
        from = 255;  // same piece again: cancel
      } else {
        playMove({from, cursor});
        from = 255;
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
constexpr int16_t X0 = 20, Y0 = 30, CS = 32;

inline void centerText(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(t), size) / 2, y);
  d.print(t);
}

inline void drawOptionScreen(Adafruit_GFX &d, const char *title, const char *a,
                             const char *b, const char *note) {
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

// Black = solid disc, white = ring. A small dot marks the last piece moved.
inline void drawPiece(Adafruit_GFX &d, int16_t cx, int16_t cy, int w, bool last) {
  d.fillCircle(cx, cy, 11, qwInk());
  if (w == 2) d.fillCircle(cx, cy, 8, qwPaper());
  if (last) d.fillCircle(cx, cy, 2, w == 1 ? qwPaper() : qwInk());
}

inline void frame3(Adafruit_GFX &d, int16_t bx, int16_t by) {
  d.drawRect(bx + 1, by + 1, CS - 1, CS - 1, qwInk());
  d.drawRect(bx + 2, by + 2, CS - 3, CS - 3, qwInk());
  d.drawRect(bx + 3, by + 3, CS - 5, CS - 5, qwInk());
}

inline void drawBoardScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());

  const char *nB = vsRobot ? (humanBlack ? "BLACK (YOU)" : "BLACK (ME)") : "BLACK";
  const char *nW = vsRobot ? (humanBlack ? "WHITE (ME)" : "WHITE (YOU)") : "WHITE";

  char l1[48];
  if (phase == PH_OVER) {
    const char *r;
    if (result == R_DRAW) r = "DRAW";
    else if (vsRobot) r = ((result == R_BLACK) == (humanBlack != 0)) ? "YOU WIN!" : "I WIN";
    else r = (result == R_BLACK) ? "BLACK WINS!" : "WHITE WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    const char *why = winKind == 0 ? "FOUR IN A LINE"
                      : winKind == 1 ? "FOUR IN A SQUARE" : "NO WINNER AFTER 100 MOVES";
    centerText(d, why, 19, 1);
  } else {
    const char *who = (turn == 1) ? nB : nW;
    int st = stage();
    snprintf(l1, sizeof(l1), "%s: %s", who,
             st == 0 ? "PLACE A PIECE" : st == 1 ? "PICK A PIECE" : "MOVE (SAME=CANCEL)");
    centerText(d, l1, 3, 1);
    centerText(d, "UP/DOWN=pick (hold=fast) MENU=ok", 14, 1);
  }

  // grid
  d.drawRect(X0 - 2, Y0 - 2, N * CS + 4, N * CS + 4, qwInk());
  for (int i = 1; i < N; i++) {
    d.drawFastVLine(X0 + i * CS, Y0, N * CS, qwInk());
    d.drawFastHLine(X0, Y0 + i * CS, N * CS, qwInk());
  }
  // pieces
  for (int c = 0; c < CELLS; c++) {
    int w = ((pos.b[1] >> c) & 1) ? 1 : (((pos.b[2] >> c) & 1) ? 2 : 0);
    if (!w) continue;
    int16_t bx = X0 + (c % N) * CS, by = Y0 + (c / N) * CS;
    drawPiece(d, bx + CS / 2, by + CS / 2, w, c == lastTo && phase != PH_OVER);
  }
  // winning four
  if (phase == PH_OVER && winCells)
    for (int c = 0; c < CELLS; c++)
      if ((winCells >> c) & 1) frame3(d, X0 + (c % N) * CS, Y0 + (c / N) * CS);

  if (phase == PH_PLAY) {
    // piece picked up: small squares in the corners of its cell
    if (from != 255) {
      int16_t bx = X0 + (from % N) * CS, by = Y0 + (from / N) * CS;
      d.fillRect(bx + 2, by + 2, 4, 4, qwInk());
      d.fillRect(bx + CS - 5, by + 2, 4, 4, qwInk());
      d.fillRect(bx + 2, by + CS - 5, 4, 4, qwInk());
      d.fillRect(bx + CS - 5, by + CS - 5, 4, 4, qwInk());
    }
    // cursor: 3-pixel frame, plus a dot when the cell is empty
    if (validCell(cursor)) {
      int16_t bx = X0 + (cursor % N) * CS, by = Y0 + (cursor / N) * CS;
      frame3(d, bx, by);
      if (!(occupied(pos) >> cursor & 1))
        d.fillRect(bx + CS / 2 - 1, by + CS / 2 - 1, 3, 3, qwInk());
    }
  }

  // bottom line
  char l2[40];
  if (phase == PH_OVER) {
    snprintf(l2, sizeof(l2), "MENU = back to the menu");
  } else if (dropping(pos, 1) || dropping(pos, 2)) {
    snprintf(l2, sizeof(l2), "PLACED: BLACK %d/4  WHITE %d/4", pc(pos.b[1]), pc(pos.b[2]));
  } else {
    snprintf(l2, sizeof(l2), "MOVE %d/%d (%d = DRAW)", plies + 1, MOVE_CAP, MOVE_CAP);
  }
  centerText(d, l2, 192, 1);
}

inline void draw(Adafruit_GFX &d) {
  initTables();
  if (phase == PH_MODE) {
    drawOptionScreen(d, "TEEKO", "1 PLAYER", "2 PLAYERS", "4 in a line or a square wins");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOU PLAY", "BLACK", "WHITE", "Black plays first");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD", "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Tk
