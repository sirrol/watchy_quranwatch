#pragma once
/*
  Qibla.h
  ------------------------------------------------------------
  QIBLA - an app of the APPS menu (between TEA TIMER and GAMES).

  Shows the direction of the Kaaba (Makkah) for a place, as an angle in
  degrees clockwise from TRUE north (great-circle bearing), the compass
  point (N, NNE, NE...), the distance, and a compass rose with an arrow.
  The watch has no compass sensor: line the angle up with a compass or a
  phone.

  The first line of the list, PRAYER PLACE, is the latitude / longitude
  used for the prayer times (SETTINGS). Below it, up to 30 places of your
  own, each with a name (12 characters) and coordinates in decimal degrees
  (+48.8566 +2.3522; north and east are positive).

  LIST   UP/DOWN choose, MENU opens a place or adds one ("+ ADD A PLACE").
  VIEW   the Qibla for that place. MENU opens its options.
  OPTIONS  USE FOR PRAYERS (copies the coordinates into SETTINGS, after a
           confirmation; the time zone is NOT changed), EDIT COORDINATES,
           RENAME, DELETE.
  NUMBER UP/DOWN change the value (hold = faster); MENU changes the step
         (1, 0.1, 0.01, 0.001 degree) and finally validates.

  The list is saved in flash at every change. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include "UiCommon.h"
#include "Storage.h"
#include "NotoMono13.h"

namespace Qb {

constexpr int MAXP = 30, PER = 6, TL = 12, NCH = 40;
static const char CS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.'";   // NCH characters
static const int32_t STEPS[4] = {10000, 1000, 100, 10};                // in 1/10000 degree
static const char *const PT[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                   "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};

struct Place { char name[TL + 1]; int32_t lat, lon; };   // lat, lon in 1/10000 degree
struct Store { uint8_t n; Place p[MAXP]; };
enum Ph : uint8_t { PH_LIST = 0, PH_VIEW, PH_OPT, PH_NUM, PH_TITLE, PH_DEL, PH_USE };

RTC_DATA_ATTR uint8_t phase = PH_LIST, sel = 0, mSel = 0, numWhich = 0, stepIdx = 0, tpos = 0, tchar = 0, eIdx = 0;
RTC_DATA_ATTR char tbuf[TL + 1];
RTC_DATA_ATTR int32_t wLat = 0, wLon = 0;     // value being edited

static Store st;
static bool ld = false;
static int32_t homeLat = 0, homeLon = 0;       // the prayer-time coordinates (SETTINGS)

inline void loadHome() {
  AppSettings s;
  QWStorage::loadSettings(s);
  homeLat = (int32_t)lroundf(s.latitude * 10000.0f);
  homeLon = (int32_t)lroundf(s.longitude * 10000.0f);
}
inline void ensure() {
  if (ld) return;
  memset(&st, 0, sizeof(st));
  Store t;
  memset(&t, 0, sizeof(t));
  if (QWStorage::loadBlob("qibS", &t, sizeof(t)) && t.n <= MAXP) st = t;
  loadHome();
  ld = true;
}
inline void save() { QWStorage::saveBlob("qibS", &st, sizeof(st)); }
// list index 0 = the prayer place, 1..n = saved places, then "+ ADD A PLACE"
inline int items() { return 1 + st.n + (st.n < MAXP ? 1 : 0); }
inline void clampSel() { int it = items(); if (sel >= it) sel = (uint8_t)(it - 1); }

// ---- the maths: bearing and distance from a place to the Kaaba ----
inline void calc(int32_t lat, int32_t lon, float &brg, float &km) {
  const float D = 0.0174532925f;
  float p1 = lat * 0.0001f * D, l1 = lon * 0.0001f * D, p2 = 21.4225f * D, l2 = 39.8262f * D;
  float dl = l2 - l1;
  float y = sinf(dl) * cosf(p2);
  float x = cosf(p1) * sinf(p2) - sinf(p1) * cosf(p2) * cosf(dl);
  brg = atan2f(y, x) / D;
  if (brg < 0) brg += 360.0f;
  float s1 = sinf((p2 - p1) * 0.5f), s2 = sinf(dl * 0.5f);
  float a = s1 * s1 + cosf(p1) * cosf(p2) * s2 * s2;
  if (a > 1) a = 1;
  km = 12742.0f * asinf(sqrtf(a));
}
inline const char *point(float brg) { return PT[((int)((brg + 11.25f) / 22.5f)) % 16]; }
inline void fmtDeg(char *b, size_t n, int32_t v) {
  int32_t a = v < 0 ? -v : v;
  snprintf(b, n, "%c%d.%04d", v < 0 ? '-' : '+', (int)(a / 10000), (int)(a % 10000));
}
inline void coordsOf(int idx, int32_t &lat, int32_t &lon, const char *&name) {
  if (idx == 0) { lat = homeLat; lon = homeLon; name = "PRAYER PLACE"; }
  else { lat = st.p[idx - 1].lat; lon = st.p[idx - 1].lon; name = st.p[idx - 1].name; }
}

inline void start() { ld = false; ensure(); phase = PH_LIST; sel = 0; }
inline bool busy() { return true; }   // leaving the app always asks first
inline bool back() {
  ensure();
  switch (phase) {
    case PH_VIEW: phase = PH_LIST; return true;
    case PH_OPT: phase = PH_VIEW; return true;
    case PH_NUM: case PH_TITLE: phase = (eIdx == 255) ? PH_LIST : PH_OPT; return true;
    case PH_DEL: case PH_USE: phase = PH_OPT; return true;
    default: return false;
  }
}

inline void startTitle(uint8_t idx) { eIdx = idx; tbuf[0] = 0; tpos = 0; tchar = 0; phase = PH_TITLE; }
inline void finishTitle() {
  int n = (int)strlen(tbuf);
  while (n > 0 && tbuf[n - 1] == ' ') tbuf[--n] = 0;
  if (n == 0) snprintf(tbuf, sizeof(tbuf), "PLACE");
  if (eIdx == 255) {                       // new place: now its coordinates (start from the prayer place)
    wLat = homeLat; wLon = homeLon; numWhich = 0; stepIdx = 0; phase = PH_NUM;
  } else {
    memcpy(st.p[sel - 1].name, tbuf, TL); st.p[sel - 1].name[TL] = 0;
    save();
    phase = PH_OPT;
  }
}
inline void commit() {
  if (eIdx == 255) {
    if (st.n >= MAXP) { phase = PH_LIST; return; }
    Place &p = st.p[st.n];
    memset(&p, 0, sizeof(p));
    memcpy(p.name, tbuf, TL); p.name[TL] = 0;
    p.lat = wLat; p.lon = wLon;
    st.n++; sel = st.n;                    // list index of the new place
  } else {
    st.p[sel - 1].lat = wLat; st.p[sel - 1].lon = wLon;
  }
  save();
  phase = PH_VIEW;
}
inline void numChange(int dir) {
  int32_t step = STEPS[stepIdx] * qwGameSteps();
  int32_t lim = numWhich ? 1800000 : 900000;
  int32_t &v = numWhich ? wLon : wLat;
  v += dir * step;
  if (v > lim) v = lim;
  if (v < -lim) v = -lim;
}
inline void usePlace() {
  AppSettings s;
  QWStorage::loadSettings(s);
  int32_t la, lo; const char *nm;
  coordsOf(sel, la, lo, nm);
  s.latitude = la * 0.0001f; s.longitude = lo * 0.0001f;
  QWStorage::saveSettings(s);
  loadHome();
}

inline void up() {
  ensure();
  int s = qwGameSteps();
  if (phase == PH_LIST) {
    int it = items();
    if (sel >= s) sel -= (uint8_t)s; else sel = (s == 1) ? (uint8_t)(it - 1) : 0;
  } else if (phase == PH_OPT) mSel = (uint8_t)((mSel + 4) % 5);
  else if (phase == PH_NUM) numChange(+1);
  else if (phase == PH_TITLE) tchar = (uint8_t)((tchar + s) % (NCH + 1));
  else if (phase == PH_DEL || phase == PH_USE) mSel ^= 1;
}
inline void down() {
  ensure();
  int s = qwGameSteps();
  if (phase == PH_LIST) {
    int it = items();
    if (sel + s < it) sel += (uint8_t)s; else sel = (s == 1 && sel == it - 1) ? 0 : (uint8_t)(it - 1);
  } else if (phase == PH_OPT) mSel = (uint8_t)((mSel + 1) % 5);
  else if (phase == PH_NUM) numChange(-1);
  else if (phase == PH_TITLE) tchar = (uint8_t)((tchar + (NCH + 1) - (s % (NCH + 1))) % (NCH + 1));
  else if (phase == PH_DEL || phase == PH_USE) mSel ^= 1;
}
inline void select() {
  ensure();
  if (phase == PH_LIST) {
    clampSel();
    if (sel <= st.n) phase = PH_VIEW;
    else startTitle(255);
  } else if (phase == PH_VIEW) {
    if (sel == 0) phase = PH_LIST;
    else { mSel = 0; phase = PH_OPT; }
  } else if (phase == PH_OPT) {
    if (mSel == 0) { mSel = 1; phase = PH_USE; }
    else if (mSel == 1) { eIdx = sel; wLat = st.p[sel - 1].lat; wLon = st.p[sel - 1].lon; numWhich = 0; stepIdx = 0; phase = PH_NUM; }
    else if (mSel == 2) startTitle(sel);
    else if (mSel == 3) { mSel = 1; phase = PH_DEL; }
    else phase = PH_VIEW;
  } else if (phase == PH_NUM) {
    if (stepIdx < 3) stepIdx++;
    else if (numWhich == 0) { numWhich = 1; stepIdx = 0; }
    else commit();
  } else if (phase == PH_TITLE) {
    if (tchar >= NCH) { finishTitle(); return; }
    tbuf[tpos++] = CS[tchar]; tbuf[tpos] = 0; tchar = 0;
    if (tpos >= TL) finishTitle();
  } else if (phase == PH_USE) {
    if (mSel == 0) { usePlace(); sel = 0; phase = PH_VIEW; }
    else { mSel = 0; phase = PH_OPT; }
  } else if (phase == PH_DEL) {
    if (mSel == 0) {
      for (int i = sel - 1; i + 1 < st.n; i++) st.p[i] = st.p[i + 1];
      if (st.n) { st.n--; memset(&st.p[st.n], 0, sizeof(Place)); }
      save();
      clampSel();
      phase = PH_LIST;
    } else { mSel = 0; phase = PH_OPT; }
  }
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d, int size = 1) { d.setTextSize(size); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
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
  centered(d, s, y + h - 6);
  d.setTextColor(qwInk());
}

inline void drawList(Adafruit_GFX &d) {
  int it = items();
  int pages = (it + PER - 1) / PER, pg = sel / PER;
  char r[10];
  snprintf(r, sizeof(r), "%d/%d", pg + 1, pages);
  header(d, "QIBLA", r);
  setup13(d);
  for (int k = 0; k < PER; k++) {
    int i = pg * PER + k;
    if (i >= it) break;
    int16_t y0 = 20 + k * 29;
    bool inv = (i == sel);
    if (inv) d.fillRect(4, y0, 192, 27, qwInk());
    d.setTextColor(inv ? qwPaper() : qwInk());
    if (i > st.n) { centered(d, "+ ADD A PLACE", y0 + 19); continue; }
    int32_t la, lo; const char *nm;
    coordsOf(i, la, lo, nm);
    float b, km; calc(la, lo, b, km);
    char s[14];
    txt(d, nm, 8, y0 + 19);
    snprintf(s, sizeof(s), "%d %s", (int)lroundf(b) % 360, point(b));
    txt(d, s, 192 - 8 * (int)strlen(s), y0 + 19);
  }
  d.setTextColor(qwInk());
}

inline void drawView(Adafruit_GFX &d) {
  int32_t la, lo; const char *nm;
  coordsOf(sel, la, lo, nm);
  float b, km; calc(la, lo, b, km);
  header(d, "QIBLA", nullptr);
  setup13(d);
  centered(d, nm, 34);
  char s[24];
  if (km < 1.0f) {
    centered(d, "YOU ARE AT", 96); centered(d, "THE KAABA", 114);
  } else {
    int deg = (int)lroundf(b) % 360;
    const int cx = 140, cy = 106, R = 38;
    txt(d, "QIBLA", 6, 64);
    qwSetText(d, 3); d.setTextColor(qwInk());
    snprintf(s, sizeof(s), "%d", deg);
    d.setCursor(6, 72); d.print(s);
    setup13(d);
    txt(d, "deg from N", 6, 114);
    qwSetText(d, 3); d.setTextColor(qwInk());
    d.setCursor(6, 124); d.print(point(b));
    setup13(d);
    // compass rose
    uint16_t ink = qwInk();
    d.drawCircle(cx, cy, R, ink);
    for (int k = 0; k < 8; k++) {
      float a = k * 0.785398f, c = cosf(a), sn = sinf(a);
      int len = (k == 0) ? 9 : 5;
      d.drawLine(cx + (int)lroundf((R - len) * sn), cy - (int)lroundf((R - len) * c), cx + (int)lroundf(R * sn), cy - (int)lroundf(R * c), ink);
    }
    txt(d, "N", cx - 4, cy - R - 8);
    float t = b * 0.0174533f, sn = sinf(t), c = cosf(t);
    int tx = cx + (int)lroundf(24 * sn), ty = cy - (int)lroundf(24 * c);
    int bx = cx + (int)lroundf(29 * sn), by = cy - (int)lroundf(29 * c);   // cube side
    for (int o = -1; o <= 1; o++) d.drawLine(cx + o, cy, bx + o, by, ink);
    d.drawLine(cx, cy + 1, bx, by + 1, ink);
    // arrow head
    float px = -c, py = -sn;     // perpendicular (screen coords)
    int hx = cx + (int)lroundf(17 * sn), hy = cy - (int)lroundf(17 * c);
    d.fillTriangle(bx, by, hx + (int)lroundf(6 * px), hy + (int)lroundf(6 * py), hx - (int)lroundf(6 * px), hy - (int)lroundf(6 * py), ink);
    (void)tx; (void)ty;
    // the Kaaba on the rim
    int kx = cx + (int)lroundf(R * sn), ky = cy - (int)lroundf(R * c);
    d.fillRect(kx - 5, ky - 5, 11, 11, ink);
    d.drawFastHLine(kx - 5, ky - 2, 11, qwPaper());
    d.drawFastHLine(kx - 5, ky - 1, 11, qwPaper());
    snprintf(s, sizeof(s), "%ld km to Kaaba", (long)lroundf(km));
    centered(d, s, 172);
  }
  char a[12], o[12];
  fmtDeg(a, sizeof(a), la); fmtDeg(o, sizeof(o), lo);
  snprintf(s, sizeof(s), "%s %s", a, o);
  centered(d, s, 192);
}

inline void drawOpt(Adafruit_GFX &d) {
  header(d, st.p[sel - 1].name, nullptr);
  setup13(d);
  float b, km; calc(st.p[sel - 1].lat, st.p[sel - 1].lon, b, km);
  char s[24];
  snprintf(s, sizeof(s), "Qibla %d deg %s", (int)lroundf(b) % 360, point(b));
  txt(d, s, 8, 36);
  static const char *const L[5] = {"USE FOR PRAYERS", "EDIT COORDINATES", "RENAME", "DELETE", "BACK"};
  for (int i = 0; i < 5; i++) bar(d, L[i], 50 + i * 28, 25, i == mSel);
}
inline void drawNum(Adafruit_GFX &d) {
  header(d, numWhich ? "LONGITUDE" : "LATITUDE", nullptr);
  setup13(d);
  centered(d, eIdx == 255 ? tbuf : st.p[sel - 1].name, 36);
  char b[12];
  fmtDeg(b, sizeof(b), numWhich ? wLon : wLat);
  d.setFont(nullptr);
  qwSetText(d, 3);
  d.setTextColor(qwInk());
  d.setCursor((200 - 18 * (int)strlen(b)) / 2, 54);
  d.print(b);
  setup13(d);
  centered(d, numWhich ? "East +  /  West -" : "North +  /  South -", 100);
  static const char *const S[4] = {"STEP 1 deg", "STEP 0.1 deg", "STEP 0.01 deg", "STEP 0.001 deg"};
  if (stepIdx < 4) bar(d, S[stepIdx], 114, 24, true);
  centered(d, "UP/DOWN: change value", 164);
  centered(d, "MENU: next step", 182);
  centered(d, "(after the last: validate)", 198);
}
inline void drawTitle(Adafruit_GFX &d) {
  header(d, eIdx == 255 ? "NEW PLACE" : "RENAME", nullptr);
  setup13(d, 2);
  for (int i = 0; i < TL; i++) {
    int16_t x = 4 + i * 16;
    char c[2] = {0, 0};
    bool cur = (i == tpos);
    if (i < tpos) c[0] = tbuf[i];
    else if (cur) c[0] = tchar < NCH ? CS[tchar] : '>';
    if (cur) d.fillRect(x, 40, 16, 26, qwInk());
    d.setTextColor(cur ? qwPaper() : qwInk());
    if (c[0]) { d.setCursor(x, 60); d.print(c); }
    d.drawFastHLine(x + 1, 70, 14, qwInk());
  }
  setup13(d);
  centered(d, "UP/DOWN: change letter", 104);
  centered(d, "MENU: validate letter", 122);
  centered(d, "Pick '>' then MENU", 148);
  centered(d, "to finish the name.", 164);
}
inline void drawDel(Adafruit_GFX &d) {
  header(d, "DELETE PLACE?", nullptr);
  setup13(d);
  centered(d, st.p[sel - 1].name, 50);
  centered(d, "This cannot be undone.", 76);
  bar(d, "YES, DELETE", 96, 24, mSel == 0);
  bar(d, "NO", 126, 24, mSel == 1);
}
inline void drawUse(Adafruit_GFX &d) {
  header(d, "PRAYER PLACE?", nullptr);
  setup13(d);
  centered(d, st.p[sel - 1].name, 40);
  centered(d, "Use these coordinates", 66);
  centered(d, "for the prayer times?", 82);
  centered(d, "Time zone is not", 104);
  centered(d, "changed: check it in", 120);
  centered(d, "SETTINGS.", 136);
  bar(d, "YES, USE THEM", 150, 22, mSel == 0);
  bar(d, "NO", 176, 22, mSel == 1);
}
inline void draw(Adafruit_GFX &d) {
  ensure();
  clampSel();
  d.fillScreen(qwPaper());
  bool saved = (sel >= 1 && sel <= st.n);
  switch (phase) {
    case PH_VIEW: if (sel <= st.n) drawView(d); else drawList(d); break;
    case PH_OPT: if (saved) drawOpt(d); else drawList(d); break;
    case PH_NUM: if (eIdx == 255 || saved) drawNum(d); else drawList(d); break;
    case PH_TITLE: drawTitle(d); break;
    case PH_DEL: if (saved) drawDel(d); else drawList(d); break;
    case PH_USE: if (saved) drawUse(d); else drawList(d); break;
    default: drawList(d);
  }
  d.setFont(nullptr);
}

}  // namespace Qb
