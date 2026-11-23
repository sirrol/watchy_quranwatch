#pragma once
/*
  Hifz.h
  ------------------------------------------------------------
  HIFZ TRACKER - an app of the APPS menu to follow the memorisation of the
  Quran: progress marks, repetition counter, planned reviews, error journal.

  Every ayah has one of five marks: not set, TO LEARN, LEARNING, MEMORIZED,
  TO REVIEW (3 bits each, about 2.3 KB for the 6236 ayat, saved in flash).
  The order of memorisation is free: you choose any portion (surah, first
  ayah, last ayah) and mark it.

  PATH (chosen at first launch, changeable in SETTINGS): SIMPLE = the
  reviews due today; METHOD = TODAY'S PLAN with SABAK (new portion from your
  position, goal in ayat / lines / pages), SABQI (recent) and MANZIL (old,
  a rotation over a cycle of days); FREE = tracking only, no plan.
  Marking a portion MEMORIZED offers a review plan: EASY / NORMAL / HARD
  sets the spacing (1,2,4,7,14,30,60 days). A review result moves the next
  date: well = later, hesitated = same step, forgot = two steps back (and
  the ayat go to TO REVIEW). Every date can be edited by hand.
  ERROR JOURNAL: confused ayat, hard starts, link errors. You tick "to
  check with a teacher" and "corrected" yourself; there is no score.

  The streak (consecutive days with saved repetitions or review results)
  is shown inside this app only. Nothing here is a score or an evaluation:
  you are the one who sets every mark. Plain ASCII; the surah names are
  the Arabic images of SurahNames.h. Lines and pages are estimates (the
  ayat are spread evenly over the pages of a surah, 15 lines per page).

  Flash blobs: "hifzB" (marks), "hifzM" (log, streak), "hifzP" (settings and
  planned passages, 60 max), "hifzE" (error journal, 40 max).
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "Storage.h"
#include "NotoMono13.h"
#include "QuranData.h"
#include "SurahNames.h"
#include "SacredMonth.h"   // Sac:: alarm settings (shown on the watch face)

namespace Hz {

constexpr int NS = 114, NV = 6236, LOGN = 30, BN = (NV * 3 + 7) / 8 + 2, PERP = 7, MAXP = 60, MAXE = 40;
constexpr uint16_t MAGIC = 0x4858, MAGICP = 0x4850;
enum Mark : uint8_t { M_NONE = 0, M_LEARN, M_ING, M_MEM, M_REV };
static const char *const MN[5] = {"NOT SET", "TO LEARN", "LEARNING", "MEMORIZED", "TO REVIEW"};
static const uint16_t GOALS[5] = {0, 10, 20, 40, 100};
static const char *const MON[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
static const uint16_t LADDER[7] = {1, 2, 4, 7, 14, 30, 60};
static const char *const PATHN[3] = {"SIMPLE", "METHOD", "FREE"};
static const char *const UNITN[3] = {"AYAT", "LINES", "PAGES"};
static const char *const DIFFN[3] = {"EASY", "NORMAL", "HARD"};
static const uint8_t UCNT[3] = {8, 7, 4};
static const uint8_t UVAL[3][8] = {{1, 2, 3, 5, 7, 10, 15, 20}, {3, 5, 7, 10, 15, 20, 30, 0}, {1, 2, 3, 5, 0, 0, 0, 0}};
static const uint8_t SQD[4] = {3, 5, 7, 14};
static const uint8_t MCY[4] = {7, 15, 30, 60};
static const char *const ETN[3] = {"CONF", "START", "LINK"};

struct Log { uint16_t day; uint8_t s; uint16_t a, b, reps; };
struct Meta {
  uint16_t magic, lastDay, streak, best;
  uint8_t lastS, nLog;
  uint16_t lastA;
  uint32_t repsTotal;
  Log log[LOGN];
};
struct Pass { uint8_t s, diff, step, pad; uint16_t a, b, learned, next, lastRev; };
struct Plan { uint16_t magic; uint8_t chosen, path, unit, goalI, sabqiI, manzilI, nP, pS; uint16_t pA; Pass p[MAXP]; };
struct Err { uint8_t s, type, flags, s2; uint16_t a, a2, day; };
struct ErrS { uint8_t n; Err e[MAXE]; };

enum Ph : uint8_t {
  PH_MENU = 0, PH_RNG, PH_MARK, PH_PLAN, PH_CNT, PH_SAVE, PH_PROG, PH_SLIST, PH_SDET, PH_LOG, PH_DONE,
  PH_PATH, PH_TODAY, PH_PLIST, PH_PDET, PH_RES, PH_EDATE, PH_SET, PH_EL, PH_ETYPE, PH_EDET, PH_CONF, PH_ALARM
};
enum Id : uint8_t { ID_TODAY = 0, ID_MARK, ID_REP, ID_PASS, ID_PROG, ID_SLIST, ID_LOG, ID_ERR, ID_SET };
static const char *const IDN[9] = {"TODAY'S PLAN", "MARK PASSAGE", "REPEAT COUNTER", "PASSAGES", "PROGRESS", "SURAH LIST", "REPEAT LOG", "ERROR JOURNAL", "SETTINGS"};

RTC_DATA_ATTR uint8_t phase = PH_MENU, sel = 0, purpose = 0, fld = 0, rs = 1, mSel = 0, goalIdx = 0, rngBack = PH_MENU, dKind = 0, dMark = 0, sdS = 1;
RTC_DATA_ATTR uint8_t kind = 0, curP = 255, plMode = 0, plBack = PH_MENU, pSel = 0, eSel = 0, eType = 0, eS = 1, confKind = 0;
RTC_DATA_ATTR uint8_t edit = 0, dayCur = 0;
RTC_DATA_ATTR uint16_t rf = 1, rt = 1, cnt = 0, dN = 0, eA = 1, tmpV = 0, dInfo = 0;

static Meta mt;
static Plan pl;
static ErrS er;
static uint8_t bits[BN];
static uint16_t off[NS + 2];
static uint8_t ord[MAXP];
static bool ld = false;

inline void saveMeta() { QWStorage::saveBlob("hifzM", &mt, sizeof(mt)); }
inline void saveBits() { QWStorage::saveBlob("hifzB", bits, sizeof(bits)); }
inline void savePlan() { QWStorage::saveBlob("hifzP", &pl, sizeof(pl)); }
inline void saveErr() { QWStorage::saveBlob("hifzE", &er, sizeof(er)); }
inline void ensure() {
  if (ld) return;
  off[1] = 0;
  for (int s = 1; s <= NS; s++) off[s + 1] = (uint16_t)(off[s] + QURAN_VERSE_COUNT[s - 1]);
  Meta t;
  memset(&t, 0, sizeof(t));
  memset(&mt, 0, sizeof(mt));
  if (QWStorage::loadBlob("hifzM", &t, sizeof(t)) && t.magic == MAGIC && t.nLog <= LOGN) mt = t;
  else { mt.magic = MAGIC; mt.lastS = 1; mt.lastA = 1; }
  memset(bits, 0, sizeof(bits));
  QWStorage::loadBlob("hifzB", bits, sizeof(bits));
  Plan p;
  memset(&p, 0, sizeof(p));
  memset(&pl, 0, sizeof(pl));
  if (QWStorage::loadBlob("hifzP", &p, sizeof(p)) && p.magic == MAGICP && p.nP <= MAXP) pl = p;
  else { pl.magic = MAGICP; pl.goalI = 3; pl.sabqiI = 2; pl.manzilI = 2; pl.pS = 1; pl.pA = 1; }
  ErrS e;
  memset(&e, 0, sizeof(e));
  memset(&er, 0, sizeof(er));
  if (QWStorage::loadBlob("hifzE", &e, sizeof(e)) && e.n <= MAXE) er = e;
  ld = true;
}

inline uint32_t today() { return qwGameDay ? qwGameDay : 1; }
inline uint16_t gi(int s, int a) { return (uint16_t)(off[s] + a - 1); }
inline uint8_t getM(uint16_t i) {
  uint32_t p = (uint32_t)i * 3;
  uint16_t w = (uint16_t)(bits[p >> 3] | (bits[(p >> 3) + 1] << 8));
  return (uint8_t)((w >> (p & 7)) & 7);
}
inline void setM(uint16_t i, uint8_t v) {
  uint32_t p = (uint32_t)i * 3;
  int q = (int)(p >> 3), sh = (int)(p & 7);
  uint16_t w = (uint16_t)(bits[q] | (bits[q + 1] << 8));
  w = (uint16_t)((w & ~(7u << sh)) | ((uint16_t)v << sh));
  bits[q] = (uint8_t)(w & 255); bits[q + 1] = (uint8_t)(w >> 8);
}
inline void counts(int s, uint16_t *c) {   // c[0..4]
  for (int k = 0; k < 5; k++) c[k] = 0;
  int n = versesInSurah((uint8_t)s);
  for (int a = 1; a <= n; a++) { uint8_t m = getM(gi(s, a)); c[m > 4 ? 0 : m]++; }
}

inline void touchDay(uint32_t day) {
  if (!day) return;
  if (mt.lastDay == day) return;
  if (mt.lastDay && day == (uint32_t)mt.lastDay + 1) mt.streak++; else mt.streak = 1;
  mt.lastDay = (uint16_t)day;
  if (mt.streak > mt.best) mt.best = mt.streak;
}
inline int streakNow() {
  if (!qwGameDay) return mt.streak;
  if (!mt.lastDay) return 0;
  return qwGameDay <= (uint32_t)mt.lastDay + 1 ? mt.streak : 0;
}
inline unsigned repsToday() {
  unsigned t = 0;
  if (qwGameDay) for (int i = 0; i < mt.nLog; i++) if (mt.log[i].day == qwGameDay) t += mt.log[i].reps;
  return t;
}
inline void dateText(char *b, size_t n, uint32_t z) {
  int32_t zz = (int32_t)z + 719468;
  int32_t era = zz / 146097, doe = zz - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int32_t mp = (5 * doy + 2) / 153, dd = doy - (153 * mp + 2) / 5 + 1, m = mp < 10 ? mp + 3 : mp - 9;
  snprintf(b, n, "%s %2d", MON[(m - 1) % 12], (int)dd);
}

// ---------------------------------------------------------------- plan helpers
inline int ivl(int step, int diff) {
  int v = LADDER[step > 6 ? 6 : step] * (diff == 0 ? 13 : (diff == 1 ? 10 : 7)) / 10;
  return v < 1 ? 1 : v;
}
inline int findPass(int s, int a, int b) {
  for (int i = 0; i < pl.nP; i++) if (pl.p[i].s == s && pl.p[i].a == a && pl.p[i].b == b) return i;
  return -1;
}
// 1 = planned, 0 = plan full
inline bool addPass(int diff) {
  int i = findPass(rs, rf, rt);
  if (i < 0) {
    if (pl.nP >= MAXP) return false;
    i = pl.nP++;
    memset(&pl.p[i], 0, sizeof(Pass));
    pl.p[i].s = rs; pl.p[i].a = rf; pl.p[i].b = rt;
    pl.p[i].learned = (uint16_t)today();
  }
  Pass &p = pl.p[i];
  p.diff = (uint8_t)diff;
  p.next = (uint16_t)(today() + ivl(p.step, diff));
  dInfo = (uint16_t)ivl(p.step, diff);
  savePlan();
  return true;
}
inline void delPass(int i) {
  for (int k = i; k + 1 < pl.nP; k++) pl.p[k] = pl.p[k + 1];
  if (pl.nP) { pl.nP--; memset(&pl.p[pl.nP], 0, sizeof(Pass)); }
  savePlan();
}
// mode 0 due, 1 sabqi (recent), 2 manzil (old rotation), 3 all. Fills ord[], returns the count.
inline int buildList(int mode) {
  uint32_t t = today(), key[MAXP];
  int sd = SQD[pl.sabqiI], oldAll = 0, oldDone = 0, n = 0;
  for (int i = 0; i < pl.nP; i++) {
    const Pass &p = pl.p[i];
    if (!(t > p.learned && t - p.learned >= (uint32_t)sd)) continue;
    oldAll++;
    if (p.lastRev == t) oldDone++;
  }
  int K = (oldAll + MCY[pl.manzilI] - 1) / MCY[pl.manzilI] - oldDone;
  for (int i = 0; i < pl.nP; i++) {
    const Pass &p = pl.p[i];
    bool recent = !(t > p.learned && t - p.learned >= (uint32_t)sd);
    uint32_t k = p.next;
    bool in = false;
    if (mode == 0) in = p.next <= t;
    else if (mode == 1) { in = recent && p.lastRev < t; k = 65535u - p.learned; }
    else if (mode == 2) { in = !recent && p.lastRev < t; k = p.lastRev; }
    else in = true;
    if (!in) continue;
    int j = n++;
    while (j > 0 && key[j - 1] > k) { key[j] = key[j - 1]; ord[j] = ord[j - 1]; j--; }
    key[j] = k; ord[j] = (uint8_t)i;
  }
  if (mode == 2 && n > K) n = K < 0 ? 0 : K;
  return n;
}
// next sabak portion from the saved position, sized by the goal
inline void sabakRange(int &s, int &a, int &b) {
  s = pl.pS < 1 || pl.pS > NS ? 1 : pl.pS; a = pl.pA < 1 ? 1 : pl.pA;
  if (a > versesInSurah((uint8_t)s)) { s = s % NS + 1; a = 1; }
  int vs = versesInSurah((uint8_t)s), amount = UVAL[pl.unit > 2 ? 0 : pl.unit][pl.goalI];
  int st = QURAN_SURAH_START_PAGE[s - 1], nx = s < NS ? QURAN_SURAH_START_PAGE[s] : 605, span = nx - st;
  if (span < 1) span = 1;
  int n = amount;
  if (pl.unit == 1) n = (amount * vs + span * 15 - 1) / (span * 15);
  else if (pl.unit == 2) n = (amount * vs + span - 1) / span;
  if (n < 1) n = 1;
  b = a + n - 1;
  if (b > vs) b = vs;
}

inline void saveReps() {
  touchDay(qwGameDay);
  int f = -1;
  for (int i = 0; i < mt.nLog; i++) if (mt.log[i].day == qwGameDay && mt.log[i].s == rs && mt.log[i].a == rf && mt.log[i].b == rt) f = i;
  if (f >= 0) {
    uint32_t v = (uint32_t)mt.log[f].reps + cnt;
    mt.log[f].reps = (uint16_t)(v > 60000 ? 60000 : v);
    Log e = mt.log[f];   // newest entry goes last
    for (int i = f; i + 1 < mt.nLog; i++) mt.log[i] = mt.log[i + 1];
    mt.log[mt.nLog - 1] = e;
  } else {
    if (mt.nLog >= LOGN) { for (int i = 0; i + 1 < LOGN; i++) mt.log[i] = mt.log[i + 1]; mt.nLog = LOGN - 1; }
    Log &e = mt.log[mt.nLog++];
    e.day = (uint16_t)qwGameDay; e.s = rs; e.a = rf; e.b = rt; e.reps = cnt;
  }
  mt.repsTotal += cnt;
  mt.lastS = rs; mt.lastA = rf;
  saveMeta();
  if (kind == 1) { pl.pS = rs; pl.pA = (uint16_t)(rt + 1); savePlan(); }
  else if (kind == 2 && curP < pl.nP) { pl.p[curP].lastRev = (uint16_t)today(); savePlan(); }
}
inline void applyResult(int r) {
  if (pSel >= pl.nP) return;
  Pass &p = pl.p[pSel];
  uint32_t t = today();
  if (r == 0) { if (p.step < 6) p.step++; }
  else if (r == 2) p.step = (uint8_t)(p.step >= 2 ? p.step - 2 : 0);
  p.next = (uint16_t)(t + ivl(p.step, p.diff));
  p.lastRev = (uint16_t)t;
  bool ch = false;
  for (int a = p.a; a <= p.b; a++) {
    uint16_t i = gi(p.s, a);
    uint8_t m = getM(i);
    if (r == 2 && m == M_MEM) { setM(i, M_REV); ch = true; }
    else if (r == 0 && m == M_REV) { setM(i, M_MEM); ch = true; }
  }
  if (ch) saveBits();
  touchDay(qwGameDay);
  savePlan(); saveMeta();
}
inline void addErr() {
  if (er.n >= MAXE) { for (int i = 0; i + 1 < MAXE; i++) er.e[i] = er.e[i + 1]; er.n = MAXE - 1; }
  Err &e = er.e[er.n++];
  memset(&e, 0, sizeof(e));
  e.s = eS; e.a = eA; e.type = eType; e.day = (uint16_t)today();
  if (eType == 0) { e.s2 = rs; e.a2 = rf; }
  saveErr();
}

// ---------------------------------------------------------------- navigation
inline int menuIds(uint8_t *ids) {
  int n = 0;
  for (int i = 0; i < 9; i++) if (i != ID_TODAY || pl.path < 2) ids[n++] = (uint8_t)i;
  return n;
}
inline void toMenu(int id) {
  uint8_t ids[9];
  int n = menuIds(ids);
  phase = PH_MENU; sel = 0;
  for (int i = 0; i < n; i++) if (ids[i] == id) sel = (uint8_t)i;
}
inline void start() {
  ensure();
  cnt = 0; kind = 0; curP = 255;
  if (!pl.chosen) { phase = PH_PATH; mSel = 1; } else { phase = PH_MENU; sel = 0; }
}
inline bool busy() { return true; }   // leaving the app always asks first
inline void openRange(uint8_t p, int s, int from, int to = 0) {
  purpose = p; fld = 0;
  rs = (uint8_t)(s ? s : mt.lastS); if (rs < 1 || rs > NS) rs = 1;
  int n = versesInSurah(rs);
  rf = (uint16_t)(from > 0 && from <= n ? from : 1);
  rt = (uint16_t)(to >= rf && to <= n ? to : (p >= 2 ? rf : n));
  phase = PH_RNG;
}
inline bool back() {
  ensure();
  switch (phase) {
    case PH_RNG:
      if (purpose == 3) phase = PH_ETYPE;
      else phase = (uint8_t)rngBack;
      return true;
    case PH_MARK: phase = PH_RNG; fld = 2; return true;
    case PH_PLAN: dKind = 0; phase = PH_DONE; return true;
    case PH_CNT:
      if (cnt > 0) { phase = PH_SAVE; mSel = 0; }
      else if (kind == 2) phase = PH_PDET;
      else { phase = PH_RNG; fld = 2; }
      return true;
    case PH_SAVE: phase = PH_CNT; return true;
    case PH_PROG: toMenu(ID_PROG); return true;
    case PH_SLIST: toMenu(ID_SLIST); return true;
    case PH_LOG: toMenu(ID_LOG); return true;
    case PH_SDET: phase = PH_SLIST; sel = (uint8_t)(sdS - 1); return true;
    case PH_DONE: if (kind == 2) { phase = PH_PLIST; sel = 0; } else toMenu(kind == 0 ? ID_MARK : ID_REP); return true;
    case PH_TODAY: toMenu(ID_TODAY); return true;
    case PH_PLIST: if (plBack == PH_TODAY) { phase = PH_TODAY; mSel = 0; } else toMenu(plMode == 3 ? ID_PASS : ID_TODAY); return true;
    case PH_PDET: phase = PH_PLIST; return true;
    case PH_RES: case PH_EDATE: phase = PH_PDET; mSel = 0; return true;
    case PH_SET: toMenu(ID_SET); return true;
    case PH_ALARM: if (edit) edit = 0; else { phase = PH_SET; mSel = 6; } return true;
    case PH_EL: toMenu(ID_ERR); return true;
    case PH_ETYPE: phase = PH_EL; return true;
    case PH_EDET: phase = PH_EL; return true;
    case PH_CONF: phase = confKind == 0 ? PH_PDET : PH_EDET; mSel = 0; return true;
    default: return false;
  }
}

inline int mul() { int s = qwGameSteps(); return s == 1 ? 1 : (s == 3 ? 5 : 20); }
inline void rngChange(int dir) {
  int n = versesInSurah(rs);
  if (fld == 0) {
    int v = rs + dir * mul();
    while (v < 1) v += NS;
    while (v > NS) v -= NS;
    rs = (uint8_t)v; rf = 1; rt = purpose >= 2 ? 1 : versesInSurah(rs);
  } else if (fld == 1) {
    int v = rf + dir * mul();
    if (v < 1) v = 1;
    if (v > n) v = n;
    rf = (uint16_t)v;
    if (purpose >= 2 || rt < rf) rt = rf;
  } else {
    int v = rt + dir * mul();
    if (v < rf) v = rf;
    if (v > n) v = n;
    rt = (uint16_t)v;
  }
}

inline void listMove(int dir, int total) {
  if (total <= 0) { sel = 0; return; }
  int s = mul();   // tap 1, short hold 5, long hold 20
  int v = sel + dir * s;
  if (v < 0) v = (s == 1 && sel == 0) ? total - 1 : 0;
  else if (v >= total) v = (s == 1 && sel == total - 1) ? 0 : total - 1;
  sel = (uint8_t)v;
}
// tap = next/previous (wraps); hold = jump to the first / last entry
inline void barMove(int dir, int n) {
  if (qwGameSteps() > 1) { mSel = (uint8_t)(dir < 0 ? 0 : n - 1); return; }
  mSel = (uint8_t)((mSel + n + dir) % n);
}
inline int listCount() {
  if (phase == PH_PLIST) return buildList(plMode);
  if (phase == PH_EL) return 1 + er.n;
  if (phase == PH_LOG) return mt.nLog;
  if (phase == PH_SLIST) return NS;
  return 0;
}

// ALARM screen: rows 0 on/off, 1 hour, 2 minute, 3 days, 4 vibrate. UP/DOWN move between the rows,
// or change the value while a row is being edited (MENU starts and ends the edit).
inline void alarmMove(int dir) {
  if (!edit) { barMove(dir, 5); return; }
  if (mSel == 1) { int v = (int)Sac::alH - dir * qwGameSteps(); while (v < 0) v += 24; Sac::alH = (uint8_t)(v % 24); Sac::saveAlarm(); }
  else if (mSel == 2) { int v = (int)Sac::alM - dir * mul(); while (v < 0) v += 60; Sac::alM = (uint8_t)(v % 60); Sac::saveAlarm(); }
  else if (mSel == 3) dayCur = (uint8_t)((dayCur + 7 + dir) % 7);
}
inline void up() {
  ensure();
  switch (phase) {
    case PH_MENU: { uint8_t ids[9]; int n = menuIds(ids); sel = (uint8_t)(qwGameSteps() > 1 ? 0 : (sel + n - 1) % n); break; }
    case PH_RNG: rngChange(+1); break;
    case PH_MARK: barMove(-1, 5); break;
    case PH_PLAN: barMove(-1, 4); break;
    case PH_CNT: goalIdx = (uint8_t)((goalIdx + 1) % 5); break;
    case PH_SAVE: case PH_CONF: mSel ^= 1; break;
    case PH_PATH: barMove(-1, 3); break;
    case PH_TODAY: barMove(-1, 4); break;
    case PH_PDET: barMove(-1, 6); break;
    case PH_RES: barMove(-1, 3); break;
    case PH_SET: barMove(-1, 7); break;
    case PH_ALARM: alarmMove(-1); break;
    case PH_ETYPE: barMove(-1, 3); break;
    case PH_EDET: barMove(-1, 4); break;
    case PH_EDATE: { uint32_t s = qwGameSteps(); tmpV = (uint16_t)(tmpV + s > 65000 ? 65000 : tmpV + s); break; }
    case PH_SLIST: case PH_LOG: case PH_PLIST: case PH_EL: listMove(-1, listCount()); break;
    default: break;
  }
}
inline void down() {
  ensure();
  switch (phase) {
    case PH_MENU: { uint8_t ids[9]; int n = menuIds(ids); sel = (uint8_t)(qwGameSteps() > 1 ? n - 1 : (sel + 1) % n); break; }
    case PH_RNG: rngChange(-1); break;
    case PH_MARK: barMove(+1, 5); break;
    case PH_PLAN: barMove(+1, 4); break;
    case PH_CNT: if (cnt > 0) cnt--; break;
    case PH_SAVE: case PH_CONF: mSel ^= 1; break;
    case PH_PATH: barMove(+1, 3); break;
    case PH_TODAY: barMove(+1, 4); break;
    case PH_PDET: barMove(+1, 6); break;
    case PH_RES: barMove(+1, 3); break;
    case PH_SET: barMove(+1, 7); break;
    case PH_ALARM: alarmMove(+1); break;
    case PH_ETYPE: barMove(+1, 3); break;
    case PH_EDET: barMove(+1, 4); break;
    case PH_EDATE: { uint16_t s = (uint16_t)qwGameSteps(); tmpV = tmpV > s ? (uint16_t)(tmpV - s) : 1; break; }
    case PH_SLIST: case PH_LOG: case PH_PLIST: case PH_EL: listMove(+1, listCount()); break;
    default: break;
  }
}

inline void finishRange() {
  if (purpose == 2) {
    eS = rs; eA = rf;
    if (eType == 0) { openRange(3, rs, rf); return; }
    addErr(); phase = PH_EL; sel = 1; return;
  }
  if (purpose == 3) { addErr(); phase = PH_EL; sel = 1; return; }
  if (purpose == 4) { pl.pS = rs; pl.pA = rf; savePlan(); phase = PH_SET; mSel = 5; return; }
  mt.lastS = rs; mt.lastA = rf;
  if (purpose == 0) { phase = PH_MARK; mSel = 1; }
  else { phase = PH_CNT; cnt = 0; }
}
inline void select() {
  ensure();
  switch (phase) {
    case PH_PATH: pl.path = mSel; pl.chosen = 1; savePlan(); toMenu(pl.path < 2 ? ID_TODAY : ID_MARK); break;
    case PH_MENU: {
      uint8_t ids[9]; menuIds(ids);
      kind = 0; curP = 255;
      switch (ids[sel]) {
        case ID_TODAY:
          if (pl.path == 0) { plMode = 0; plBack = PH_MENU; phase = PH_PLIST; sel = 0; }
          else { phase = PH_TODAY; mSel = 0; }
          break;
        case ID_MARK: rngBack = PH_MENU; openRange(0, 0, 0); break;
        case ID_REP: rngBack = PH_MENU; openRange(1, 0, 0); break;
        case ID_PASS: plMode = 3; plBack = PH_MENU; phase = PH_PLIST; sel = 0; break;
        case ID_PROG: phase = PH_PROG; break;
        case ID_SLIST: phase = PH_SLIST; sel = (uint8_t)(mt.lastS - 1); break;
        case ID_LOG: phase = PH_LOG; sel = 0; break;
        case ID_ERR: phase = PH_EL; sel = 0; break;
        default: phase = PH_SET; mSel = 0; break;
      }
      break;
    }
    case PH_RNG:
      if (purpose >= 2 ? fld < 1 : fld < 2) { fld++; break; }
      finishRange();
      break;
    case PH_MARK: {
      uint8_t m = mSel < 4 ? (uint8_t)(mSel + 1) : 0;
      for (int a = rf; a <= rt; a++) setM(gi(rs, a), m);
      saveBits(); saveMeta();
      dN = (uint16_t)(rt - rf + 1); dMark = m; dKind = 0;
      if (m == M_MEM) { phase = PH_PLAN; mSel = 1; } else phase = PH_DONE;
      break;
    }
    case PH_PLAN:
      if (mSel < 3) dKind = addPass(mSel) ? 2 : 3; else dKind = 0;
      phase = PH_DONE;
      break;
    case PH_CNT: if (cnt < 9999) cnt++; break;
    case PH_SAVE:
      if (mSel == 0) { saveReps(); dN = cnt; dKind = 1; cnt = 0; phase = PH_DONE; }
      else { cnt = 0; if (kind == 2) phase = PH_PDET; else toMenu(ID_REP); }
      break;
    case PH_PROG: toMenu(ID_PROG); break;
    case PH_SLIST: sdS = (uint8_t)(sel + 1); phase = PH_SDET; break;
    case PH_SDET: rngBack = PH_SDET; openRange(0, sdS, 0); break;
    case PH_LOG: break;
    case PH_DONE: if (kind == 2) { phase = PH_PLIST; sel = 0; } else toMenu(kind == 0 ? ID_MARK : ID_REP); break;
    case PH_TODAY:
      if (mSel == 0) { int s, a, b; sabakRange(s, a, b); kind = 1; curP = 255; rngBack = PH_TODAY; openRange(1, s, a, b); }
      else { plMode = mSel == 1 ? 1 : (mSel == 2 ? 2 : 0); plBack = PH_TODAY; phase = PH_PLIST; sel = 0; }
      break;
    case PH_PLIST: {
      int n = buildList(plMode);
      if (sel < n) { pSel = ord[sel]; phase = PH_PDET; mSel = 0; }
      break;
    }
    case PH_PDET: {
      if (pSel >= pl.nP) { phase = PH_PLIST; sel = 0; break; }
      Pass &p = pl.p[pSel];
      if (mSel == 0) { phase = PH_RES; mSel = 0; }
      else if (mSel == 1) { kind = 2; curP = pSel; rs = p.s; rf = p.a; rt = p.b; cnt = 0; phase = PH_CNT; }
      else if (mSel == 2) { phase = PH_EDATE; tmpV = p.next; }
      else if (mSel == 3) { p.diff = (uint8_t)((p.diff + 1) % 3); savePlan(); }
      else if (mSel == 4) { confKind = 0; phase = PH_CONF; mSel = 1; }
      else phase = PH_PLIST;
      break;
    }
    case PH_RES: applyResult(mSel); phase = PH_PLIST; sel = 0; break;
    case PH_EDATE: if (pSel < pl.nP) { pl.p[pSel].next = tmpV; savePlan(); } phase = PH_PDET; mSel = 0; break;
    case PH_SET:
      if (mSel == 0) { pl.path = (uint8_t)((pl.path + 1) % 3); savePlan(); }
      else if (mSel == 1) { pl.unit = (uint8_t)((pl.unit + 1) % 3); pl.goalI = 0; savePlan(); }
      else if (mSel == 2) { pl.goalI = (uint8_t)((pl.goalI + 1) % UCNT[pl.unit]); savePlan(); }
      else if (mSel == 3) { pl.sabqiI = (uint8_t)((pl.sabqiI + 1) % 4); savePlan(); }
      else if (mSel == 4) { pl.manzilI = (uint8_t)((pl.manzilI + 1) % 4); savePlan(); }
      else if (mSel == 6) { Sac::loadAlarm(); phase = PH_ALARM; mSel = 0; edit = 0; }
      else { int s, a, b; sabakRange(s, a, b); rngBack = PH_SET; openRange(4, s, a); }
      break;
    case PH_ALARM:
      if (mSel == 0) { Sac::alOn ^= 1; Sac::saveAlarm(); }
      else if (mSel == 4) { Sac::alVib ^= 1; Sac::saveAlarm(); }
      else if (mSel == 3) {
        if (!edit) { edit = 1; dayCur = 0; }
        else { Sac::alDays ^= (uint8_t)(1u << dayCur); Sac::saveAlarm(); }
      } else edit ^= 1;
      break;
    case PH_EL:
      if (sel == 0) { phase = PH_ETYPE; mSel = 0; }
      else { eSel = (uint8_t)(er.n - sel); phase = PH_EDET; mSel = 0; }
      break;
    case PH_ETYPE: eType = mSel; rngBack = PH_ETYPE; openRange(2, 0, 0); break;
    case PH_EDET:
      if (eSel >= er.n) { phase = PH_EL; break; }
      if (mSel == 0) { er.e[eSel].flags ^= 1; saveErr(); }
      else if (mSel == 1) { er.e[eSel].flags ^= 2; saveErr(); }
      else if (mSel == 2) { confKind = 1; phase = PH_CONF; mSel = 1; }
      else phase = PH_EL;
      break;
    case PH_CONF:
      if (mSel == 0) {
        if (confKind == 0) { if (pSel < pl.nP) delPass(pSel); phase = PH_PLIST; sel = 0; }
        else { for (int i = eSel; i + 1 < er.n; i++) er.e[i] = er.e[i + 1]; if (er.n) er.n--; saveErr(); phase = PH_EL; sel = 0; }
      } else { phase = confKind == 0 ? PH_PDET : PH_EDET; mSel = 0; }
      break;
  }
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d) { d.setTextSize(1); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
inline void txt(Adafruit_GFX &d, const char *s, int16_t x, int16_t base) { d.setCursor(x, base); d.print(s); }
inline void centered(Adafruit_GFX &d, const char *s, int16_t base) { txt(d, s, (200 - 8 * (int)strlen(s)) / 2, base); }
inline void header(Adafruit_GFX &d, const char *l, const char *r) {
  setup13(d);
  if (l) txt(d, l, 6, 12);
  if (r) txt(d, r, 194 - 8 * (int)strlen(r), 12);
  d.drawFastHLine(4, 15, 192, qwInk());
}
inline void bar(Adafruit_GFX &d, const char *s, int16_t y, int16_t h, bool inv) {
  if (inv) d.fillRect(4, y, 192, h, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  centered(d, s, y + (h + 10) / 2);   // caps are about 10 px high: centred in the bar
  d.setTextColor(qwInk());
}
inline void fieldBar(Adafruit_GFX &d, int16_t y, const char *lab, const char *val, bool inv) {
  if (inv) d.fillRect(4, y, 192, 24, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  txt(d, lab, 10, y + 17);
  txt(d, val, 190 - 8 * (int)strlen(val), y + 17);
  d.setTextColor(qwInk());
}
inline void pbar(Adafruit_GFX &d, int16_t x, int16_t y, int16_t w, int16_t h, uint32_t num, uint32_t den) {
  d.drawRect(x, y, w, h, qwInk());
  if (den && num) { int16_t f = (int16_t)((uint32_t)(w - 2) * (num > den ? den : num) / den); if (f < 1) f = 1; d.fillRect(x + 1, y + 1, f, h - 2, qwInk()); }
}
inline void rowText(Adafruit_GFX &d, const char *s, int16_t y, bool inv) {
  if (inv) d.fillRect(4, y, 192, 22, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  txt(d, s, 8, y + 16);
  d.setTextColor(qwInk());
}
inline void passRef(char *b, size_t n, int s, int a, int bb) {
  if (a == bb) snprintf(b, n, "S%d:%d", s, a); else snprintf(b, n, "S%d:%d-%d", s, a, bb);
}

inline void drawPath(Adafruit_GFX &d) {
  header(d, "CHOOSE YOUR PATH", nullptr);
  setup13(d);
  static const char *const T[3] = {"SIMPLE", "METHOD", "FREE"};
  static const char *const U[3] = {"Reviews due today", "Sabak, sabqi, manzil", "Tracking only, no plan"};
  for (int i = 0; i < 3; i++) {
    int y = 22 + i * 58;
    bool inv = (i == mSel);
    if (inv) d.fillRect(4, y, 192, 54, qwInk());
    d.setTextColor(inv ? qwPaper() : qwInk());
    centered(d, T[i], y + 22);
    centered(d, U[i], y + 42);
    d.setTextColor(qwInk());
  }
}
inline void drawMenu(Adafruit_GFX &d) {
  char r[16];
  snprintf(r, sizeof(r), "STREAK %d", streakNow());
  header(d, "HIFZ TRACKER", r);
  uint8_t ids[9];
  int n = menuIds(ids);
  for (int i = 0; i < n; i++) bar(d, IDN[ids[i]], 19 + i * 20, 19, i == sel);
}
inline void drawRng(Adafruit_GFX &d) {
  static const char *const T[5] = {"MARK PASSAGE", "REPEAT", "ERROR AT AYAH", "CONFUSED WITH", "SET POSITION"};
  header(d, T[purpose > 4 ? 0 : purpose], nullptr);
  drawSurahName(d, rs, 100, 20);
  setup13(d);
  char v[12];
  snprintf(v, sizeof(v), "%d", rs); fieldBar(d, 50, "SURAH", v, fld == 0);
  if (purpose >= 2) {
    snprintf(v, sizeof(v), "%d", rf); fieldBar(d, 76, "AYAH", v, fld == 1);
    char b[28];
    snprintf(b, sizeof(b), "%d ayat in surah", versesInSurah(rs)); centered(d, b, 126);
    centered(d, "UP/DOWN value  MENU next", 190);
    return;
  }
  snprintf(v, sizeof(v), "%d", rf); fieldBar(d, 76, "FROM AYAH", v, fld == 1);
  snprintf(v, sizeof(v), "%d", rt); fieldBar(d, 102, "TO AYAH", v, fld == 2);
  char b[28];
  snprintf(b, sizeof(b), "%d ayat in surah", versesInSurah(rs)); centered(d, b, 146);
  snprintf(b, sizeof(b), "%d selected", rt - rf + 1); centered(d, b, 164);
  centered(d, "UP/DOWN value  MENU next", 190);
}
inline void passageLine(Adafruit_GFX &d, int16_t y) {
  char b[28], r[20];
  passRef(r, sizeof(r), rs, rf, rt);
  snprintf(b, sizeof(b), "%s", r);
  centered(d, b, y);
}
inline void drawMark(Adafruit_GFX &d) {
  header(d, "MARK AS", nullptr);
  setup13(d);
  passageLine(d, 34);
  for (int i = 0; i < 5; i++) bar(d, i < 4 ? MN[i + 1] : "CLEAR MARK", 44 + i * 30, 26, i == mSel);
}
inline void drawPlan(Adafruit_GFX &d) {
  header(d, "PLAN REVIEWS?", nullptr);
  setup13(d);
  passageLine(d, 36);
  centered(d, "Level sets the spacing", 56);
  static const char *const L[4] = {"EASY", "NORMAL", "HARD", "NO PLAN"};
  for (int i = 0; i < 4; i++) bar(d, L[i], 70 + i * 30, 26, i == mSel);
}
inline void drawCnt(Adafruit_GFX &d) {
  header(d, "REPEAT", nullptr);
  setup13(d);
  passageLine(d, 34);
  char b[12];
  snprintf(b, sizeof(b), "%u", cnt);
  d.setFont(nullptr);
  qwSetText(d, 5);
  d.setTextColor(qwInk());
  d.setCursor((200 - 30 * (int)strlen(b)) / 2, 50);
  d.print(b);
  setup13(d);
  uint16_t g = GOALS[goalIdx];
  char l[28];
  if (g) { pbar(d, 8, 98, 184, 10, cnt, g); snprintf(l, sizeof(l), cnt >= g ? "GOAL %u REACHED" : "GOAL %u", g); }
  else snprintf(l, sizeof(l), "GOAL: FREE");
  centered(d, l, 128);
  snprintf(l, sizeof(l), "Today: %u reps", repsToday() + cnt);
  centered(d, l, 148);
  centered(d, "MENU +1      DOWN -1", 172);
  centered(d, "UP goal   BACK save", 190);
}
inline void drawSave(Adafruit_GFX &d) {
  header(d, "SAVE?", nullptr);
  setup13(d);
  passageLine(d, 40);
  char b[28];
  snprintf(b, sizeof(b), "%u repetitions", cnt);
  centered(d, b, 62);
  bar(d, "YES, SAVE", 80, 26, mSel == 0);
  bar(d, "NO, DISCARD", 112, 26, mSel == 1);
  centered(d, "BACK: keep counting", 168);
}
inline void drawProg(Adafruit_GFX &d) {
  header(d, "PROGRESS", nullptr);
  setup13(d);
  uint32_t c[5] = {0, 0, 0, 0, 0};
  int complete = 0;
  for (int s = 1; s <= NS; s++) {
    uint16_t k[5];
    counts(s, k);
    for (int i = 0; i < 5; i++) c[i] += k[i];
    if (k[3] + k[4] == versesInSurah((uint8_t)s)) complete++;
  }
  char b[28];
  snprintf(b, sizeof(b), "MEMORIZED  %5lu", (unsigned long)c[3]); txt(d, b, 8, 34);
  snprintf(b, sizeof(b), "TO REVIEW  %5lu", (unsigned long)c[4]); txt(d, b, 8, 50);
  snprintf(b, sizeof(b), "LEARNING   %5lu", (unsigned long)c[2]); txt(d, b, 8, 66);
  snprintf(b, sizeof(b), "TO LEARN   %5lu", (unsigned long)c[1]); txt(d, b, 8, 82);
  uint32_t mem = c[3] + c[4];
  pbar(d, 8, 92, 184, 10, mem, NV);
  snprintf(b, sizeof(b), "%lu%% memorized (%lu/%d)", (unsigned long)(mem * 100 / NV), (unsigned long)mem, NV);
  txt(d, b, 8, 122);
  snprintf(b, sizeof(b), "Surahs complete: %d", complete); txt(d, b, 8, 140);
  snprintf(b, sizeof(b), "Streak: %d (best %u)", streakNow(), mt.best); txt(d, b, 8, 158);
  snprintf(b, sizeof(b), "Today: %u reps", repsToday()); txt(d, b, 8, 176);
  snprintf(b, sizeof(b), "Total: %lu reps", (unsigned long)mt.repsTotal); txt(d, b, 8, 194);
}
inline void drawSlist(Adafruit_GFX &d) {
  int pg = sel / PERP, pages = (NS + PERP - 1) / PERP;
  char r[12];
  snprintf(r, sizeof(r), "%d/%d", pg + 1, pages);
  header(d, "SURAH LIST", r);
  setup13(d);
  for (int k = 0; k < PERP; k++) {
    int i = pg * PERP + k;
    if (i >= NS) break;
    int s = i + 1, y0 = 19 + k * 25;
    char b[8];
    snprintf(b, sizeof(b), "%3d", s);
    txt(d, b, 8, y0 + 17);
    drawSurahName(d, (uint8_t)s, 96, y0 + 1);
    uint16_t cc[5];
    counts(s, cc);
    pbar(d, 138, y0 + 7, 52, 9, cc[3] + cc[4], versesInSurah((uint8_t)s));
    if (i == sel) d.drawRect(4, y0, 192, 24, qwInk());
  }
}
inline void drawSdet(Adafruit_GFX &d) {
  char r[12];
  snprintf(r, sizeof(r), "SURAH %d", sdS);
  header(d, r, nullptr);
  drawSurahName(d, sdS, 100, 20);
  setup13(d);
  int n = versesInSurah(sdS);
  uint16_t c[5];
  counts(sdS, c);
  char b[28];
  snprintf(b, sizeof(b), "Ayat: %d", n); txt(d, b, 8, 62);
  snprintf(b, sizeof(b), "MEMORIZED %3u (%u%%)", c[3], (unsigned)(n ? c[3] * 100 / n : 0)); txt(d, b, 8, 78);
  snprintf(b, sizeof(b), "TO REVIEW %3u", c[4]); txt(d, b, 8, 94);
  snprintf(b, sizeof(b), "LEARNING  %3u", c[2]); txt(d, b, 8, 110);
  snprintf(b, sizeof(b), "TO LEARN  %3u", c[1]); txt(d, b, 8, 126);
  d.drawRect(4, 134, 192, 16, qwInk());
  for (int a = 1; a <= n; a++) {
    int x0 = 4 + (a - 1) * 192 / n, x1 = 4 + a * 192 / n, w = x1 - x0;
    if (w < 1) w = 1;
    uint8_t m = getM(gi(sdS, a));
    if (m == M_MEM) d.fillRect(x0, 135, w, 14, qwInk());
    else if (m == M_ING) d.fillRect(x0, 142, w, 7, qwInk());
    else if (m == M_REV) { d.fillRect(x0, 135, w, 2, qwInk()); d.fillRect(x0, 147, w, 2, qwInk()); }
    else if (m == M_LEARN) for (int xx = x0; xx < x0 + w; xx++) if (xx & 1) { d.drawPixel(xx, 138, qwInk()); d.drawPixel(xx, 142, qwInk()); d.drawPixel(xx, 146, qwInk()); }
  }
  centered(d, "full=memorized low=learning", 168);
  centered(d, "dots=to learn edges=review", 182);
  centered(d, "MENU: mark a passage", 197);
}
inline void drawLog(Adafruit_GFX &d) {
  int n = mt.nLog;
  header(d, "REPEAT LOG", nullptr);
  setup13(d);
  if (!n) { centered(d, "No repetitions saved", 90); centered(d, "yet.", 108); return; }
  int pg = sel / PERP;
  for (int k = 0; k < PERP; k++) {
    int i = pg * PERP + k;
    if (i >= n) break;
    const Log &e = mt.log[n - 1 - i];
    char dt[12], rf2[20], b[32];
    dateText(dt, sizeof(dt), e.day);
    passRef(rf2, sizeof(rf2), e.s, e.a, e.b);
    snprintf(b, sizeof(b), "%s %s x%u", dt, rf2, e.reps);
    rowText(d, b, 22 + k * 25, i == sel);
  }
}
inline void drawDone(Adafruit_GFX &d) {
  setup13(d);
  char b[28];
  if (dKind == 1) {
    header(d, "SAVED", nullptr);
    snprintf(b, sizeof(b), "%u repetitions", dN); centered(d, b, 70);
    snprintf(b, sizeof(b), "Streak: %d days", streakNow()); centered(d, b, 100);
    snprintf(b, sizeof(b), "Today: %u reps", repsToday()); centered(d, b, 122);
  } else {
    header(d, "DONE", nullptr);
    snprintf(b, sizeof(b), "%u ayat", dN); centered(d, b, 60);
    centered(d, "marked as", 80);
    centered(d, dMark ? MN[dMark] : "NOT SET", 100);
    if (dKind == 2) { snprintf(b, sizeof(b), "First review in %ud", dInfo); centered(d, b, 130); }
    else if (dKind == 3) { centered(d, "Review plan is full", 130); centered(d, "(60 passages)", 148); }
  }
  centered(d, "MENU: continue", 186);
}
inline void drawToday(Adafruit_GFX &d) {
  char r[16];
  snprintf(r, sizeof(r), "STREAK %d", streakNow());
  header(d, "TODAY'S PLAN", r);
  setup13(d);
  char b[28], rr[20];
  int s, a, bb;
  sabakRange(s, a, bb);
  passRef(rr, sizeof(rr), s, a, bb);
  snprintf(b, sizeof(b), "SABAK  %s", rr); bar(d, b, 22, 30, mSel == 0);
  snprintf(b, sizeof(b), "SABQI: %d", buildList(1)); bar(d, b, 56, 30, mSel == 1);
  snprintf(b, sizeof(b), "MANZIL: %d", buildList(2)); bar(d, b, 90, 30, mSel == 2);
  snprintf(b, sizeof(b), "DUE REVIEWS: %d", buildList(0)); bar(d, b, 124, 30, mSel == 3);
  snprintf(b, sizeof(b), "Goal: %d %s", UVAL[pl.unit > 2 ? 0 : pl.unit][pl.goalI], UNITN[pl.unit > 2 ? 0 : pl.unit]);
  centered(d, b, 174);
  centered(d, "SABAK = new portion", 192);
}
inline void drawPlist(Adafruit_GFX &d) {
  static const char *const T[4] = {"DUE REVIEWS", "SABQI (RECENT)", "MANZIL (OLD)", "ALL PASSAGES"};
  int n = buildList(plMode);
  char r[12];
  snprintf(r, sizeof(r), "%d", n);
  header(d, T[plMode > 3 ? 0 : plMode], r);
  setup13(d);
  if (!n) {
    centered(d, plMode == 3 ? "No passage planned." : "Nothing to do here.", 80);
    if (plMode == 3) { centered(d, "Mark a portion as", 106); centered(d, "MEMORIZED to plan it.", 124); }
    return;
  }
  int pg = sel / PERP;
  for (int k = 0; k < PERP; k++) {
    int i = pg * PERP + k;
    if (i >= n) break;
    const Pass &p = pl.p[ord[i]];
    char dt[12], rr[20], b[32];
    dateText(dt, sizeof(dt), p.next);
    passRef(rr, sizeof(rr), p.s, p.a, p.b);
    snprintf(b, sizeof(b), "%s %s%s", rr, dt, p.next < today() ? " !" : "");
    rowText(d, b, 22 + k * 25, i == sel);
  }
}
inline void drawPdet(Adafruit_GFX &d) {
  const Pass &p = pl.p[pSel < pl.nP ? pSel : 0];
  char rr[20], b[32], dt[12];
  passRef(rr, sizeof(rr), p.s, p.a, p.b);
  header(d, rr, nullptr);
  setup13(d);
  dateText(dt, sizeof(dt), p.next);
  int dd = (int)p.next - (int)today();
  if (dd > 0) snprintf(b, sizeof(b), "Next: %s (in %dd)", dt, dd);
  else if (dd == 0) snprintf(b, sizeof(b), "Next: %s (today)", dt);
  else snprintf(b, sizeof(b), "Next: %s (late %dd)", dt, -dd);
  txt(d, b, 8, 34);
  snprintf(b, sizeof(b), "Step %d/7  Level %s", p.step + 1, DIFFN[p.diff % 3]);
  txt(d, b, 8, 50);
  static const char *const L[6] = {"REVIEW RESULT", "REPEAT COUNTER", "EDIT NEXT DATE", "LEVEL", "DELETE", "BACK"};
  for (int i = 0; i < 6; i++) {
    if (i == 3) { snprintf(b, sizeof(b), "LEVEL: %s", DIFFN[p.diff % 3]); bar(d, b, 58 + i * 23, 21, i == mSel); }
    else bar(d, L[i], 58 + i * 23, 21, i == mSel);
  }
}
inline void drawRes(Adafruit_GFX &d) {
  const Pass &p = pl.p[pSel < pl.nP ? pSel : 0];
  char rr[20];
  passRef(rr, sizeof(rr), p.s, p.a, p.b);
  header(d, "HOW WAS IT?", nullptr);
  setup13(d);
  centered(d, rr, 40);
  static const char *const L[3] = {"RECITED WELL", "HESITATED", "FORGOT"};
  for (int i = 0; i < 3; i++) bar(d, L[i], 56 + i * 34, 30, i == mSel);
  centered(d, "Forgot = sooner review", 178);
}
inline void drawEdate(Adafruit_GFX &d) {
  header(d, "NEXT REVIEW", nullptr);
  setup13(d);
  const Pass &p = pl.p[pSel < pl.nP ? pSel : 0];
  char rr[20], b[32], dt[12];
  passRef(rr, sizeof(rr), p.s, p.a, p.b);
  centered(d, rr, 40);
  dateText(dt, sizeof(dt), tmpV);
  d.setFont(nullptr);
  qwSetText(d, 3);
  d.setTextColor(qwInk());
  d.setCursor((200 - 18 * (int)strlen(dt)) / 2, 62);
  d.print(dt);
  setup13(d);
  int dd = (int)tmpV - (int)today();
  if (dd > 0) snprintf(b, sizeof(b), "in %d days", dd);
  else if (dd == 0) snprintf(b, sizeof(b), "today");
  else snprintf(b, sizeof(b), "%d days late", -dd);
  centered(d, b, 120);
  centered(d, "UP later  DOWN sooner", 156);
  centered(d, "MENU: save   BACK: cancel", 178);
}
inline void drawSet(Adafruit_GFX &d) {
  header(d, "SETTINGS", nullptr);
  setup13(d);
  char b[28], rr[20];
  int u = pl.unit > 2 ? 0 : pl.unit, s, a, bb;
  sabakRange(s, a, bb);
  snprintf(b, sizeof(b), "PATH: %s", PATHN[pl.path % 3]); bar(d, b, 19, 23, mSel == 0);
  snprintf(b, sizeof(b), "GOAL UNIT: %s", UNITN[u]); bar(d, b, 44, 23, mSel == 1);
  snprintf(b, sizeof(b), "GOAL: %d %s", UVAL[u][pl.goalI], UNITN[u]); bar(d, b, 69, 23, mSel == 2);
  snprintf(b, sizeof(b), "SABQI: LAST %d DAYS", SQD[pl.sabqiI % 4]); bar(d, b, 94, 23, mSel == 3);
  snprintf(b, sizeof(b), "MANZIL: %d-DAY CYCLE", MCY[pl.manzilI % 4]); bar(d, b, 119, 23, mSel == 4);
  passRef(rr, sizeof(rr), s, a, a);
  snprintf(b, sizeof(b), "POSITION: %s", rr); bar(d, b, 144, 23, mSel == 5);
  Sac::loadAlarm();
  if (Sac::alOn) snprintf(b, sizeof(b), "ALARM: %02d:%02d", Sac::alH, Sac::alM); else snprintf(b, sizeof(b), "ALARM: OFF");
  bar(d, b, 169, 23, mSel == 6);
  centered(d, "MENU changes the value", 198);
}
inline void drawAlarm(Adafruit_GFX &d) {
  header(d, "HIFZ ALARM", nullptr);
  setup13(d);
  char v[8];
  if (mSel == 0) d.fillRect(4, 20, 192, 24, qwInk());
  d.setTextColor(mSel == 0 ? qwPaper() : qwInk());
  centered(d, Sac::alOn ? "ALARM: ON" : "ALARM: OFF", 37);
  d.setTextColor(qwInk());
  snprintf(v, sizeof(v), "%02d", Sac::alH); fieldBar(d, 48, edit && mSel == 1 ? "HOUR  [edit]" : "HOUR", v, mSel == 1);
  snprintf(v, sizeof(v), "%02d", Sac::alM); fieldBar(d, 74, edit && mSel == 2 ? "MINUTE  [edit]" : "MINUTE", v, mSel == 2);
  // weekdays
  static const char L[7] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
  if (mSel == 3) d.drawRect(4, 102, 192, 32, qwInk());
  for (int i = 0; i < 7; i++) {
    int x = 10 + i * 26;
    bool on = (Sac::alDays >> i) & 1;
    if (on) d.fillRect(x, 106, 22, 24, qwInk()); else d.drawRect(x, 106, 22, 24, qwInk());
    d.setTextColor(on ? qwPaper() : qwInk());
    char c[2] = {L[i], 0};
    txt(d, c, x + 7, 123);
    d.setTextColor(qwInk());
    if (edit && mSel == 3 && i == dayCur) d.drawRect(x - 2, 104, 26, 28, qwInk());
  }
  if (mSel == 4) d.fillRect(4, 138, 192, 24, qwInk());
  d.setTextColor(mSel == 4 ? qwPaper() : qwInk());
  centered(d, Sac::alVib ? "VIBRATE: ON" : "VIBRATE: OFF", 155);
  d.setTextColor(qwInk());
  if (edit) { centered(d, "UP/DOWN change", 178); centered(d, "MENU or BACK: done", 194); }
  else { centered(d, "Shows HIFZ TIME on the", 178); centered(d, "watch face until BACK", 194); }
}
inline void errText(char *b, size_t n, const Err &e) {
  char suffix[6] = "";
  if (e.flags & 2) snprintf(suffix, sizeof(suffix), " OK"); else if (e.flags & 1) snprintf(suffix, sizeof(suffix), " T");
  if (e.type == 0) snprintf(b, n, "S%d:%d CONF S%d:%d%s", e.s, e.a, e.s2, e.a2, suffix);
  else snprintf(b, n, "S%d:%d %s%s", e.s, e.a, ETN[e.type % 3], suffix);
}
inline void drawEl(Adafruit_GFX &d) {
  int n = 1 + er.n;
  int pg = sel / PERP;
  char r[12];
  snprintf(r, sizeof(r), "%d", er.n);
  header(d, "ERROR JOURNAL", r);
  setup13(d);
  for (int k = 0; k < PERP; k++) {
    int i = pg * PERP + k;
    if (i >= n) break;
    char b[32];
    if (i == 0) snprintf(b, sizeof(b), "+ ADD AN ERROR");
    else errText(b, sizeof(b), er.e[er.n - i]);
    rowText(d, b, 22 + k * 25, i == sel);
  }
  if (er.n == 0) { centered(d, "T = to check with a teacher", 150); centered(d, "OK = corrected", 168); }
}
inline void drawEtype(Adafruit_GFX &d) {
  header(d, "ERROR TYPE", nullptr);
  setup13(d);
  centered(d, "What went wrong?", 38);
  static const char *const L[3] = {"CONFUSED AYAH", "HARD START", "LINK ERROR"};
  for (int i = 0; i < 3; i++) bar(d, L[i], 52 + i * 38, 32, i == mSel);
  centered(d, "Mixed with another ayah,", 172);
  centered(d, "a start or a link.", 190);
}
inline void drawEdet(Adafruit_GFX &d) {
  header(d, "ERROR", nullptr);
  setup13(d);
  if (eSel >= er.n) return;
  const Err &e = er.e[eSel];
  char b[32], dt[12];
  snprintf(b, sizeof(b), "S%d:%d %s", e.s, e.a, e.type == 0 ? "CONFUSED" : (e.type == 1 ? "HARD START" : "LINK ERROR"));
  txt(d, b, 8, 34);
  if (e.type == 0) { snprintf(b, sizeof(b), "with S%d:%d", e.s2, e.a2); txt(d, b, 8, 50); }
  dateText(dt, sizeof(dt), e.day);
  snprintf(b, sizeof(b), "Added %s", dt); txt(d, b, 8, 66);
  snprintf(b, sizeof(b), "TEACHER CHECK: %s", (e.flags & 1) ? "YES" : "NO"); bar(d, b, 78, 26, mSel == 0);
  snprintf(b, sizeof(b), "CORRECTED: %s", (e.flags & 2) ? "YES" : "NO"); bar(d, b, 106, 26, mSel == 1);
  bar(d, "DELETE", 134, 26, mSel == 2);
  bar(d, "BACK", 162, 26, mSel == 3);
}
inline void drawConf(Adafruit_GFX &d) {
  header(d, confKind == 0 ? "DELETE PASSAGE?" : "DELETE ERROR?", nullptr);
  setup13(d);
  centered(d, "This cannot be undone.", 60);
  bar(d, "YES, DELETE", 84, 26, mSel == 0);
  bar(d, "NO", 116, 26, mSel == 1);
}
inline void draw(Adafruit_GFX &d) {
  ensure();
  d.fillScreen(qwPaper());
  switch (phase) {
    case PH_PATH: drawPath(d); break;
    case PH_RNG: drawRng(d); break;
    case PH_MARK: drawMark(d); break;
    case PH_PLAN: drawPlan(d); break;
    case PH_CNT: drawCnt(d); break;
    case PH_SAVE: drawSave(d); break;
    case PH_PROG: drawProg(d); break;
    case PH_SLIST: drawSlist(d); break;
    case PH_SDET: drawSdet(d); break;
    case PH_LOG: drawLog(d); break;
    case PH_DONE: drawDone(d); break;
    case PH_TODAY: drawToday(d); break;
    case PH_PLIST: drawPlist(d); break;
    case PH_PDET: drawPdet(d); break;
    case PH_RES: drawRes(d); break;
    case PH_EDATE: drawEdate(d); break;
    case PH_SET: drawSet(d); break;
    case PH_EL: drawEl(d); break;
    case PH_ETYPE: drawEtype(d); break;
    case PH_EDET: drawEdet(d); break;
    case PH_CONF: drawConf(d); break;
    case PH_ALARM: drawAlarm(d); break;
    default: drawMenu(d);
  }
  d.setFont(nullptr);
}

}  // namespace Hz
