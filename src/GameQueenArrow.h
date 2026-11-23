#pragma once
/*
  GameQueenArrow.h
  ------------------------------------------------------------
  ARROW QUEENS for QuranWatch (included by Games.h).

  A game for two players on an 8 x 8 board, three queens each (BLACK
  plays first). On your turn you do two things:

    1. MOVE one of your queens like a chess queen: any distance in a
       straight line (row, column or diagonal), without jumping over or
       landing on a queen or on a burnt square;
    2. SHOOT AN ARROW: from the square where the queen just arrived, in
       any queen direction, to an empty square. That square is burnt for
       the rest of the game (the queen's old square is free again, so the
       arrow may fly over it).

  Burnt squares block everybody. The first player who cannot move any
  queen LOSES, so you want to cut the board into a part for you and a
  part that is useless for the other player.

  Origin: this is the "Game of the Amazons" (Walter Zamkauskas, 1988), in
  the 8 x 8 / 3 queens version of "Math Games with Bad Drawings".
  The start position is the table START_BLACK / START_WHITE below: edit
  it to change it.

  1 player: you pick BLACK (first) or WHITE, the watch plays the other
  colour. Robot levels: EASY (random legal moves) and HARD (looks ahead
  for at most ~2.5 seconds, trying to keep more room for itself).

  Moving: UP/DOWN walk through the squares you may pick (hold the button
  to jump 3 or 6 places), MENU confirms. A turn has three picks:
    queen -> where it goes -> where the arrow lands.
  Picking the queen again cancels its move, and while the arrow is
  being aimed, picking the queen undoes the move. Dots show the squares
  you can pick.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Qa {

constexpr int N = 8, CELLS = 64, QUEENS = 3;
typedef uint64_t u64;

// Start position: row 0 is the top of the screen, column 0 the left.
// Each entry is row * 8 + column.
static const uint8_t START_BLACK[QUEENS] = {7 * 8 + 1, 7 * 8 + 4, 7 * 8 + 6};
static const uint8_t START_WHITE[QUEENS] = {0 * 8 + 6, 0 * 8 + 3, 0 * 8 + 1};

// ---------------- bitboards ----------------
constexpr u64 FILE_A = 0x0101010101010101ULL;  // column 0
constexpr u64 FILE_H = 0x8080808080808080ULL;  // column 7

// Moves every set bit one square in direction d (edges do not wrap).
inline u64 shiftDir(u64 x, int d) {
  switch (d) {
    case 0:  return x >> 8;
    case 1:  return x << 8;
    case 2:  return (x << 1) & ~FILE_A;
    case 3:  return (x >> 1) & ~FILE_H;
    case 4:  return (x >> 7) & ~FILE_A;
    case 5:  return (x >> 9) & ~FILE_H;
    case 6:  return (x << 9) & ~FILE_A;
    default: return (x << 7) & ~FILE_H;
  }
}

// Every empty square a queen standing on one of the `from` squares can
// reach in one move.
inline u64 slide(u64 from, u64 empty) {
  u64 res = 0;
  for (int d = 0; d < 8; d++) {
    u64 x = shiftDir(from, d) & empty, y = x;
    for (int i = 0; i < 6; i++) {
      x = shiftDir(x, d) & empty;
      y |= x;
    }
    res |= y;
  }
  return res;
}

inline int pc(u64 x) { return __builtin_popcountll(x); }
inline int lowest(u64 x) { return __builtin_ctzll(x); }

struct Bd {
  u64 q[3];  // q[0] = burnt squares, q[1] = black queens, q[2] = white queens
};
inline u64 emptyOf(const Bd &b) { return ~(b.q[0] | b.q[1] | b.q[2]); }

inline Bd applyTurn(Bd b, int w, int from, int to, int arrow) {
  b.q[w] = (b.q[w] & ~(1ULL << from)) | (1ULL << to);
  b.q[0] |= 1ULL << arrow;
  return b;
}

inline bool canMove(const Bd &b, int w) {
  return slide(b.q[w], emptyOf(b)) != 0;
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

struct Search {
  unsigned long t0 = 0, limitMs = 2500;
  uint32_t nodes = 0;
  bool timeUp = false;
};

// Squares that are closer (in queen moves) to `me` than to the opponent,
// minus the same count for the opponent.
inline int territory(const Bd &b, int me) {
  u64 empty = emptyOf(b);
  u64 fm = b.q[me], fo = b.q[3 - me], seenM = 0, seenO = 0;
  int tm = 0, to = 0;
  for (int k = 0; k < 10; k++) {
    u64 nm = slide(fm, empty) & ~seenM, no = slide(fo, empty) & ~seenO;
    if (!(nm | no)) break;
    tm += pc(nm & ~no & ~seenO);
    to += pc(no & ~nm & ~seenM);
    seenM |= nm;
    seenO |= no;
    fm = nm;
    fo = no;
  }
  return tm - to;
}

inline int mobility(const Bd &b, int w) { return pc(slide(b.q[w], emptyOf(b))); }

// Value of a position for `me`.
inline int fullEval(const Bd &b, int me) {
  return 3 * territory(b, me) + mobility(b, me) - mobility(b, 3 - me);
}

struct Mv {
  uint8_t from, to, arrow;
  int16_t key;
};

// The `B` most promising turns of w (quick judgement: reachable squares
// of both sides), best first. Returns how many were kept; 0 = no move.
inline int genTop(const Bd &b, int w, Mv *out, int B, Search &S) {
  u64 empty = emptyOf(b);
  int cnt = 0;
  for (u64 qs = b.q[w]; qs; qs &= qs - 1) {
    u64 qb = qs & (0 - qs);
    for (u64 ds = slide(qb, empty); ds; ds &= ds - 1) {
      u64 db = ds & (0 - ds);
      u64 empty2 = (empty | qb) & ~db;
      u64 mine = (b.q[w] & ~qb) | db;
      for (u64 as = slide(db, empty2); as; as &= as - 1) {
        u64 ab = as & (0 - as);
        if (((++S.nodes) & 255) == 0 && millis() - S.t0 > S.limitMs) {
          S.timeUp = true;
          return cnt;
        }
        u64 e3 = empty2 & ~ab;
        int key = pc(slide(mine, e3)) - pc(slide(b.q[3 - w], e3));
        if (cnt == B && key <= out[cnt - 1].key) continue;
        int i = (cnt < B) ? cnt++ : cnt - 1;
        while (i > 0 && out[i - 1].key < key) { out[i] = out[i - 1]; i--; }
        out[i] = {(uint8_t)lowest(qb), (uint8_t)lowest(db), (uint8_t)lowest(ab),
                  (int16_t)key};
      }
    }
  }
  return cnt;
}

constexpr int BEAM_ROOT = 14, BEAM_DEEP = 6, MAXDEPTH = 6, WINV = 100000;

// Negamax with alpha-beta over the best few turns; value for w (to move).
inline int negamax(const Bd &b, int w, int depth, int alpha, int beta, Search &S) {
  Mv mv[BEAM_DEEP];
  int n = genTop(b, w, mv, BEAM_DEEP, S);
  if (S.timeUp) return 0;
  if (n == 0) return -WINV - depth;  // w cannot move: w loses
  if (depth <= 1) {
    int best = -1000000;
    for (int i = 0; i < n; i++) {
      int v = fullEval(applyTurn(b, w, mv[i].from, mv[i].to, mv[i].arrow), w);
      if (v > best) best = v;
    }
    return best;
  }
  int best = -1000000;
  for (int i = 0; i < n; i++) {
    int v = -negamax(applyTurn(b, w, mv[i].from, mv[i].to, mv[i].arrow), 3 - w,
                     depth - 1, -beta, -alpha, S);
    if (S.timeUp) return 0;
    if (v > best) best = v;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

inline int lastDepth = 0;  // depth reached by the last HARD search (for tests)

inline bool chooseHard(const Bd &b, int w, unsigned long limitMs, Mv &res) {
  Mv root[BEAM_ROOT];
  Search S;
  S.t0 = millis();
  S.limitMs = limitMs;
  int n = genTop(b, w, root, BEAM_ROOT, S);
  if (n == 0) return false;
  res = root[0];
  if (S.timeUp) return true;  // out of time already: quick judgement
  lastDepth = 0;
  for (int depth = 1; depth <= MAXDEPTH; depth++) {
    int bestV = -1000000, bestI = 0;
    for (int i = 0; i < n; i++) {
      Bd c = applyTurn(b, w, root[i].from, root[i].to, root[i].arrow);
      int v = (depth <= 1) ? fullEval(c, w)
                           : -negamax(c, 3 - w, depth - 1, -1000000, 1000000, S);
      if (S.timeUp) break;
      if (v > bestV) { bestV = v; bestI = i; }
    }
    if (S.timeUp) break;  // incomplete depth: ignore it
    lastDepth = depth;
    res = root[bestI];
    if (bestV >= WINV) break;  // forced win found
  }
  return true;
}

// A random legal turn.
inline bool chooseEasy(const Bd &b, int w, Mv &res) {
  u64 empty = emptyOf(b);
  int qn = pc(b.q[w]), start = (int)(rnd() % (uint32_t)qn);
  for (int t = 0; t < qn; t++) {
    int k = (start + t) % qn;
    u64 qs = b.q[w];
    for (int i = 0; i < k; i++) qs &= qs - 1;
    u64 qb = qs & (0 - qs);
    u64 dests = slide(qb, empty);
    int dn = pc(dests);
    if (!dn) continue;
    int dk = (int)(rnd() % (uint32_t)dn);
    for (int i = 0; i < dk; i++) dests &= dests - 1;
    u64 db = dests & (0 - dests);
    u64 arrows = slide(db, (empty | qb) & ~db);
    int an = pc(arrows), ak = (int)(rnd() % (uint32_t)an);
    for (int i = 0; i < ak; i++) arrows &= arrows - 1;
    u64 ab = arrows & (0 - arrows);
    res = {(uint8_t)lowest(qb), (uint8_t)lowest(db), (uint8_t)lowest(ab), 0};
    return true;
  }
  return false;
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_ROLE, PH_LEVEL, PH_PLAY, PH_OVER };

RTC_DATA_ATTR uint8_t occ[CELLS];  // 0 empty, 1 black queen, 2 white queen, 3 burnt
RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t vsRobot = 1;
RTC_DATA_ATTR uint8_t humanBlack = 1;  // 1P: you play BLACK (first)
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t turn = 1;        // 1 = black to move, 2 = white
RTC_DATA_ATTR uint8_t cursor = 0;
RTC_DATA_ATTR uint8_t from = 255;      // queen picked up
RTC_DATA_ATTR uint8_t to = 255;        // where the queen went (arrow to aim)
RTC_DATA_ATTR uint8_t result = 0;      // 1 black wins, 2 white wins
RTC_DATA_ATTR uint8_t lastTo = 255;    // last queen moved
RTC_DATA_ATTR uint8_t lastArrow = 255; // last arrow shot

inline Bd boardOf() {
  Bd b = {{0, 0, 0}};
  for (int c = 0; c < CELLS; c++)
    if (occ[c]) b.q[occ[c] == 3 ? 0 : occ[c]] |= 1ULL << c;
  return b;
}

inline bool robotTurn() {
  return vsRobot && ((turn == 1) != (humanBlack != 0));
}

// 0 = pick a queen, 1 = pick where it goes, 2 = pick where the arrow lands
inline int stage() {
  if (from == 255) return 0;
  return to == 255 ? 1 : 2;
}

// The squares the player may pick now.
inline u64 validMask() {
  Bd b = boardOf();
  u64 empty = emptyOf(b);
  switch (stage()) {
    case 0: {
      u64 m = 0;
      for (u64 qs = b.q[turn]; qs; qs &= qs - 1) {
        u64 qb = qs & (0 - qs);
        if (slide(qb, empty)) m |= qb;
      }
      return m;
    }
    case 1:  return slide(1ULL << from, empty) | (1ULL << from);
    default: return slide(1ULL << to, empty) | (1ULL << to);  // from is empty then
  }
}

inline void fixCursor() {
  u64 m = validMask();
  if (m & (1ULL << cursor)) return;
  if (m) cursor = (uint8_t)lowest(m);
}

inline int cycleMask(int cur, int dir, u64 m) {
  for (int k = 1; k <= CELLS; k++) {
    int i = ((cur + dir * k) % CELLS + CELLS) % CELLS;
    if ((m >> i) & 1) return i;
  }
  return cur;
}

// The player to move, if blocked, loses; otherwise the robot plays.
inline void advance() {
  for (int guard = 0; guard < 4; guard++) {
    Bd b = boardOf();
    if (!canMove(b, turn)) {
      result = (uint8_t)(3 - turn);
      phase = PH_OVER;
      return;
    }
    if (!robotTurn()) break;
    stirRng();
    Mv m;
    bool ok = hard ? chooseHard(b, turn, hardLimitMs, m) : chooseEasy(b, turn, m);
    if (!ok) break;
    occ[m.from] = 0;
    occ[m.to] = turn;
    occ[m.arrow] = 3;
    lastTo = m.to;
    lastArrow = m.arrow;
    turn = (uint8_t)(3 - turn);
  }
  phase = PH_PLAY;
  from = to = 255;
  fixCursor();
}

inline void newGame() {
  for (int c = 0; c < CELLS; c++) occ[c] = 0;
  for (int i = 0; i < QUEENS; i++) {
    occ[START_BLACK[i]] = 1;
    occ[START_WHITE[i]] = 2;
  }
  turn = 1;
  from = to = 255;
  result = 0;
  lastTo = lastArrow = 255;
  cursor = START_BLACK[1];
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
    u64 m = validMask();
    for (int s = qwGameSteps(); s > 0; s--) cursor = (uint8_t)cycleMask(cursor, dir, m);
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
      if (!((validMask() >> cursor) & 1)) break;
      int st = stage();
      if (st == 0) {
        from = cursor;  // where it goes is picked next
      } else if (st == 1) {
        if (cursor == from) {
          from = 255;  // same queen again: cancel
        } else {
          to = cursor;  // the queen moves; now aim the arrow
          occ[to] = turn;
          occ[from] = 0;
        }
      } else if (cursor == to) {  // queen picked again: undo the move
        occ[from] = turn;
        occ[to] = 0;
        to = 255;
      } else {  // shoot the arrow: the turn is over
        occ[cursor] = 3;
        lastTo = to;
        lastArrow = cursor;
        turn = (uint8_t)(3 - turn);
        advance();
        break;
      }
      fixCursor();
      break;
    }
    case PH_OVER:
      phase = PH_MODE;
      menuSel = 0;
      break;
  }
}

// ---------------- drawing ----------------
constexpr int16_t X0 = 16, Y0 = 30, CS = 21;

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

// Chess queen silhouette, 15 x 14 ('#' = ink). Black queen = solid shape,
// white queen = outline only (the watch has no Unicode font, so the
// symbol is drawn from this table).
static const char *const QUEEN_SHAPE[14] = {
  ".#..#..#..#..#.",
  ".#..#..#..#..#.",
  ".##.##.#.##.##.",
  ".#############.",
  "..###########..",
  "...#########...",
  "....#######....",
  "....#######....",
  "....#######....",
  "...#########...",
  "..###########..",
  "..###########..",
  "...............",
  ".#############.",
};

inline bool queenPix(int x, int y) {
  return x >= 0 && x < 15 && y >= 0 && y < 14 && QUEEN_SHAPE[y][x] == '#';
}

inline void drawQueen(Adafruit_GFX &d, int16_t cx, int16_t cy, int w, bool last) {
  int16_t x0 = cx - 8, y0 = cy - 7;
  for (int y = 0; y < 14; y++)
    for (int x = 0; x < 15; x++) {
      if (!queenPix(x, y)) continue;
      bool edge = !queenPix(x - 1, y) || !queenPix(x + 1, y) ||
                  !queenPix(x, y - 1) || !queenPix(x, y + 1);
      if (w == 1 || edge) d.drawPixel(x0 + x, y0 + y, qwInk());
    }
  // small mark on the queen that moved last
  if (last) d.fillRect(x0 + 6, y0 + 6, 3, 2, w == 1 ? qwPaper() : qwInk());
}

// Burnt square: a thick cross.
inline void drawBurnt(Adafruit_GFX &d, int16_t bx, int16_t by) {
  for (int t = 0; t <= 1; t++) {
    d.drawLine(bx + 5 + t, by + 5, bx + CS - 6 + t, by + CS - 6, qwInk());
    d.drawLine(bx + 5 + t, by + CS - 6, bx + CS - 6 + t, by + 5, qwInk());
  }
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

  if (phase == PH_OVER) {
    const char *r;
    if (vsRobot) r = ((result == 1) == (humanBlack != 0)) ? "YOU WIN!" : "I WIN";
    else r = (result == 1) ? "BLACK WINS!" : "WHITE WINS!";
    qwSetText(d, 2);
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(r), 2) / 2, 2);
    d.print(r);
    char l1[40];
    snprintf(l1, sizeof(l1), "%s HAS NO MOVE LEFT", result == 1 ? nW : nB);
    centerText(d, l1, 19, 1);
  } else {
    char l1[48];
    int st = stage();
    snprintf(l1, sizeof(l1), "%s: %s", turn == 1 ? nB : nW,
             st == 0 ? "PICK A QUEEN" : st == 1 ? "MOVE (SAME=CANCEL)" : "SHOOT (QUEEN=UNDO)");
    centerText(d, l1, 3, 1);
    centerText(d, "UP/DOWN=pick (hold=fast) MENU=ok", 14, 1);
  }

  // grid
  d.drawRect(X0 - 2, Y0 - 2, N * CS + 4, N * CS + 4, qwInk());
  for (int i = 1; i < N; i++) {
    d.drawFastVLine(X0 + i * CS, Y0, N * CS, qwInk());
    d.drawFastHLine(X0, Y0 + i * CS, N * CS, qwInk());
  }
  // pieces and burnt squares
  for (int c = 0; c < CELLS; c++) {
    if (!occ[c]) continue;
    int16_t bx = X0 + (c % N) * CS, by = Y0 + (c / N) * CS;
    if (occ[c] == 3) {
      drawBurnt(d, bx, by);
      if (c == lastArrow && phase != PH_OVER)
        d.drawRect(bx + 2, by + 2, CS - 3, CS - 3, qwInk());
    } else {
      drawQueen(d, bx + CS / 2 + 1, by + CS / 2 + 1, occ[c],
                (c == lastTo && phase != PH_OVER && to == 255));
    }
  }

  if (phase == PH_PLAY) {
    u64 m = validMask();
    int st = stage();
    // dots on the squares that can be picked (queens are marked by the cursor)
    if (st >= 1)
      for (int c = 0; c < CELLS; c++)
        if (((m >> c) & 1) && !occ[c])
          d.fillRect(X0 + (c % N) * CS + CS / 2 - 1, Y0 + (c / N) * CS + CS / 2 - 1, 3, 3,
                     qwInk());
    // queen picked up: small squares in the corners of its square
    int q = (st == 1) ? from : (st == 2 ? to : 255);
    if (q != 255) {
      int16_t bx = X0 + (q % N) * CS, by = Y0 + (q / N) * CS;
      d.fillRect(bx + 1, by + 1, 3, 3, qwInk());
      d.fillRect(bx + CS - 3, by + 1, 3, 3, qwInk());
      d.fillRect(bx + 1, by + CS - 3, 3, 3, qwInk());
      d.fillRect(bx + CS - 3, by + CS - 3, 3, 3, qwInk());
    }
    // cursor
    if ((m >> cursor) & 1)
      frame3(d, X0 + (cursor % N) * CS, Y0 + (cursor / N) * CS);
  }
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_MODE) {
    drawOptionScreen(d, "ARROW QUEENS", "1 PLAYER", "2 PLAYERS", "Trap the other player's queens");
  } else if (phase == PH_ROLE) {
    drawOptionScreen(d, "YOU PLAY", "BLACK", "WHITE", "Black plays first");
  } else if (phase == PH_LEVEL) {
    drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD", "HARD thinks up to 2-3 seconds");
  } else {
    drawBoardScreen(d);
  }
}

}  // namespace Qa
