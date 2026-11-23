#pragma once
/*
  GameSky.h
  ------------------------------------------------------------
  SKY TRADER - a text-only, simplified game inspired by the free game
  Endless Sky (https://endless-sky.github.io, GPL v3). Part of the GAMES
  menu of QuranWatch. The galaxy is a real piece of Endless Sky's map: 47
  systems around Sol, with their real links and trade prices (SkyData.h,
  generated from the game's data files by es/gen.py).

  You are an independent pilot with a small ship:
    TRADE   buy low, sell high (10 commodities, prices drift with time)
    JOBS    cargo, passengers, surveys, bounties (3 active at once)
    EXPLORE jump to new systems (bonus), SCAN them, MINE asteroids
    UPGRADE outfitter (weapons, shields, cargo, fuel) and shipyard (8 ships)
    STORY   a long trail of signals across the map ("The Signal")
  PRICE GUIDE (best places to sell/buy, from the market menu), market events
  (+ scarce / - glut), reputation with the two governments, ARMOR and RADAR
  upgrades, patrol inspections, convoys, 3 short QUEST chains (ASK AROUND in
  PLANET INFO), CAPTAIN'S LOG and REPORT (from STATUS), refuel advice on the
  map when a route is longer than your fuel.
  Random encounters on the way (pirates, derelicts, distress calls,
  traders). Combat is a few simple choices.

  Controls: UP/DOWN choose, MENU confirms (or turns the page of a long
  text), BACK leaves (it asks first). The game is saved in flash after
  every action, so you can stop at any time.

  All state lives in one small struct (kept in RTC memory while the watch
  sleeps and in flash). Screens are rebuilt from that state at every key
  press, so nothing else has to survive deep sleep.
  Plain ASCII only on screen.
*/

#include <Arduino.h>
#include <stdarg.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "Storage.h"
#include "NotoMono13.h"
#include "SkyData.h"

namespace Sk {

constexpr int COLS = 24;
constexpr int PITCH = 15;
constexpr int ROWH = 17;
constexpr int MAXROWS = 50;
constexpr int MAXJOB = 3;
constexpr int NSHIP = 8;
constexpr int NUP = 4;

struct Ship { const char *name; uint32_t cost; uint16_t cargo; uint8_t fuel; uint8_t hull; uint8_t shield; uint8_t slots; uint8_t gun; };
static const Ship SHIPS[NSHIP] = {
  {"Shuttle", 180000, 20, 4, 6, 3, 3, 1},
  {"Star Barge", 190000, 50, 3, 10, 3, 3, 1},
  {"Bounder", 1140000, 40, 8, 7, 22, 5, 2},
  {"Freighter", 730000, 150, 6, 20, 5, 5, 1},
  {"Argosy", 1960000, 120, 4, 26, 42, 6, 2},
  {"Hauler II", 2340000, 260, 4, 52, 29, 6, 1},
  {"Falcon", 10900000, 130, 6, 37, 128, 8, 4},
  {"Bulk Freighter", 11100000, 640, 9, 76, 40, 7, 2},
};
constexpr int NUPT = 6;   // 4 stored in the main save + ARMOR and RADAR (stored in the extra save)
static const char *const UPN[NUPT] = {"WEAPONS", "SHIELDS", "CARGO", "FUEL TANKS", "ARMOR", "RADAR"};
static const uint32_t UPC[NUPT][4] = {{60000, 250000, 800000, 2000000}, {100000, 500000, 1500000, 0}, {50000, 200000, 600000, 0}, {40000, 120000, 300000, 0}, {80000, 300000, 900000, 0}, {120000, 400000, 0, 0}};
static const uint8_t UPMAX[NUPT] = {4, 3, 3, 3, 3, 2};
static const uint8_t UPV[NUPT][4] = {{2, 4, 6, 8}, {6, 14, 26, 0}, {10, 30, 70, 0}, {1, 2, 4, 0}, {3, 8, 16, 0}, {1, 2, 0, 0}};
static const char *const UPD[NUPT] = {"damage in combat", "shield points", "tons of cargo space", "extra jumps of fuel", "extra hull points", "-25% pirate attacks per level"};

struct Job { uint8_t type; uint8_t from; uint8_t to; uint8_t com; uint16_t qty; uint16_t due; uint32_t reward; };  // type 0 empty, 1 cargo, 2 passengers, 3 survey, 4 bounty

struct Sv {
  uint16_t magic;
  uint8_t ship, sys, planet, landed, fuel, hull;
  int32_t credits;
  uint32_t day;
  uint8_t up[NUP];
  uint16_t cargo[SKY_COMM];
  Job job[MAXJOB];
  uint64_t visited, scanned;
  uint8_t story;
  uint8_t taken;            // jobs of the current board already accepted (bit mask)
  uint16_t takenTick;
  uint8_t takenSys;
  uint16_t jobsDone, kills, jumps;
  uint32_t profit;
  uint8_t impSys;
  uint16_t impTick;
  int16_t imp[SKY_COMM];     // tons bought minus sold here this market period (moves the price)
};
constexpr uint16_t MAGIC = 0x5A31;

enum Phase : uint8_t {
  PH_INTRO = 0, PH_SPACE, PH_PORT, PH_PICK, PH_TRADE, PH_TRADE2, PH_JOBS, PH_JOB, PH_ACTIVE, PH_ACT1,
  PH_OUTFIT, PH_YARD, PH_YARD2, PH_INFO, PH_STATUS, PH_EVENT, PH_FIGHT, PH_MSG, PH_JUMP, PH_STORY, PH_MINE, PH_MAP, PH_ROUTE, PH_GUIDE, PH_GUIDE2, PH_REPORT, PH_LOG
};

RTC_DATA_ATTR Sv sv;
RTC_DATA_ATTR uint8_t phase = PH_INTRO;
RTC_DATA_ATTR uint8_t sel = 0, scroll = 0, page = 0;
RTC_DATA_ATTR uint16_t argA = 0, argB = 0;
RTC_DATA_ATTR uint8_t mk = 0;          // message kind
RTC_DATA_ATTR int32_t m1 = 0, m2 = 0;  // message numbers
RTC_DATA_ATTR uint8_t eHull = 0, eShield = 0, eTier = 0, eKind = 0;   // current enemy
RTC_DATA_ATTR uint8_t myShield = 0;
RTC_DATA_ATTR uint16_t evA = 0, evB = 0;
RTC_DATA_ATTR uint32_t rngState = 2463534242u;
RTC_DATA_ATTR uint8_t navT = 255;      // route target system (255 = none)
RTC_DATA_ATTR uint8_t chNote = 0;      // quest step just completed (1 + chain * 3 + step), told in the next message
RTC_DATA_ATTR uint32_t chRew = 0;

// ---- extra save data (flash blob "skyX", kept apart from "skyS" so old games survive)
constexpr int NLOG = 12;
struct Lg { uint16_t day; uint8_t kind, sys; int32_t val; };
struct Ex {
  uint16_t magic;
  uint8_t up2[2];            // ARMOR, RADAR levels
  int8_t rep[2];             // reputation with Republic / Syndicate (-20..20)
  uint8_t ch, cs, cdone;     // quest: chain 1..3 (0 = none), step 0..2, finished chains (bit mask)
  uint8_t lh, lc;            // log ring: next slot, entries
  uint32_t jobEarn, fightEarn;
  int32_t trade;             // cash flow of trading
  Lg lg[NLOG];
};
constexpr uint16_t XMAGIC = 0x5A58;
static Ex ex;
static bool exOk = false, exDirty = false;
inline Ex &X() {
  if (!exOk) {
    Ex t;
    memset(&t, 0, sizeof(t));
    if (QWStorage::loadBlob("skyX", &t, sizeof(t)) && t.magic == XMAGIC) ex = t;
    else { memset(&ex, 0, sizeof(ex)); ex.magic = XMAGIC; }
    exOk = true;
  }
  return ex;
}
inline void exReset() { memset(&ex, 0, sizeof(ex)); ex.magic = XMAGIC; exOk = true; exDirty = true; chNote = 0; }
inline void logEv(uint8_t kind, int32_t val) {
  Ex &e = X();
  e.lg[e.lh].day = (uint16_t)sv.day; e.lg[e.lh].kind = kind; e.lg[e.lh].sys = sv.sys; e.lg[e.lh].val = val;
  e.lh = (uint8_t)((e.lh + 1) % NLOG);
  if (e.lc < NLOG) e.lc++;
  exDirty = true;
}
inline void addRep(int gov, int v) {
  Ex &e = X();
  int r = e.rep[gov & 1] + v;
  e.rep[gov & 1] = (int8_t)(r > 20 ? 20 : r < -20 ? -20 : r);
  exDirty = true;
}
inline int lvl(int t) { return t < NUP ? sv.up[t] : X().up2[t - NUP]; }
inline void setLvl(int t, int v) { if (t < NUP) sv.up[t] = (uint8_t)v; else { X().up2[t - NUP] = (uint8_t)v; exDirty = true; } }

// star map coordinates (x,y per system, 1 unit = 3 px on the map), generated from the jump links
static const int8_t SKY_XY[SKY_N * 2] = {
  0,0,0,-8,-13,12,9,14,2,-21,19,0,-18,-3,-21,30,
  16,29,17,9,11,-25,-12,24,-34,-19,-1,-38,-28,-1,37,2,
  32,33,-34,31,4,37,46,13,-50,-21,-11,35,-24,-28,7,-51,
  50,-4,-14,-35,-6,-47,-44,-30,-21,45,-24,57,41,25,16,-57,
  -45,34,-51,-39,60,-1,42,34,-60,-27,-43,-41,-54,-11,34,44,
  -31,49,57,9,-16,55,-62,-18,7,47,59,-10,6,-63,
};

// hop distances from src over the jump links
inline void bfs(int src, int8_t *dist) {
  uint8_t q[SKY_N]; int h = 0, t = 0;
  for (int i = 0; i < SKY_N; i++) dist[i] = -1;
  dist[src] = 0; q[t++] = (uint8_t)src;
  while (h < t) {
    int u = q[h++];
    for (int k = 0; k < SKY_SYS[u].nLinks; k++) { int v = SKY_SYS[u].links[k]; if (dist[v] < 0) { dist[v] = (int8_t)(dist[u] + 1); q[t++] = (uint8_t)v; } }
  }
}
inline bool navOn() { return navT < SKY_N && navT != sv.sys; }
// index of the link that starts the shortest route to navT (0 if none)
inline uint8_t nextHop() {
  if (!navOn()) return 0;
  int8_t dist[SKY_N]; bfs(navT, dist);
  const SkySys &S = SKY_SYS[sv.sys];
  for (int k = 0; k < S.nLinks; k++) if (dist[S.links[k]] == dist[sv.sys] - 1) return (uint8_t)k;
  return 0;
}

// ---------------------------------------------------------------- helpers
inline uint32_t rnd() {
  rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
  return rngState;
}
inline void seedRng() {
#if defined(ARDUINO_ARCH_ESP32)
  rngState ^= esp_random();
#endif
  rngState ^= (uint32_t)micros() * 2654435761u;
  rngState ^= (uint32_t)millis() * 2246822519u;
  if (!rngState) rngState = 2463534242u;
  for (int i = 0; i < 4; i++) rnd();
}
inline uint32_t hsh(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 2654435761u ^ (b + 0x9E3779B9u) * 2246822519u ^ (c + 0x85EBCA6Bu) * 3266489917u;
  h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
  return h;
}
inline int sysIdx(const char *n) { for (int i = 0; i < SKY_N; i++) if (!strcmp(SKY_SYS[i].name, n)) return i; return 0; }
inline const Ship &ship() { return SHIPS[sv.ship]; }
inline int upBonus(int t) { int l = lvl(t); return l ? UPV[t][l - 1] : 0; }
inline int cargoCap() { return ship().cargo + upBonus(2); }
inline int fuelCap() { return ship().fuel + upBonus(3); }
inline int hullMax() { return ship().hull + upBonus(4); }
inline int shieldMax() { return ship().shield + upBonus(1); }
inline int gunPower() { return 2 + ship().gun + upBonus(0); }
inline int slotsUsed() { int s = 0; for (int i = 0; i < NUPT; i++) s += lvl(i); return s; }
inline int jobSpace() { int s = 0; for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type == 1 || sv.job[i].type == 2) s += sv.job[i].qty; return s; }
inline int cargoUsed() { int s = jobSpace(); for (int i = 0; i < SKY_COMM; i++) s += sv.cargo[i]; return s; }
inline int freeSpace() { int f = cargoCap() - cargoUsed(); return f < 0 ? 0 : f; }
inline int visitedCount() { int c = 0; for (int i = 0; i < SKY_N; i++) if (sv.visited >> i & 1) c++; return c; }
inline int rankIdx() {
  int sc = sv.jobsDone * 5 + visitedCount() * 2 + sv.kills * 3 + sv.story * 10;
  return sc >= 400 ? 5 : sc >= 200 ? 4 : sc >= 100 ? 3 : sc >= 50 ? 2 : sc >= 20 ? 1 : 0;
}
static const char *const RANKS[6] = {"NEWCOMER", "TRADER", "PILOT", "CAPTAIN", "COMMODORE", "ADMIRAL"};

// market event of a system for a commodity: 0 none, 1 scarce (dear), 2 glut (cheap); changes every 20 days
inline int evKind(int s, int c, uint32_t day) {
  int r = (int)(hsh(s, 100 + c, day / 20) % 100);
  return r < 4 ? 1 : (r < 8 ? 2 : 0);
}
inline int priceOf(int s, int c, uint32_t day) {
  int base = SKY_SYS[s].price[c];
  int noise = (int)(hsh(s, c, day / 8) % 13) - 6;
  int p = base * (100 + noise) / 100;
  int e = evKind(s, c, day);
  if (e == 1) p = p * 135 / 100; else if (e == 2) p = p * 70 / 100;
  return p < 20 ? 20 : p;
}
constexpr int IMPK = 600;   // price moves 1/600 per ton traded
inline void syncImp() {
  if (sv.impSys != sv.sys || sv.impTick != (uint16_t)(sv.day / 5)) { sv.impSys = sv.sys; sv.impTick = (uint16_t)(sv.day / 5); memset(sv.imp, 0, sizeof(sv.imp)); }
}
// unit price after the market has been moved by `imp` tons
inline int unitPrice(int base, int imp) {
  int f = IMPK + imp; if (f < IMPK / 2) f = IMPK / 2;
  int p = (int)((int64_t)base * f / IMPK);
  return p < 20 ? 20 : p;
}
inline int64_t costBuy(int c, int n) { syncImp(); int b = priceOf(sv.sys, c, sv.day); int64_t t = 0; for (int i = 0; i < n; i++) t += unitPrice(b, sv.imp[c] + i); return t; }
inline int64_t gainSell(int c, int n) { syncImp(); int b = priceOf(sv.sys, c, sv.day); int64_t t = 0; for (int i = 0; i < n; i++) t += unitPrice(b, sv.imp[c] - 1 - i); return t; }
inline void fmt(char *b, int32_t v) {
  if (v < 0) { snprintf(b, 12, "-%ld", (long)-v); return; }
  if (v < 10000) snprintf(b, 12, "%ld", (long)v);
  else if (v < 100000) snprintf(b, 12, "%ld.%ldK", (long)(v / 1000), (long)((v % 1000) / 100));
  else if (v < 1000000) snprintf(b, 12, "%ldK", (long)(v / 1000));
  else snprintf(b, 12, "%ld.%ldM", (long)(v / 1000000), (long)((v % 1000000) / 100000));
}
inline int32_t addCredits(int32_t v) {
  int64_t c = (int64_t)sv.credits + v;
  if (c > 2000000000LL) c = 2000000000LL;
  if (c < 0) c = 0;
  sv.credits = (int32_t)c;
  return sv.credits;
}

// distances (jumps) from a system
static int8_t dist[SKY_N];
inline void bfs(int from) {
  for (int i = 0; i < SKY_N; i++) dist[i] = -1;
  uint8_t q[SKY_N]; int h = 0, t = 0;
  dist[from] = 0; q[t++] = (uint8_t)from;
  while (h < t) {
    int a = q[h++];
    for (int k = 0; k < SKY_SYS[a].nLinks; k++) {
      int b = SKY_SYS[a].links[k];
      if (dist[b] < 0) { dist[b] = dist[a] + 1; q[t++] = (uint8_t)b; }
    }
  }
}

inline void saveX() { QWStorage::saveBlob("skyX", &ex, sizeof(ex)); exDirty = false; }
inline void save() { QWStorage::saveBlob("skyS", &sv, sizeof(sv)); if (exDirty) saveX(); }
inline void newGame() {
  memset(&sv, 0, sizeof(sv));
  exReset();
  sv.magic = MAGIC;
  sv.ship = 0;
  sv.sys = (uint8_t)sysIdx("Sol");
  sv.planet = SKY_SYS[sv.sys].p0;
  sv.landed = 1;
  sv.fuel = (uint8_t)fuelCap();
  sv.hull = (uint8_t)hullMax();
  sv.credits = 15000;
  sv.day = 1;
  sv.visited = 1ull << sv.sys;
  sv.takenTick = 0xFFFF;
  myShield = (uint8_t)shieldMax();
  phase = PH_INTRO; sel = scroll = page = 0; navT = 255;
}
inline void loadOrNew() {
  if (sv.magic == MAGIC) return;
  Sv t;
  memset(&t, 0, sizeof(t));
  if (QWStorage::loadBlob("skyS", &t, sizeof(t)) && t.magic == MAGIC && t.ship < NSHIP && t.sys < SKY_N) {
    sv = t; phase = PH_SPACE; sel = scroll = page = 0;
    if (sv.landed) phase = PH_PORT;
    myShield = (uint8_t)shieldMax();
  } else newGame();
}

// ---------------------------------------------------------------- jobs
inline int boardTick() { return (int)(sv.day / 4); }
inline void genJob(int slot, Job &j) {
  uint32_t h = hsh(sv.sys, boardTick(), slot + 11);
  bfs(sv.sys);
  uint8_t cand[SKY_N]; int n = 0;
  for (int i = 0; i < SKY_N; i++) if (dist[i] >= 1 && dist[i] <= 3) cand[n++] = (uint8_t)i;
  memset(&j, 0, sizeof(j));
  int r = (int)(h % 100);
  uint32_t h2 = hsh(h, 3, 5), h3 = hsh(h, 7, 9);
  int to = cand[h2 % n];
  if (r < 70) {   // cargo / passengers need a planet at the destination
    for (int k = 0; k < n && !SKY_SYS[to].np; k++) to = cand[(h2 + k + 1) % n];
    if (!SKY_SYS[to].np) r = 80;
  }
  int d = dist[to];
  j.from = sv.sys; j.to = (uint8_t)to;
  if (r < 45 && SKY_SYS[to].np) {
    j.type = 1; j.com = (uint8_t)(h3 % SKY_COMM);
    int cap = cargoCap(); int mx = cap / 2; if (mx < 4) mx = 4;
    j.qty = (uint16_t)(3 + (h3 >> 8) % (uint32_t)mx);
    j.reward = (uint32_t)j.qty * (70 + 45 * d) * SKY_SYS[sv.sys].price[j.com] / 400 + 800 * d;
    j.due = (uint16_t)(sv.day + 8 * d + 6);
  } else if (r < 70 && SKY_SYS[to].np) {
    j.type = 2; j.qty = (uint16_t)(1 + (h3 >> 8) % 8);
    j.reward = (uint32_t)j.qty * (180 + 110 * d);
    j.due = (uint16_t)(sv.day + 8 * d + 6);
  } else if (r < 88) {
    j.type = 3;
    int best = -1;
    for (int k = 0; k < n; k++) { int c = cand[(h2 + k) % n]; if (!(sv.visited >> c & 1)) { best = c; break; } }
    if (best >= 0) { to = best; d = dist[to]; j.to = (uint8_t)to; }
    j.reward = 2500 + 2000 * d + (sv.visited >> to & 1 ? 0 : 1500);
  } else {
    j.type = 4; j.reward = 9000 + 4500 * (SKY_SYS[to].depth);
    j.qty = SKY_SYS[to].depth;
  }
  if (j.type) j.reward = (uint32_t)((uint64_t)j.reward * (uint32_t)(100 + X().rep[SKY_SYS[sv.sys].gov & 1] / 2) / 100);
}
inline int jobDist(const Job &j) { bfs(sv.sys); return dist[j.to] < 0 ? 0 : dist[j.to]; }
inline void jobText(char *b, size_t n, const Job &j, int d) {
  char r[12]; fmt(r, (int32_t)j.reward);
  const char *dn = SKY_SYS[j.to].name;
  if (j.type == 1) snprintf(b, n, "Deliver %d t of %s to %s (%d jumps). Pays %s. Due day %d.", j.qty, SKY_COMM_NAME[j.com], dn, d, r, j.due);
  else if (j.type == 2) snprintf(b, n, "Carry %d passengers to %s (%d jumps). Pays %s. Due day %d.", j.qty, dn, d, r, j.due);
  else if (j.type == 3) snprintf(b, n, "Fly to %s (%d jumps) and survey it. Pays %s.", dn, d, r);
  else snprintf(b, n, "A wanted pirate hides in %s (%d jumps). Destroy it. Pays %s.", dn, d, r);
}

// ---------------------------------------------------------------- screen model
struct Row { char t[27]; uint16_t id; };
static Row rows[MAXROWS];
static int nRows = 0;
static char body[560];
static char title[20];
static bool hud = false;

inline void addRow(uint16_t id, const char *fmtStr, ...) {
  if (nRows >= MAXROWS) return;
  va_list ap; va_start(ap, fmtStr);
  vsnprintf(rows[nRows].t, sizeof(rows[nRows].t), fmtStr, ap);
  va_end(ap);
  rows[nRows].id = id; nRows++;
}
inline void setBody(const char *fmtStr, ...) {
  va_list ap; va_start(ap, fmtStr);
  vsnprintf(body, sizeof(body), fmtStr, ap);
  va_end(ap);
}
inline void appendBody(const char *fmtStr, ...) {
  size_t l = strlen(body);
  va_list ap; va_start(ap, fmtStr);
  vsnprintf(body + l, sizeof(body) - l, fmtStr, ap);
  va_end(ap);
}

// row ids
enum : uint16_t {
  R_NONE = 0, R_BEGIN, R_LAND, R_SCAN, R_MINE, R_JUMP, R_WAIT, R_STATUS, R_TRADE, R_JOBS, R_OUTFIT, R_YARD, R_INFO, R_TAKEOFF,
  R_BACK, R_CONT, R_BUY1, R_BUY10, R_BUYMAX, R_SELL1, R_SELL10, R_SELLALL, R_ACCEPT, R_ACTIVE, R_ABANDON, R_STORY, R_FIRE, R_EVADE,
  R_PAY, R_FLEE, R_SEARCH, R_LEAVE, R_HELP, R_BUYDEAL, R_MAP, R_ROUTE, R_CLEAR, R_GUIDE, R_LOG, R_REPORT, R_CHAIN, R_SELLC, R_PAYTAX, R_RUN, R_PICK = 100, R_ITEM = 200
};

static const char *const STORY_START =
  "A dockworker on Earth mentions a faint, repeating signal picked up beyond Sirius and Caph, near Eteron. Nobody has investigated. Maybe you could scan that quiet system.";
static const char *const STORY_TXT[7] = {
  "",
  "Among the debris you find a dead beacon that still pulses three times, then pauses. Its log points to Procyon.",
  "A second beacon, much older. Its pulses carry a short string of coordinates leading to Nocte.",
  "The third beacon hums faintly. The signal grows stronger in the direction of Fingol.",
  "A buried data core lists a route and a date. The next marker lies at Acamar.",
  "Fragments of a star chart, drawn by a hand that was not human. The last marker is at Muphrid.",
  "At the heart of the system an ancient relay station wakes up and sends its whole archive to your computer: star charts, trade records and a message of welcome. You sell the data and are named a Pathfinder."};
static const char *const STORY_TARGET[7] = {"", "Eteron", "Procyon", "Nocte", "Fingol", "Acamar", "Muphrid"};

inline int storyTarget() { return (sv.story >= 1 && sv.story <= 6) ? sysIdx(STORY_TARGET[sv.story]) : -1; }

// ---- quests: 3 chains of 3 steps (0 = scan a system, 1 = bring cargo to a system with a planet, 2 = destroy a pirate)
struct Ch { uint8_t type; const char *sys; uint8_t com, qty; uint16_t reward; const char *txt; };
static const char *const CHN[3] = {"THE LOST COURIER", "FROZEN HARVEST", "OLD DEBTS"};
static const char *const CHO[3] = {
  "A courier ship vanished near Merak. Its last beacon came from Regulus. Scan that system and see what happened.",
  "Alderamin colonists are short of food. Bring them 10 t of Food and they will share what they know.",
  "A retired miner at Pollux needs 6 t of Heavy Metals and pays well. He asks no questions."};
static const Ch CH[3][3] = {
  {{0, "Regulus", 0, 0, 4000, "You find the courier wreck. Its log says survivors hide on Phecda and need medicine."},
   {1, "Phecda", 5, 6, 6000, "The survivors recover. They tell you the raiders who hit the courier now lurk in Merak."},
   {2, "Merak", 0, 0, 12000, "The raider leader is destroyed. The courier company sends a generous reward."}},
  {{1, "Alderamin", 0, 10, 5000, "Grateful, they speak of a lost cargo pod drifting near Kugel. Scan the system to find it."},
   {0, "Kugel", 0, 0, 6000, "The pod holds strange old crates. Raiders are tracking you; their ship waits at Diphda."},
   {2, "Diphda", 0, 0, 14000, "The raiders are gone. The colonists reward you with their harvest money."}},
  {{1, "Pollux", 8, 6, 7000, "He pays and mentions a hidden vault near Menkalinan. Scan the system for its beacon."},
   {0, "Menkalinan", 0, 0, 9000, "The vault is empty, but a note blames a pirate gang that operates from Menkent."},
   {2, "Menkent", 0, 0, 16000, "You destroy the gang. The miner's old debt is settled and your name is known."}}};
inline int chType() { Ex &e = X(); return e.ch ? CH[e.ch - 1][e.cs].type : -1; }
inline int chainTarget() { Ex &e = X(); return e.ch ? sysIdx(CH[e.ch - 1][e.cs].sys) : -1; }
inline int nextChain() { Ex &e = X(); for (int i = 0; i < 3; i++) if (!(e.cdone >> i & 1)) return i; return -1; }
inline void chObj(char *b, size_t n) {
  Ex &e = X();
  if (!e.ch) { b[0] = 0; return; }
  const Ch &c = CH[e.ch - 1][e.cs];
  if (c.type == 0) snprintf(b, n, "Scan %s", c.sys);
  else if (c.type == 1) snprintf(b, n, "Bring %d t %s to %s", c.qty, SKY_COMM_NAME[c.com], c.sys);
  else snprintf(b, n, "Destroy the pirate at %s", c.sys);
}
// current quest step is finished: pay, log, move on (the text is shown by appendChain)
inline void chainDone() {
  Ex &e = X();
  const Ch &c = CH[e.ch - 1][e.cs];
  chNote = (uint8_t)(1 + (e.ch - 1) * 3 + e.cs); chRew = c.reward;
  addCredits((int32_t)c.reward);
  logEv(8, e.ch * 10 + e.cs + 1);
  addRep(SKY_SYS[sysIdx(c.sys)].gov, 2);
  e.cs++;
  if (e.cs >= 3) { e.cdone |= (uint8_t)(1 << (e.ch - 1)); e.ch = 0; e.cs = 0; }
  exDirty = true;
}
inline void appendChain() {
  if (!chNote) return;
  int i = chNote - 1;
  if (body[0]) appendBody("\n\n");
  appendBody("%s\nReward: %lu", CH[i / 3][i % 3].txt, (unsigned long)chRew);
  chNote = 0;
}


inline void startPhase(uint8_t p, uint16_t a = 0, uint16_t b = 0) { phase = p; sel = scroll = page = 0; argA = a; argB = b; }

enum Msg : uint8_t { MG_NONE = 0, MG_ARRIVE, MG_LAND, MG_TEXT, MG_VICTORY, MG_DEFEAT, MG_LOOT, MG_BOUGHT, MG_NO, MG_SCAN, MG_MINE, MG_STORY, MG_WAIT, MG_PAID, MG_TAX, MG_SOLD, MG_CHAIN };

inline void say(uint8_t kind, int32_t a = 0, int32_t b = 0, uint8_t next = PH_SPACE) {
  mk = kind; m1 = a; m2 = b; startPhase(PH_MSG, next);
}

// ---------------------------------------------------------------- text layout
struct Ln { uint16_t o; uint8_t n; };
static Ln lines[64];
static int nLines = 0, pages = 1, vr = 0, cap = 12;
static int rowCap = 0;      // 0 = automatic
static const char *ttl = "";

inline void wrapBody() {
  nLines = 0;
  const char *p = body;
  while (*p && nLines < 64) {
    const char *ls = p;
    int n = 0, lastSp = -1;
    while (p[n] && p[n] != '\n' && n < COLS) { if (p[n] == ' ') lastSp = n; n++; }
    if (p[n] && p[n] != '\n' && p[n] != ' ' && lastSp > 0) n = lastSp;   // break at last space
    lines[nLines].o = (uint16_t)(ls - body); lines[nLines].n = (uint8_t)n; nLines++;
    p += n;
    if (*p == ' ') p++;
    else if (*p == '\n') p++;
  }
}

inline void fixSel() {
  if (nRows == 0) { sel = 0; scroll = 0; return; }
  if (sel >= nRows) sel = (uint8_t)(nRows - 1);
  if (scroll > sel) scroll = sel;
  if (sel >= scroll + vr) scroll = (uint8_t)(sel - vr + 1);
  if (scroll + vr > nRows) scroll = (uint8_t)(nRows - vr);
}

// ---------------------------------------------------------------- guide / report helpers
static int gS[3], gB[3];   // best 3 known ports to sell / buy a commodity
inline void guideTop(int c) {
  for (int k = 0; k < 3; k++) gS[k] = gB[k] = -1;
  for (int i = 0; i < SKY_N; i++) {
    if (i == sv.sys || !(sv.visited >> i & 1) || !SKY_SYS[i].np) continue;
    int p = priceOf(i, c, sv.day);
    for (int k = 0; k < 3; k++) if (gS[k] < 0 || p > priceOf(gS[k], c, sv.day)) { for (int m = 2; m > k; m--) gS[m] = gS[m - 1]; gS[k] = i; break; }
    for (int k = 0; k < 3; k++) if (gB[k] < 0 || p < priceOf(gB[k], c, sv.day)) { for (int m = 2; m > k; m--) gB[m] = gB[m - 1]; gB[k] = i; break; }
  }
}
inline const char *repName(int r) { return r <= -10 ? "HOSTILE" : r <= -3 ? "COLD" : r < 3 ? "NEUTRAL" : r < 10 ? "FRIENDLY" : "ALLIED"; }
inline int patrolTax() {
  int t = sv.cargo[9] * SKY_SYS[sv.sys].price[9] / 10;
  if (sv.cargo[9] && t < 1) t = 1;
  return t;
}
inline void logText(char *b, size_t n, const Lg &g) {
  char v[12]; fmt(v, g.val);
  const char *sn = SKY_SYS[g.sys < SKY_N ? g.sys : 0].name;
  char t[22];
  switch (g.kind) {
    case 1: snprintf(t, sizeof(t), "Job +%s %.9s", v, sn); break;
    case 2: snprintf(t, sizeof(t), "Pirate +%s", v); break;
    case 3: snprintf(t, sizeof(t), "Defeat -%s", v); break;
    case 4: snprintf(t, sizeof(t), "New: %.12s", sn); break;
    case 5: snprintf(t, sizeof(t), "Signal %ld/6", (long)g.val); break;
    case 6: snprintf(t, sizeof(t), "Ship: %.11s", SHIPS[g.val >= 0 && g.val < NSHIP ? g.val : 0].name); break;
    case 7: snprintf(t, sizeof(t), "Upgrade %.9s", UPN[g.val >= 0 && g.val < NUPT ? g.val : 0]); break;
    case 8: snprintf(t, sizeof(t), "Quest %ld/3 done", (long)(g.val % 10)); break;
    case 10: snprintf(t, sizeof(t), "Convoy +%s", v); break;
    default: snprintf(t, sizeof(t), "..."); break;
  }
  snprintf(b, n, "D%u %s", (unsigned)g.day, t);
}

// ---------------------------------------------------------------- screen builders
inline void hudLines() {
  appendBody("Day %lu  Fuel %d/%d\nHull %d/%d  Hold %d/%d\n", (unsigned long)sv.day, sv.fuel, fuelCap(), sv.hull, hullMax(), cargoUsed(), cargoCap());
}
inline int activeJobs() { int c = 0; for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type) c++; return c; }
inline bool isJobTarget(int s) { for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type && sv.job[i].to == s) return true; return s == chainTarget(); }
static const char *const JT[5] = {"", "CARGO", "PASS", "SURVEY", "BOUNTY"};
static const char *const TIER[5] = {"Raider", "Marauder", "Corsair", "Warlord", "Dreadnought"};
static const char *const FLAV[6] = {
  "A convoy of freighters passes, their running lights blinking in formation.",
  "Your scanners pick up an old, silent satellite. Nothing useful remains.",
  "A solar flare crackles across the comm. You wait it out, then go on.",
  "A pilot hails you with a cheerful greeting and a bad joke, then is gone.",
  "You drift past a field of ice, glittering in the light of the star.",
  "Static on the radio. For a moment it sounds almost like a song."};

inline void buildMsg() {
  ttl = "MESSAGE";
  char a[12];
  switch (mk) {
    case MG_ARRIVE:
      setBody("You arrive at %s.", SKY_SYS[sv.sys].name);
      if (m1 > 0) appendBody("\nDiscovery bonus: %ld credits.", (long)m1);
      if (m2 > 0) appendBody("\nSurvey complete: %ld credits.", (long)m2);
      break;
    case MG_LAND:
      setBody("Landed on %s. Your ship is repaired and refueled.", SKY_PLANET[sv.planet].name);
      if (m2 > 0) appendBody("\nDelivered %ld job(s): +%ld credits.", (long)m2, (long)m1);
      appendChain();
      break;
    case MG_TEXT:
      if (m1 >= 100) setBody("%s", STORY_START); else setBody("%s", STORY_TXT[m1]);
      if (m2 > 0) appendBody("\n\nReward: %ld credits. Title earned: PATHFINDER.", (long)m2);
      ttl = "THE SIGNAL";
      break;
    case MG_VICTORY:
      setBody("Enemy ship destroyed! You salvage %ld credits.", (long)m1);
      if (m2 > 0) appendBody("\nBounty paid: %ld credits.", (long)m2);
      appendChain();
      break;
    case MG_DEFEAT:
      fmt(a, (int32_t)m1);
      setBody("Your ship is crippled. You lose %s credits and a quarter of your cargo. A tug tows you to %s.", a, SKY_SYS[m2].name);
      break;
    case MG_LOOT:
      if (m1 >= 0) setBody("You recover %ld t of %s.", (long)m2, SKY_COMM_NAME[m1]);
      else if (m1 == -1) setBody("You find %ld credits.", (long)m2);
      else if (m1 == -2) setBody("A booby trap damages your hull!");
      else if (m1 == -3) setBody("The grateful captain pays you %ld credits.", (long)m2);
      else if (m1 == -4) setBody("Only background noise. Nothing here.");
      else if (m1 == -5) setBody("You escape into the void.");
      else if (m1 == -6) setBody("Broke and stranded, a kind dockworker lends you 500 credits.");
      else setBody("You leave the wreck.");
      break;
    case MG_BOUGHT:
      if (m1 == 1) setBody("%s upgraded to level %ld.", UPN[m2 / 10], (long)(m2 % 10));
      else if (m1 == 2) setBody("You now fly a %s. Your old ship was traded in.", SHIPS[m2].name);
      else setBody("Deal! You load %ld t of %s.", (long)(m2 / 100), SKY_COMM_NAME[m2 % 100]);
      break;
    case MG_MINE:
      setBody("You mine %ld t of %s.", (long)m2, SKY_COMM_NAME[m1]);
      break;
    case MG_WAIT:
      setBody("You wait %ld days. Fuel recharged by %ld.", (long)m1, (long)m2);
      break;
    case MG_PAID:
      setBody("You pay the pirates %ld credits. They leave you alone.", (long)m1);
      break;
    case MG_TAX:
      if (m2) setBody("The patrol catches you! Fine: %ld credits.", (long)m1);
      else setBody("You pay the tax of %ld credits. The patrol thanks you.", (long)m1);
      break;
    case MG_SOLD:
      setBody("The convoy buys %d t of %s and pays %ld credits.", (int)evB, SKY_COMM_NAME[m1], (long)m2);
      break;
    case MG_CHAIN:
      ttl = "QUEST";
      if (m1 >= 100) setBody("%s\n\n%s", CHN[m1 - 100], CHO[m1 - 100]); else body[0] = 0;
      appendChain();
      break;
    default: {
      static const char *const NO[] = {"", "Not enough credits.", "No free upgrade slots on this ship.", "Already at maximum level.",
        "Your cargo would not fit in that ship.", "Not enough cargo space.", "You already have 3 jobs.", "Your hold is full.", "You need 2 fuel to help."};
      setBody("%s", NO[m1 > 0 && m1 < 9 ? m1 : 0]);
    }
  }
  addRow(R_CONT, "OK");
}

inline void tradeIn(int32_t &v) {
  uint32_t t = SHIPS[sv.ship].cost;
  for (int i = 0; i < NUPT; i++) for (int l = 0; l < lvl(i); l++) t += UPC[i][l];
  v = (int32_t)(t / 100 * 70);
}

inline void build() {
  nRows = 0; body[0] = 0; rowCap = 0; ttl = "SKY TRADER";
  const SkySys &S = SKY_SYS[sv.sys];
  char a[12], b[12];
  switch (phase) {
    case PH_INTRO:
      ttl = "SKY TRADER";
      setBody("You own a worn Shuttle and 15K credits. Trade goods, take jobs, explore the stars. Rumors on Earth speak of a strange signal. Based on Endless Sky.");
      addRow(R_BEGIN, "START");
      break;
    case PH_SPACE:
      ttl = S.name;
      setBody("%s space\n", SKY_GOV_NAME[S.gov]);
      hudLines();
      if (S.np) appendBody("%d landable planet%s", S.np, S.np > 1 ? "s" : ""); else appendBody("Uninhabited");
      if (S.mine) appendBody(", asteroids");
      if (S.np) addRow(R_LAND, "LAND");
      if (!(sv.scanned >> sv.sys & 1) || (sv.story >= 1 && sv.story <= 6 && storyTarget() == sv.sys) || (chType() == 0 && chainTarget() == sv.sys)) addRow(R_SCAN, "SCAN SYSTEM");
      if (S.mine) addRow(R_MINE, "MINE ASTEROIDS");
      addRow(R_MAP, "MAP / JUMP");
      addRow(R_ROUTE, "ROUTE TO...");
      addRow(R_WAIT, "WAIT 2 DAYS");
      addRow(R_STATUS, "STATUS");
      break;
    case PH_PORT: {
      const SkyPlanet &P = SKY_PLANET[sv.planet];
      ttl = P.name;
      setBody("%s, %s\n", S.name, SKY_GOV_NAME[S.gov]);
      hudLines();
      addRow(R_TRADE, "TRADE");
      addRow(R_JOBS, "JOB BOARD");
      if (P.flags & 2) addRow(R_OUTFIT, "OUTFITTER");
      if (P.flags & 1) addRow(R_YARD, "SHIPYARD");
      addRow(R_INFO, "PLANET INFO");
      addRow(R_STATUS, "STATUS");
      addRow(R_TAKEOFF, "TAKE OFF");
      break;
    }
    case PH_PICK:
      ttl = "LAND ON";
      setBody("Choose a planet in %s.", S.name);
      for (int i = 0; i < S.np; i++) addRow(R_PICK + i, "%s", SKY_PLANET[S.p0 + i].name);
      addRow(R_BACK, "BACK");
      break;
    case PH_JUMP: {
      ttl = "JUMP TO";
      if (sv.fuel == 0) { setBody("No fuel! Wait in space to recharge."); addRow(R_BACK, "BACK"); break; }
      setBody("Fuel %d/%d. A jump costs 1 fuel and 1 day.", sv.fuel, fuelCap());
      int st = storyTarget();
      for (int k = 0; k < S.nLinks; k++) {
        int d = S.links[k];
        const char *mark = (d == st) ? "SIG" : isJobTarget(d) ? "JOB" : "";
        addRow(R_ITEM + k, "%-10.10s %-3s %s", SKY_SYS[d].name, (sv.visited >> d & 1) ? "" : "NEW", mark);
      }
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_ROUTE: {
      ttl = "ROUTE TO"; rowCap = 9;
      setBody("Pick a destination.");
      int8_t dist[SKY_N]; bfs(sv.sys, dist);
      uint8_t ord[SKY_N]; int n = 0; int st = storyTarget();
      for (int i = 0; i < SKY_N; i++) if (i != sv.sys && dist[i] > 0) {
        int key = ((i == st || isJobTarget(i)) ? 0 : 64) + dist[i], j = n++;
        while (j > 0) {
          int o = ord[j - 1], ko = ((o == st || isJobTarget(o)) ? 0 : 64) + dist[o];
          if (ko <= key) break;
          ord[j] = (uint8_t)o; j--;
        }
        ord[j] = (uint8_t)i;
      }
      if (navOn()) addRow(R_CLEAR, "CLEAR ROUTE");
      for (int k = 0; k < n; k++) {
        int i = ord[k];
        const char *mark = (i == st) ? "SIG" : isJobTarget(i) ? "JOB" : (i == navT) ? "NAV" : (sv.visited >> i & 1) ? "" : "NEW";
        addRow(R_ITEM + i, "%-10.10s %-3s %dj", SKY_SYS[i].name, mark, dist[i]);
      }
      break;
    }
    case PH_MAP: ttl = "MAP"; for (int k = 0; k < S.nLinks; k++) addRow(R_CONT, "-"); break;   // one dummy row per link keeps sel from being clamped
    case PH_TRADE:
      ttl = "MARKET";
      syncImp();
      setBody("Goods       Cost  Own +-");
      for (int c = 0; c < SKY_COMM; c++) { int ev = evKind(sv.sys, c, sv.day); addRow(R_ITEM + c, "%-11.11s %4d %4d %c", SKY_COMM_NAME[c], unitPrice(priceOf(sv.sys, c, sv.day), sv.imp[c]), sv.cargo[c], ev == 1 ? '+' : ev == 2 ? '-' : ' '); }
      addRow(R_GUIDE, "PRICE GUIDE");
      addRow(R_BACK, "BACK");
      rowCap = 9;
      break;
    case PH_TRADE2: {
      syncImp();
      int c = argA, p = unitPrice(priceOf(sv.sys, c, sv.day), sv.imp[c]);
      ttl = SKY_COMM_NAME[c];
      fmt(a, sv.credits);
      setBody("Buy %d  Sell %d\nYou own %d t, free %d t.\nCredits %s", p, unitPrice(priceOf(sv.sys, c, sv.day), sv.imp[c]), sv.cargo[c], freeSpace(), a);
      { int ev = evKind(sv.sys, c, sv.day); if (ev) appendBody("\nMarket: %s", ev == 1 ? "SCARCE (high)" : "GLUT (low)"); }
      addRow(R_BUY1, "BUY 1");
      addRow(R_BUY10, "BUY 10");
      addRow(R_BUYMAX, "BUY MAX");
      if (sv.cargo[c]) { addRow(R_SELL1, "SELL 1"); addRow(R_SELL10, "SELL 10"); addRow(R_SELLALL, "SELL ALL"); }
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_GUIDE:
      ttl = "PRICE GUIDE"; rowCap = 9;
      setBody("Best sale (known ports)");
      for (int c = 0; c < SKY_COMM; c++) {
        guideTop(c);
        if (gS[0] >= 0) addRow(R_ITEM + c, "%-11.11s%+5d %-5.5s", SKY_COMM_NAME[c], priceOf(gS[0], c, sv.day) - priceOf(sv.sys, c, sv.day), SKY_SYS[gS[0]].name);
        else addRow(R_ITEM + c, "%-11.11s   --", SKY_COMM_NAME[c]);
      }
      addRow(R_BACK, "BACK");
      break;
    case PH_GUIDE2: {
      int c = argA, here = priceOf(sv.sys, c, sv.day), ev = evKind(sv.sys, c, sv.day);
      int8_t dd[SKY_N]; bfs(sv.sys, dd);
      ttl = SKY_COMM_NAME[c];
      guideTop(c);
      setBody("Here today: %d%s\nSELL HIGH AT:", here, ev == 1 ? " SCARCE" : ev == 2 ? " GLUT" : "");
      for (int k = 0; k < 3; k++) if (gS[k] >= 0) appendBody("\n %-9.9s%2dj %4d %+d", SKY_SYS[gS[k]].name, dd[gS[k]], priceOf(gS[k], c, sv.day), priceOf(gS[k], c, sv.day) - here);
      if (gS[0] < 0) appendBody("\n none known yet");
      appendBody("\nBUY LOW AT:");
      for (int k = 0; k < 3; k++) if (gB[k] >= 0) appendBody("\n %-9.9s%2dj %4d %+d", SKY_SYS[gB[k]].name, dd[gB[k]], priceOf(gB[k], c, sv.day), priceOf(gB[k], c, sv.day) - here);
      if (gB[0] < 0) appendBody("\n none known yet");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_REPORT: {
      ttl = "REPORT";
      Ex &e = X();
      int32_t ti; tradeIn(ti);
      int64_t nw = (int64_t)sv.credits + ti;
      for (int c = 0; c < SKY_COMM; c++) nw += (int64_t)sv.cargo[c] * SKY_SYS[sv.sys].price[c];
      char c2[12], d2[12];
      fmt(a, nw > 2000000000LL ? 2000000000 : (int32_t)nw);
      fmt(b, e.jobEarn > 2000000000u ? 2000000000 : (int32_t)e.jobEarn);
      fmt(c2, e.fightEarn > 2000000000u ? 2000000000 : (int32_t)e.fightEarn);
      fmt(d2, e.trade);
      setBody("NET WORTH %s\nJobs paid %s\nCombat %s\nTrading %s\nJumps %u  Day %lu\n%-9s %-8s %+d\n%-9s %-8s %+d\nQuests done %d/3",
              a, b, c2, d2, (unsigned)sv.jumps, (unsigned long)sv.day,
              SKY_GOV_NAME[0], repName(e.rep[0]), e.rep[0], SKY_GOV_NAME[1], repName(e.rep[1]), e.rep[1],
              (e.cdone & 1) + (e.cdone >> 1 & 1) + (e.cdone >> 2 & 1));
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_LOG: {
      ttl = "CAPTAIN'S LOG";
      Ex &e = X();
      body[0] = 0;
      if (!e.lc) setBody("No entries yet.");
      for (int k = 0; k < e.lc; k++) {
        char t[28]; logText(t, sizeof(t), e.lg[(e.lh + NLOG - 1 - k) % NLOG]);
        appendBody("%s\n", t);
      }
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_JOBS: {
      ttl = "JOB BOARD";
      setBody("Active %d/%d. New jobs every 4 days.", activeJobs(), MAXJOB);
      bool same = sv.takenSys == sv.sys && sv.takenTick == boardTick();
      for (int s = 0; s < 3; s++) {
        if (same && (sv.taken >> s & 1)) continue;
        Job j; genJob(s, j);
        fmt(a, (int32_t)j.reward);
        addRow(R_ITEM + s, "%-7s%.15s", JT[j.type], SKY_SYS[j.to].name);
      }
      addRow(R_ACTIVE, "MY ACTIVE JOBS");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_JOB: {
      Job j; genJob(argA, j);
      ttl = JT[j.type];
      char t[150]; jobText(t, sizeof(t), j, jobDist(j));
      setBody("%s", t);
      if (j.type == 1 || j.type == 2) appendBody("\nHold free: %d t.", freeSpace());
      addRow(R_ACCEPT, "ACCEPT");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_ACTIVE:
      ttl = "ACTIVE JOBS";
      setBody("Late deliveries pay half.");
      for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type) {
        fmt(a, (int32_t)sv.job[i].reward);
        addRow(R_ITEM + i, "%-7s%.15s", JT[sv.job[i].type], SKY_SYS[sv.job[i].to].name);
      }
      addRow(R_BACK, "BACK");
      break;
    case PH_ACT1:
      if (argA == 1) {
        ttl = "RESET GAME";
        setBody("Erase your ship, money and progress and start over?");
        addRow(R_CONT, "YES, RESET");
        addRow(R_BACK, "NO");
      } else {
        const Job &j = sv.job[argB];
        ttl = JT[j.type];
        char t[150]; jobText(t, sizeof(t), j, jobDist(j));
        setBody("%s", t);
        addRow(R_ABANDON, "ABANDON JOB");
        addRow(R_BACK, "BACK");
      }
      break;
    case PH_OUTFIT:
      ttl = "OUTFITTER";
      setBody("Slots %d/%d. Upgrades are lost when you change ship.", slotsUsed(), ship().slots);
      for (int t = 0; t < NUPT; t++) {
        if (lvl(t) >= UPMAX[t]) snprintf(b, sizeof(b), "MAX"); else fmt(b, (int32_t)UPC[t][lvl(t)]);
        addRow(R_ITEM + t, "%-10.10s L%d/%d %5s", UPN[t], lvl(t), UPMAX[t], b);
      }
      addRow(R_BACK, "BACK");
      break;
    case PH_YARD:
      ttl = "SHIPYARD";
      fmt(a, sv.credits);
      setBody("Your ship: %s. Credits %s. Trade-in is 70%% of its value.", ship().name, a);
      for (int i = 0; i < NSHIP; i++) {
        if (i == sv.ship) snprintf(b, sizeof(b), "OWNED"); else fmt(b, (int32_t)SHIPS[i].cost);
        addRow(R_ITEM + i, "%-14.14s%7s", SHIPS[i].name, b);
      }
      addRow(R_BACK, "BACK");
      break;
    case PH_YARD2: {
      const Ship &h = SHIPS[argA];
      int32_t ti; tradeIn(ti);
      ttl = h.name;
      fmt(a, (int32_t)h.cost); fmt(b, (int32_t)h.cost - ti);
      setBody("Cargo %d t\nFuel %d jumps\nHull %d  Shield %d\nGuns %d  Slots %d\nPrice %s, net %s", h.cargo, h.fuel, h.hull, h.shield, h.gun, h.slots, a, b);
      if (argA != sv.ship) addRow(R_BUYDEAL, "BUY THIS SHIP");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_INFO: {
      const SkyPlanet &P = SKY_PLANET[sv.planet];
      ttl = P.name;
      setBody("%s", P.desc);
      if (sv.story == 0 && !strcmp(P.name, "Earth")) addRow(R_STORY, "LISTEN TO RUMORS");
      if (!X().ch && nextChain() >= 0) addRow(R_CHAIN, "ASK AROUND (QUEST)");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_STATUS: {
      ttl = "STATUS";
      fmt(a, sv.credits);
      setBody("%s - %s\nCredits %s, day %lu\nHull %d/%d Shield %d\nGuns %d Fuel %d/%d\nHold %d/%d t\nSystems %d/%d\nJobs %d Kills %d\n", RANKS[rankIdx()], ship().name, a, (unsigned long)sv.day,
              sv.hull, hullMax(), shieldMax(), gunPower(), sv.fuel, fuelCap(), cargoUsed(), cargoCap(), visitedCount(), SKY_N, sv.jobsDone, sv.kills);
      if (sv.story == 0) appendBody("Signal: not heard yet");
      else if (sv.story <= 6) appendBody("Signal: next at %s (%d/6)", STORY_TARGET[sv.story], sv.story - 1);
      else appendBody("Signal: PATHFINDER");
      if (X().ch) { char o[40]; chObj(o, sizeof(o)); appendBody("\nQuest: %s\n%s", CHN[X().ch - 1], o); }
      if (activeJobs()) addRow(R_ACTIVE, "MY ACTIVE JOBS");
      addRow(R_REPORT, "REPORT");
      addRow(R_LOG, "CAPTAIN'S LOG");
      addRow(R_HELP, "RESET GAME");
      addRow(R_BACK, "BACK");
      break;
    }
    case PH_EVENT:
      ttl = "ENCOUNTER";
      if (eKind == 2) { setBody("You find a drifting derelict ship, dark and silent."); addRow(R_SEARCH, "SEARCH IT"); addRow(R_LEAVE, "LEAVE"); }
      else if (eKind == 3) {
        setBody("A freighter is out of fuel and begs for help.");
        if (sv.fuel >= 2) addRow(R_SEARCH, "GIVE 1 FUEL");
        addRow(R_LEAVE, "IGNORE");
      } else if (eKind == 4) {
        int c = evA, q = evB, p = SKY_SYS[sv.sys].price[c] * 60 / 100;
        setBody("A passing trader sells %d t of %s at %d each (cheap!).", q, SKY_COMM_NAME[c], p);
        if (sv.credits >= p * q) addRow(R_BUYDEAL, "BUY ALL (%d)", p * q);
        addRow(R_LEAVE, "NO THANKS");
      } else if (eKind == 6) {
        int t = patrolTax();
        if (t > 0) { setBody("A %s patrol demands an inspection. Luxury goods carry a tax of %d credits.", SKY_GOV_NAME[S.gov], t); addRow(R_PAYTAX, "PAY TAX"); addRow(R_RUN, "RUN FOR IT"); }
        else { setBody("A %s patrol inspects your hold and finds nothing wrong. They wish you a safe trip.", SKY_GOV_NAME[S.gov]); addRow(R_LEAVE, "OK"); }
      } else if (eKind == 7) {
        int c = evA, q = evB;
        setBody("A merchant convoy pays 130%% for %d t of %s: %d credits.", q, SKY_COMM_NAME[c], q * S.price[c] * 130 / 100);
        addRow(R_SELLC, "SELL ALL (%d)", q * S.price[c] * 130 / 100);
        addRow(R_LEAVE, "NO THANKS");
      } else if (eKind == 8) {
        int c = evA, bs = -1, bp = 0;
        for (int i = 0; i < SKY_N; i++) if (i != sv.sys && SKY_SYS[i].np) { int p = priceOf(i, c, sv.day); if (p > bp) { bp = p; bs = i; } }
        setBody("A friendly pilot whispers: %s fetches %d at %s today.", SKY_COMM_NAME[c], bp, bs >= 0 ? SKY_SYS[bs].name : "Sol");
        addRow(R_LEAVE, "OK");
      } else { setBody("%s", FLAV[evA % 6]); addRow(R_LEAVE, "OK"); }
      break;
    case PH_FIGHT: {
      ttl = eKind ? "WANTED PIRATE" : "PIRATES";
      setBody("%s\nFoe: hull %d shield %d\nYou: hull %d/%d shield %d/%d\n", TIER[eTier], eHull, eShield, sv.hull, hullMax(), myShield, shieldMax());
      if (m1 > 0) appendBody("You hit for %ld. ", (long)m1);
      else if (argB & 1) appendBody("You miss. ");
      if (m2 > 0) appendBody("It hits for %ld.", (long)m2);
      else if (argB & 2) appendBody("It misses.");
      addRow(R_FIRE, "FIRE");
      addRow(R_EVADE, "EVADE");
      if (!eKind) addRow(R_PAY, "PAY THEM OFF");
      addRow(R_FLEE, "FLEE");
      break;
    }
    case PH_MSG: buildMsg(); break;
    default: setBody("..."); addRow(R_CONT, "OK");
  }
  // layout
  wrapBody();
  int cr = rowCap ? rowCap : (nLines ? 5 : 10);
  vr = nRows < cr ? nRows : cr;
  int top = 198 - vr * ROWH;
  cap = (top - 18) / 15; if (cap < 1) cap = 1;
  pages = (nLines + cap - 1) / cap; if (pages < 1) pages = 1;
  if (page >= pages) page = (uint8_t)(pages - 1);
  if (page < pages - 1) {   // not the last page: only a NEXT row
    nRows = 0; addRow(R_NONE, "NEXT >  %d/%d", page + 1, pages); vr = 1;
  }
  fixSel();
}

// ---------------------------------------------------------------- game actions
inline void removeJob(int i) { memset(&sv.job[i], 0, sizeof(Job)); }

inline void setupFight(uint8_t kind, int tier) {
  if (tier < 0) tier = 0;
  if (tier > 4) tier = 4;
  eKind = kind; eTier = (uint8_t)tier;
  eHull = (uint8_t)(5 + 7 * tier); eShield = (uint8_t)(2 + 5 * tier);
  myShield = (uint8_t)shieldMax();
  m1 = m2 = 0; argB = 0;
}
inline int pirateTier(int depth) { int t = depth - 1 + (int)(rnd() & 1); return t < 0 ? 0 : t > 4 ? 4 : t; }

inline void land(int pl) {
  sv.planet = (uint8_t)pl; sv.landed = 1;
  sv.fuel = (uint8_t)fuelCap(); sv.hull = (uint8_t)hullMax();
  int cnt = 0; int32_t tot = 0;
  for (int i = 0; i < MAXJOB; i++) {
    Job &j = sv.job[i];
    if ((j.type == 1 || j.type == 2) && j.to == sv.sys) {
      uint32_t r = j.reward;
      if (sv.day > j.due) r /= 2;
      tot += (int32_t)r; cnt++; sv.jobsDone++;
      removeJob(i);
    }
  }
  addCredits(tot);
  if (cnt) { X().jobEarn += (uint32_t)tot; logEv(1, tot); addRep(SKY_SYS[sv.sys].gov, cnt); }
  if (chType() == 1) {   // quest: cargo for this system
    const Ch &c = CH[X().ch - 1][X().cs];
    if (sysIdx(c.sys) == sv.sys && sv.cargo[c.com] >= c.qty) { sv.cargo[c.com] -= c.qty; chainDone(); }
  }
  if (sv.credits < 300 && cargoUsed() == 0) { addCredits(500); say(MG_LOOT, -6, 500, PH_PORT); return; }
  if (cnt || chNote) say(MG_LAND, tot, cnt, PH_PORT); else startPhase(PH_PORT);
}

inline void defeat() {
  int32_t lost = sv.credits / 10;
  addCredits(-lost);
  logEv(3, lost);
  for (int i = 0; i < SKY_COMM; i++) sv.cargo[i] -= sv.cargo[i] / 4;
  bfs(sv.sys);
  int best = -1;
  for (int i = 0; i < SKY_N; i++) if (SKY_SYS[i].np && dist[i] >= 0 && (best < 0 || dist[i] < dist[best])) best = i;
  if (best < 0) best = sysIdx("Sol");
  int d = dist[best] < 0 ? 1 : dist[best];
  sv.day += d + 1;
  sv.sys = (uint8_t)best; sv.visited |= 1ull << best;
  sv.planet = SKY_SYS[best].p0; sv.landed = 1;
  sv.fuel = (uint8_t)fuelCap(); sv.hull = (uint8_t)hullMax();
  say(MG_DEFEAT, lost, best, PH_PORT);
}

inline void victory() {
  int32_t loot = 400 * (eTier + 1) + (int32_t)(rnd() % (300u * (eTier + 1)));
  int32_t bounty = 0;
  if (eKind == 1) {
    for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type == 4 && sv.job[i].to == sv.sys) { bounty = (int32_t)sv.job[i].reward; removeJob(i); sv.jobsDone++; break; }
  }
  addCredits(loot + bounty);
  sv.kills++;
  X().fightEarn += (uint32_t)(loot + bounty); exDirty = true;
  logEv(2, loot + bounty);
  addRep(SKY_SYS[sv.sys].gov, eKind == 1 ? 2 : 1);
  if (eKind == 1 && chType() == 2 && chainTarget() == sv.sys) chainDone();
  say(MG_VICTORY, loot, bounty, PH_SPACE);
}

inline int hitMe(int chance) {
  if ((int)(rnd() % 100) >= chance) return 0;
  int d = 1 + eTier, ab = d < myShield ? d : myShield;
  myShield -= (uint8_t)ab;
  int rest = d - ab;
  if (rest > 0) sv.hull = (uint8_t)(sv.hull > rest ? sv.hull - rest : 0);
  return d;
}

inline void fightAction(uint16_t id) {
  m1 = 0; m2 = 0; argB = 0;
  if (id == R_FIRE) {
    argB |= 1;
    if (rnd() % 100 < 75) {
      int dmg = gunPower(), s = dmg < eShield ? dmg : eShield;
      eShield -= (uint8_t)s;
      int rest = dmg - s;
      eHull = (uint8_t)(eHull > rest ? eHull - rest : 0);
      m1 = dmg;
      if (eHull == 0) { victory(); return; }
    }
    argB |= 2; m2 = hitMe(70);
  } else if (id == R_EVADE) {
    argB |= 2;
    int add = 2; if (myShield + add > shieldMax()) add = shieldMax() - myShield;
    if (add > 0) myShield += (uint8_t)add;
    m2 = hitMe(25);
  } else if (id == R_PAY) {
    int32_t cost = 300 * (eTier + 1) + sv.credits / 100;
    if (cost > sv.credits) cost = sv.credits;
    if (cost > 0) { addCredits(-cost); say(MG_PAID, cost, 0, PH_SPACE); return; }
    argB |= 2; m2 = hitMe(70);
  } else {   // flee
    if (rnd() % 100 < 45) { say(MG_LOOT, -5, 0, PH_SPACE); return; }
    argB |= 2; m2 = hitMe(70);
  }
  if (sv.hull == 0) defeat();
}

inline void doJump(int k) {
  const SkySys &S = SKY_SYS[sv.sys];
  int d = S.links[k];
  if (sv.fuel == 0) return;
  sv.fuel--; sv.day++; sv.jumps++;
  sv.sys = (uint8_t)d; sv.planet = SKY_SYS[d].p0; sv.landed = 0;
  bool first = !(sv.visited >> d & 1);
  sv.visited |= 1ull << d;
  int32_t bonus = first ? 300 + 250 * SKY_SYS[d].depth : 0, survey = 0;
  for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type == 3 && sv.job[i].to == d) { survey += (int32_t)sv.job[i].reward; removeJob(i); sv.jobsDone++; }
  addCredits(bonus + survey);
  uint8_t next = PH_SPACE;
  bool bounty = false;
  for (int i = 0; i < MAXJOB; i++) if (sv.job[i].type == 4 && sv.job[i].to == d) bounty = true;
  bool qhunt = (chType() == 2 && chainTarget() == d);
  int depth = SKY_SYS[d].depth;
  if (first) logEv(4, bonus);
  if (bounty || qhunt) { setupFight(1, bounty ? depth + 1 : depth); next = PH_FIGHT; }
  else {
    int r = (int)(rnd() % 100), pc = 4 + 3 * depth;
    pc = pc * (100 - 25 * upBonus(5)) / 100;   // RADAR
    if (r < pc) { setupFight(0, pirateTier(depth)); next = PH_FIGHT; }
    else if (r < pc + 8) { eKind = 2; next = PH_EVENT; }
    else if (r < pc + 12) { eKind = 3; next = PH_EVENT; }
    else if (r < pc + 18 && freeSpace() >= 3) {
      eKind = 4; evA = (uint16_t)(rnd() % SKY_COMM); int f = freeSpace(); evB = (uint16_t)(3 + rnd() % 8); if (evB > f) evB = (uint16_t)f; next = PH_EVENT;
    }
    else if (r < pc + 30) { eKind = 5; evA = (uint16_t)(rnd() % 6); next = PH_EVENT; }
    else if (r < pc + 34) { eKind = 6; next = PH_EVENT; }
    else if (r < pc + 38 && cargoUsed() - jobSpace() > 0) {
      int c = (int)(rnd() % SKY_COMM);
      while (!sv.cargo[c]) c = (c + 1) % SKY_COMM;
      eKind = 7; evA = (uint16_t)c; evB = sv.cargo[c] > 12 ? 12 : sv.cargo[c]; next = PH_EVENT;
    }
    else if (r < pc + 41) { eKind = 8; evA = (uint16_t)(rnd() % SKY_COMM); next = PH_EVENT; }
  }
  if (bonus || survey) say(MG_ARRIVE, bonus, survey, next);
  else { uint8_t keep = eKind; startPhase(next); eKind = keep; if (next == PH_FIGHT) { m1 = m2 = 0; argB = 0; } }
}

inline void doScan() {
  int st = storyTarget();
  sv.day++;
  if (chType() == 0 && chainTarget() == sv.sys) { chainDone(); say(MG_CHAIN, -1, 0, PH_SPACE); return; }
  if (st == sv.sys) {
    int step = sv.story;
    sv.story++;
    logEv(5, step);
    int32_t reward = 0;
    if (sv.story == 7) { reward = 150000; addCredits(reward); }
    say(MG_TEXT, step, reward, PH_SPACE);
    return;
  }
  sv.scanned |= 1ull << sv.sys;
  int r = (int)(rnd() % 100);
  if (r < 35) say(MG_LOOT, -4, 0, PH_SPACE);
  else if (r < 65) { int32_t c = 300 + (int32_t)(rnd() % 1200); addCredits(c); say(MG_LOOT, -1, c, PH_SPACE); }
  else if (r < 90) {
    int f = freeSpace();
    if (f >= 2) { int c = (int)(rnd() % SKY_COMM), q = 2 + (int)(rnd() % 6); if (q > f) q = f; sv.cargo[c] += (uint16_t)q; say(MG_LOOT, c, q, PH_SPACE); }
    else { int32_t c = 500; addCredits(c); say(MG_LOOT, -1, c, PH_SPACE); }
  } else { int32_t c = 2000 + (int32_t)(rnd() % 4000); addCredits(c); say(MG_LOOT, -1, c, PH_SPACE); }
}

inline void doMine() {
  const SkySys &S = SKY_SYS[sv.sys];
  sv.day++;
  int c = (S.mine == 2 && (rnd() & 1)) ? 8 : 2;
  int q = 3 + (int)(rnd() % 4) + ship().gun + upBonus(0) / 2;
  int f = freeSpace();
  if (q > f) q = f;
  if (q <= 0) { say(MG_NO, 7, 0, PH_SPACE); return; }
  sv.cargo[c] += (uint16_t)q;
  if (rnd() % 100 < 10 + 3 * S.depth) { setupFight(0, pirateTier(S.depth)); say(MG_MINE, c, q, PH_FIGHT); }
  else say(MG_MINE, c, q, PH_SPACE);
}

inline void doBuy(int c, int n) {
  syncImp();
  int f = freeSpace();
  if (n > f) n = f;
  while (n > 0 && costBuy(c, n) > sv.credits) n--;
  if (n <= 0) { say(MG_NO, f <= 0 ? 7 : 1, 0, PH_TRADE); return; }
  { int32_t cst = (int32_t)costBuy(c, n); addCredits(-cst); X().trade -= cst; exDirty = true; }
  sv.cargo[c] += (uint16_t)n; sv.imp[c] += (int16_t)n;
}
inline void doSell(int c, int n) {
  syncImp();
  if (n > sv.cargo[c]) n = sv.cargo[c];
  if (n <= 0) return;
  { int32_t g = (int32_t)gainSell(c, n); addCredits(g); X().trade += g; exDirty = true; }
  sv.cargo[c] -= (uint16_t)n; sv.imp[c] -= (int16_t)n;
}

inline void acceptJob(int slot) {
  if (activeJobs() >= MAXJOB) { say(MG_NO, 6, 0, PH_JOBS); return; }
  Job j; genJob(slot, j);
  if ((j.type == 1 || j.type == 2) && j.qty > freeSpace()) { say(MG_NO, 5, 0, PH_JOBS); return; }
  for (int i = 0; i < MAXJOB; i++) if (!sv.job[i].type) { sv.job[i] = j; break; }
  if (sv.takenSys != sv.sys || sv.takenTick != boardTick()) { sv.taken = 0; sv.takenSys = sv.sys; sv.takenTick = (uint16_t)boardTick(); }
  sv.taken |= (uint8_t)(1 << slot);
  startPhase(PH_JOBS);
}

inline void buyUpgrade(int t) {
  if (t < 0 || t >= NUPT) return;
  if (lvl(t) >= UPMAX[t]) { say(MG_NO, 3, 0, PH_OUTFIT); return; }
  if (slotsUsed() >= ship().slots) { say(MG_NO, 2, 0, PH_OUTFIT); return; }
  uint32_t c = UPC[t][lvl(t)];
  if (sv.credits < (int32_t)c) { say(MG_NO, 1, 0, PH_OUTFIT); return; }
  addCredits(-(int32_t)c); setLvl(t, lvl(t) + 1);
  if (t == 3) sv.fuel = (uint8_t)fuelCap();
  logEv(7, t);
  say(MG_BOUGHT, 1, t * 10 + lvl(t), PH_OUTFIT);
}

inline void buyShip(int i) {
  int32_t ti; tradeIn(ti);
  int32_t net = (int32_t)SHIPS[i].cost - ti;
  if (sv.credits < net) { say(MG_NO, 1, 0, PH_YARD); return; }
  int newCap = SHIPS[i].cargo;
  if (cargoUsed() > newCap) { say(MG_NO, 4, 0, PH_YARD); return; }
  addCredits(-net);
  sv.ship = (uint8_t)i;
  memset(sv.up, 0, sizeof(sv.up));
  X().up2[0] = X().up2[1] = 0; exDirty = true;
  logEv(6, i);
  sv.fuel = (uint8_t)fuelCap(); sv.hull = (uint8_t)hullMax();
  say(MG_BOUGHT, 2, i, PH_PORT);
}

inline void act(uint16_t id) {
  const SkySys &S = SKY_SYS[sv.sys];
  switch (phase) {
    case PH_INTRO: startPhase(PH_PORT); break;
    case PH_SPACE:
      if (id == R_LAND) { if (S.np > 1) startPhase(PH_PICK); else land(S.p0); }
      else if (id == R_SCAN) doScan();
      else if (id == R_MINE) doMine();
      else if (id == R_MAP) { startPhase(PH_MAP); sel = nextHop(); }
      else if (id == R_ROUTE) startPhase(PH_ROUTE);
      else if (id == R_WAIT) {
        sv.day += 2; int g = sv.fuel < fuelCap() ? 1 : 0; sv.fuel += (uint8_t)g;
        say(MG_WAIT, 2, g, PH_SPACE);
      }
      else if (id == R_STATUS) startPhase(PH_STATUS);
      break;
    case PH_PORT:
      if (id == R_TRADE) startPhase(PH_TRADE);
      else if (id == R_JOBS) startPhase(PH_JOBS);
      else if (id == R_OUTFIT) startPhase(PH_OUTFIT);
      else if (id == R_YARD) startPhase(PH_YARD);
      else if (id == R_INFO) startPhase(PH_INFO);
      else if (id == R_STATUS) startPhase(PH_STATUS);
      else if (id == R_TAKEOFF) { sv.landed = 0; startPhase(PH_SPACE); }
      break;
    case PH_PICK:
      if (id == R_BACK) startPhase(PH_SPACE); else land(S.p0 + (id - R_PICK));
      break;
    case PH_JUMP:
      if (id == R_BACK) startPhase(PH_SPACE); else doJump(id - R_ITEM);
      break;
    case PH_ROUTE:
      if (id == R_CLEAR) { navT = 255; startPhase(PH_SPACE); }
      else { navT = (uint8_t)(id - R_ITEM); startPhase(PH_MAP); sel = nextHop(); }
      break;
    case PH_TRADE:
      if (id == R_BACK) startPhase(PH_PORT); else if (id == R_GUIDE) startPhase(PH_GUIDE); else startPhase(PH_TRADE2, id - R_ITEM);
      break;
    case PH_GUIDE:
      if (id == R_BACK) startPhase(PH_TRADE); else startPhase(PH_GUIDE2, id - R_ITEM);
      break;
    case PH_GUIDE2: startPhase(PH_GUIDE); break;
    case PH_REPORT: case PH_LOG: startPhase(PH_STATUS); break;
    case PH_TRADE2: {
      int c = argA;
      if (id == R_BACK) startPhase(PH_TRADE);
      else if (id == R_BUY1) doBuy(c, 1);
      else if (id == R_BUY10) doBuy(c, 10);
      else if (id == R_BUYMAX) doBuy(c, 9999);
      else if (id == R_SELL1) doSell(c, 1);
      else if (id == R_SELL10) doSell(c, 10);
      else if (id == R_SELLALL) doSell(c, 9999);
      break;
    }
    case PH_JOBS:
      if (id == R_BACK) startPhase(PH_PORT);
      else if (id == R_ACTIVE) startPhase(PH_ACTIVE);
      else startPhase(PH_JOB, id - R_ITEM);
      break;
    case PH_JOB:
      if (id == R_ACCEPT) acceptJob(argA); else startPhase(PH_JOBS);
      break;
    case PH_ACTIVE:
      if (id == R_BACK) startPhase(sv.landed ? PH_PORT : PH_SPACE); else startPhase(PH_ACT1, 2, id - R_ITEM);
      break;
    case PH_ACT1:
      if (id == R_CONT) { newGame(); }
      else if (id == R_ABANDON) { addRep(SKY_SYS[sv.job[argB].to].gov, -1); removeJob(argB); startPhase(PH_ACTIVE); }
      else startPhase(argA == 1 ? PH_STATUS : PH_ACTIVE);
      break;
    case PH_OUTFIT:
      if (id == R_BACK) startPhase(PH_PORT); else buyUpgrade(id - R_ITEM);
      break;
    case PH_YARD:
      if (id == R_BACK) startPhase(PH_PORT); else startPhase(PH_YARD2, id - R_ITEM);
      break;
    case PH_YARD2:
      if (id == R_BUYDEAL) buyShip(argA); else startPhase(PH_YARD);
      break;
    case PH_INFO:
      if (id == R_STORY) { sv.story = 1; say(MG_TEXT, 100, 0, PH_INFO); }
      else if (id == R_CHAIN) { int n = nextChain(); if (n >= 0) { X().ch = (uint8_t)(n + 1); X().cs = 0; exDirty = true; say(MG_CHAIN, 100 + n, 0, PH_INFO); } else startPhase(PH_PORT); }
      else startPhase(PH_PORT);
      break;
    case PH_STATUS:
      if (id == R_ACTIVE) startPhase(PH_ACTIVE);
      else if (id == R_REPORT) startPhase(PH_REPORT);
      else if (id == R_LOG) startPhase(PH_LOG);
      else if (id == R_HELP) startPhase(PH_ACT1, 1);
      else startPhase(sv.landed ? PH_PORT : PH_SPACE);
      break;
    case PH_EVENT:
      if (eKind == 2 && id == R_SEARCH) {
        int r = (int)(rnd() % 100);
        if (r < 55 && freeSpace() >= 2) { int c = (int)(rnd() % SKY_COMM), q = 3 + (int)(rnd() % 6), f = freeSpace(); if (q > f) q = f; sv.cargo[c] += (uint16_t)q; say(MG_LOOT, c, q, PH_SPACE); }
        else if (r < 85) { int32_t c = 800 + (int32_t)(rnd() % 2200); addCredits(c); say(MG_LOOT, -1, c, PH_SPACE); }
        else { sv.hull = (uint8_t)(sv.hull > 2 ? sv.hull - 2 : 1); say(MG_LOOT, -2, 0, PH_SPACE); }
      } else if (eKind == 3 && id == R_SEARCH) {
        sv.fuel -= 1; int32_t c = 1500 + (int32_t)(rnd() % 2500); addCredits(c); say(MG_LOOT, -3, c, PH_SPACE);
      } else if (eKind == 4 && id == R_BUYDEAL) {
        int p = SKY_SYS[sv.sys].price[evA] * 60 / 100, q = evB;
        if ((int64_t)p * q <= sv.credits && q <= freeSpace()) { addCredits(-(int32_t)(p * q)); sv.cargo[evA] += (uint16_t)q; say(MG_BOUGHT, 3, q * 100 + evA, PH_SPACE); }
        else startPhase(PH_SPACE);
      } else if (eKind == 6 && id == R_PAYTAX) {
        int t = patrolTax(); if (t > sv.credits) t = sv.credits;
        addCredits(-t); addRep(S.gov, 1); say(MG_TAX, t, 0, PH_SPACE);
      } else if (eKind == 6 && id == R_RUN) {
        addRep(S.gov, -3);
        if (rnd() & 1) say(MG_LOOT, -5, 0, PH_SPACE);
        else { int t = patrolTax() * 2; if (t > sv.credits) t = sv.credits; addCredits(-t); say(MG_TAX, t, 1, PH_SPACE); }
      } else if (eKind == 7 && id == R_SELLC) {
        int c = evA, q = evB; if (q > sv.cargo[c]) q = sv.cargo[c];
        evB = (uint16_t)q;
        int32_t tot = q * S.price[c] * 130 / 100;
        sv.cargo[c] -= (uint16_t)q; addCredits(tot); X().trade += tot; exDirty = true; logEv(10, tot);
        say(MG_SOLD, c, tot, PH_SPACE);
      } else startPhase(PH_SPACE);
      break;
    case PH_FIGHT: fightAction(id); break;
    case PH_MSG: {
      bool dfl = (mk == MG_DEFEAT);
      startPhase((uint8_t)argA);
      if (phase == PH_FIGHT) { m1 = m2 = 0; argB = 0; }
      if (phase == PH_MAP) sel = nextHop();
      if (dfl && phase == PH_PORT) land(sv.planet);   // rescued: deliver passengers/cargo bound for this planet
      break;
    }
  }
}

// ---------------------------------------------------------------- framework hooks
inline void start() {
  loadOrNew();
  seedRng();
  if (phase != PH_INTRO) startPhase(sv.landed ? PH_PORT : PH_SPACE);
  else startPhase(PH_INTRO);
}
inline bool busy() { return true; }   // leaving the app always asks first
// BACK inside the app: go up one level. false = already at the top (leave).
inline bool back() {
  switch (phase) {
    case PH_PICK: case PH_JUMP: case PH_MAP: case PH_ROUTE: startPhase(PH_SPACE); return true;
    case PH_TRADE: case PH_JOBS: case PH_OUTFIT: case PH_YARD: case PH_INFO: startPhase(PH_PORT); return true;
    case PH_TRADE2: startPhase(PH_TRADE); return true;
    case PH_GUIDE: startPhase(PH_TRADE); return true;
    case PH_GUIDE2: startPhase(PH_GUIDE); return true;
    case PH_REPORT: case PH_LOG: startPhase(PH_STATUS); return true;
    case PH_JOB: startPhase(PH_JOBS); return true;
    case PH_YARD2: startPhase(PH_YARD); return true;
    case PH_ACTIVE: case PH_STATUS: startPhase(sv.landed ? PH_PORT : PH_SPACE); return true;
    case PH_ACT1: startPhase(argA == 1 ? PH_STATUS : PH_ACTIVE); return true;
    default: return false;
  }
}

inline void mapJump() {
  if (sv.fuel == 0) return;
  doJump(sel);
  if (navT == sv.sys) navT = 255;
  if (phase == PH_MSG && argA == PH_SPACE) argA = PH_MAP;
  else if (phase == PH_SPACE) { startPhase(PH_MAP); sel = nextHop(); }
}
inline void up() {
  if (phase == PH_MAP) { int n = SKY_SYS[sv.sys].nLinks; sel = (uint8_t)((sel + n - 1) % n); return; }
  build();
  if (page > 0 && page < pages - 1) { page--; return; }
  if (nRows == 0) return;
  int s = qwGameSteps();
  if (sel >= s) sel -= (uint8_t)s; else sel = (s == 1) ? (uint8_t)(nRows - 1) : 0;
}
inline void down() {
  if (phase == PH_MAP) { sel = (uint8_t)((sel + 1) % SKY_SYS[sv.sys].nLinks); return; }
  build();
  if (page < pages - 1) { page++; return; }
  if (nRows == 0) return;
  int s = qwGameSteps();
  if (sel + s < nRows) sel += (uint8_t)s; else sel = (s == 1 && sel == nRows - 1) ? 0 : (uint8_t)(nRows - 1);
}
inline void select() {
  if (phase == PH_MAP) { mapJump(); save(); return; }
  build();
  if (page < pages - 1) { page++; return; }
  if (nRows == 0) return;
  uint16_t id = rows[sel].id;
  uint8_t before = phase;
  act(id);
  (void)before;
  if (phase != PH_FIGHT && phase != PH_EVENT) save(); else if (exDirty) saveX();
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d) { d.setTextSize(1); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
inline void txtN(Adafruit_GFX &d, const char *s, int n, int16_t x, int16_t base) {
  char b[COLS + 2];
  if (n > COLS) n = COLS;
  memcpy(b, s, n); b[n] = 0;
  d.setCursor(x, base); d.print(b);
}
inline void drawMap(Adafruit_GFX &d) {
  const SkySys &S = SKY_SYS[sv.sys];
  const int CX = 100, CY = 89, SC = 3;
  int8_t dist[SKY_N]; int8_t pn[SKY_N];
  bool nav = navOn();
  for (int i = 0; i < SKY_N; i++) pn[i] = -1;
  if (nav) {
    bfs(navT, dist);
    int u = sv.sys;
    while (u != navT) {
      int nx = -1;
      for (int k = 0; k < SKY_SYS[u].nLinks; k++) if (dist[SKY_SYS[u].links[k]] == dist[u] - 1) { nx = SKY_SYS[u].links[k]; break; }
      if (nx < 0) break;
      pn[u] = (int8_t)nx; u = nx;
    }
  }
  int X[SKY_N], Y[SKY_N];
  for (int i = 0; i < SKY_N; i++) { X[i] = CX + (SKY_XY[2 * i] - SKY_XY[2 * sv.sys]) * SC; Y[i] = CY + (SKY_XY[2 * i + 1] - SKY_XY[2 * sv.sys + 1]) * SC; }
  auto inView = [&](int i) { return X[i] >= 4 && X[i] <= 195 && Y[i] >= 22 && Y[i] <= 156; };
  uint16_t ink = qwInk(), pap = qwPaper();
  for (int i = 0; i < SKY_N; i++) for (int k = 0; k < SKY_SYS[i].nLinks; k++) {
    int j = SKY_SYS[i].links[k];
    if (j < i || (!inView(i) && !inView(j))) continue;
    d.drawLine(X[i], Y[i], X[j], Y[j], ink);
    if (pn[i] == j || pn[j] == i) { d.drawLine(X[i] + 1, Y[i], X[j] + 1, Y[j], ink); d.drawLine(X[i], Y[i] + 1, X[j], Y[j] + 1, ink); }
  }
  int st = storyTarget();
  for (int i = 0; i < SKY_N; i++) {
    if (i == sv.sys || !inView(i)) continue;
    d.fillCircle(X[i], Y[i], 3, pap);
    if (sv.visited >> i & 1) d.fillCircle(X[i], Y[i], 3, ink); else d.drawCircle(X[i], Y[i], 3, ink);
    if (isJobTarget(i)) d.drawRect(X[i] - 5, Y[i] - 5, 11, 11, ink);
    if (i == st) d.drawCircle(X[i], Y[i], 6, ink);
    if (nav && i == navT) d.fillTriangle(X[i], Y[i] - 6, X[i] - 4, Y[i] - 13, X[i] + 4, Y[i] - 13, ink);
  }
  d.fillCircle(X[sv.sys], Y[sv.sys], 6, pap);
  d.drawCircle(X[sv.sys], Y[sv.sys], 6, ink);
  d.fillCircle(X[sv.sys], Y[sv.sys], 3, ink);
  d.setFont(nullptr); d.setTextSize(1);
  int ns = S.nLinks ? sel % S.nLinks : 0;
  for (int k = 0; k < S.nLinks; k++) {
    int j = S.links[k];
    if (!inView(j)) continue;
    char b[6]; memcpy(b, SKY_SYS[j].name, 5); b[5] = 0;
    int w = 6 * (int)strlen(b), lx = X[j] + 7, ly = Y[j] - 3;
    if (lx + w > 198) lx = X[j] - 7 - w;
    if (k == ns) { d.fillRect(lx - 1, ly - 1, w + 2, 9, ink); d.setTextColor(pap); d.drawCircle(X[j], Y[j], 5, ink); }
    else { d.fillRect(lx - 1, ly - 1, w + 2, 9, pap); d.setTextColor(ink); }
    d.setCursor(lx, ly); d.print(b);
  }
  d.fillRect(0, 158, 200, 42, pap);
  d.drawFastHLine(4, 160, 192, ink);
  setup13(d);
  char l1[28], l2[28];
  int j = S.links[ns];
  const char *mk2 = (j == st) ? "SIG" : isJobTarget(j) ? "JOB" : (sv.visited >> j & 1) ? "" : "NEW";
  snprintf(l1, sizeof(l1), "> %-9.9s %-3s", SKY_SYS[j].name, mk2);
  if (sv.fuel == 0) snprintf(l2, sizeof(l2), "NO FUEL, WAIT IN SPACE");
  else if (nav && dist[sv.sys] > sv.fuel) {   // route longer than the fuel: advise a port on the way
    int u = sv.sys, h = 0, bp = -1, bh = 0;
    while (u != navT && pn[u] >= 0) { u = pn[u]; h++; if (h > sv.fuel) break; if (SKY_SYS[u].np) { bp = u; bh = h; } }
    if (bp >= 0) snprintf(l2, sizeof(l2), "F%d REFUEL %.9s %dj", sv.fuel, SKY_SYS[bp].name, bh);
    else if (S.np) snprintf(l2, sizeof(l2), "F%d LAND TO REFUEL", sv.fuel);
    else snprintf(l2, sizeof(l2), "F%d NO PORT IN REACH", sv.fuel);
  }
  else if (nav) snprintf(l2, sizeof(l2), "F%d NAV %.8s %dj", sv.fuel, SKY_SYS[navT].name, dist[sv.sys]);
  else snprintf(l2, sizeof(l2), "FUEL %d/%d  MENU=JUMP", sv.fuel, fuelCap());
  d.setCursor(6, 176); d.print(l1);
  d.setCursor(6, 194); d.print(l2);
}

inline void draw(Adafruit_GFX &d) {
  build();
  d.fillScreen(qwPaper());
  setup13(d);
  if (phase == PH_MAP) { drawMap(d); d.fillRect(0, 0, 200, 17, qwPaper()); setup13(d); }
  char r[12], h[16];
  fmt(r, sv.credits);
  snprintf(h, sizeof(h), "CR %s", r);
  { int room = 23 - (int)strlen(h); int n = (int)strlen(ttl); if (n > room) n = room; txtN(d, ttl, n, 6, 12); }
  d.setCursor(194 - 8 * (int)strlen(h), 12); d.print(h);
  d.drawFastHLine(4, 15, 192, qwInk());
  if (phase == PH_MAP) { d.setFont(nullptr); return; }
  int first = page * cap;
  for (int k = 0; k < cap && first + k < nLines; k++) txtN(d, body + lines[first + k].o, lines[first + k].n, 6, 30 + 15 * k);
  int top = 198 - vr * ROWH;
  for (int k = 0; k < vr; k++) {
    int i = scroll + k;
    if (i >= nRows) break;
    int y = top + k * ROWH;
    bool inv = (i == sel);
    if (inv) d.fillRect(4, y, 192, ROWH, qwInk());
    d.setTextColor(inv ? qwPaper() : qwInk());
    txtN(d, rows[i].t, 24, 8, y + 13);
    d.setTextColor(qwInk());
  }
  if (scroll > 0) { d.setCursor(188, top + 9); d.print("^"); }
  if (scroll + vr < nRows) { d.setCursor(188, 197); d.print("v"); }
  d.setFont(nullptr);
}

}  // namespace Sk
