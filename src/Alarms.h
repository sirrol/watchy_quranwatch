#pragma once
/*
  Alarms.h
  ------------------------------------------------------------
  ALARMS - an app of the APPS menu: up to 8 named alarms.

  LIST   "+ NEW ALARM" and the existing alarms (time, name, ON / OFF).
  EDIT   ON/OFF, HOUR, MINUTE and DAYS (MENU starts the edit, UP/DOWN change,
         MENU or BACK ends it; in DAYS, MENU ticks the day under the cursor),
         VIBRATION (MENU = next Morse pattern, played at once: UP, GO, RISE,
         ARISE, WAKE), NAME (UP/DOWN letter, MENU validates it, pick ">" to
         finish) and DELETE.
  When an alarm rings, the watch face shows a sonar picture with the name
  in the info zone and vibrates once a minute for 5 minutes, until BACK.
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "AlarmCore.h"
#include "NotoMono13.h"

namespace Am {

using Alm::al;
using Alm::n;
constexpr int PERP = 7, NCH = 40;
static const char CS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.'";   // NCH characters
enum Ph : uint8_t { PH_LIST = 0, PH_EDIT, PH_NAME, PH_DEL, PH_QACT, PH_QSET };

RTC_DATA_ATTR uint8_t phase = PH_LIST, sel = 0, mSel = 0, edit = 0, dayCur = 0, cur = 0, tpos = 0, tchar = 0;
RTC_DATA_ATTR char tbuf[Alm::NAMEL + 1];
RTC_DATA_ATTR uint16_t qd = 15;   // quick alarm: minutes from now

// list rows: 0 = QUICK ALARM, then "+ NEW ALARM" (if there is room), then the alarms
inline int rowsOf() { return 1 + n + (n < Alm::MAXA ? 1 : 0); }
inline int firstAlarmRow() { return 1 + (n < Alm::MAXA ? 1 : 0); }
inline bool quickActive() { return Alm::q.on && Alm::q.day == qwGameDay; }
inline int quickMax() { return 1439 - (int)qwGameMin; }   // latest delay that is still today
inline void toList() { phase = PH_LIST; sel = (uint8_t)(cur + firstAlarmRow()); if (sel >= rowsOf()) sel = 0; }

inline void start() { Alm::load(); phase = PH_LIST; sel = 0; edit = 0; }
inline bool busy() { return true; }
inline bool back() {
  Alm::load();
  switch (phase) {
    case PH_EDIT: if (edit) edit = 0; else toList(); return true;
    case PH_NAME: phase = PH_EDIT; mSel = 5; return true;
    case PH_DEL: phase = PH_EDIT; mSel = 6; return true;
    case PH_QACT: case PH_QSET: phase = PH_LIST; sel = 0; return true;
    default: return false;
  }
}

inline int mul() { int s = qwGameSteps(); return s == 1 ? 1 : (s == 3 ? 5 : 20); }
inline void up() {
  Alm::load();
  int s = qwGameSteps();
  if (phase == PH_LIST) { int r = rowsOf(); sel = (uint8_t)((sel + r - s % r) % r); }
  else if (phase == PH_NAME) tchar = (uint8_t)((tchar + s) % (NCH + 1));
  else if (phase == PH_DEL) mSel ^= 1;
  else if (phase == PH_QACT) mSel = (uint8_t)((mSel + 2) % 3);
  else if (phase == PH_QSET) { int v = qd + mul(); qd = (uint16_t)(v > quickMax() ? quickMax() : v); if (qd < 1) qd = 1; }
  else if (phase == PH_EDIT) {
    Alm::Al &a = al[cur];
    if (!edit) mSel = (uint8_t)((mSel + 6) % 7);
    else if (mSel == 1) { a.h = (uint8_t)((a.h + s) % 24); Alm::save(); }
    else if (mSel == 2) { a.m = (uint8_t)((a.m + mul()) % 60); Alm::save(); }
    else if (mSel == 3) dayCur = (uint8_t)((dayCur + 6) % 7);
  }
}
inline void down() {
  Alm::load();
  int s = qwGameSteps();
  if (phase == PH_LIST) { int r = rowsOf(); sel = (uint8_t)((sel + s) % r); }
  else if (phase == PH_NAME) tchar = (uint8_t)((tchar + (NCH + 1) - (s % (NCH + 1))) % (NCH + 1));
  else if (phase == PH_DEL) mSel ^= 1;
  else if (phase == PH_QACT) mSel = (uint8_t)((mSel + 1) % 3);
  else if (phase == PH_QSET) { int v = qd - mul(); qd = (uint16_t)(v < 1 ? 1 : v); }
  else if (phase == PH_EDIT) {
    Alm::Al &a = al[cur];
    if (!edit) mSel = (uint8_t)((mSel + 1) % 7);
    else if (mSel == 1) { a.h = (uint8_t)((a.h + 24 - s % 24) % 24); Alm::save(); }
    else if (mSel == 2) { a.m = (uint8_t)((a.m + 60 - mul() % 60) % 60); Alm::save(); }
    else if (mSel == 3) dayCur = (uint8_t)((dayCur + 1) % 7);
  }
}

inline void finishName() {
  int k = (int)strlen(tbuf);
  while (k > 0 && tbuf[k - 1] == ' ') tbuf[--k] = 0;
  if (k > 0) { strncpy(al[cur].name, tbuf, Alm::NAMEL); al[cur].name[Alm::NAMEL] = 0; Alm::save(); }
  phase = PH_EDIT; mSel = 5;
}
inline void select() {
  Alm::load();
  switch (phase) {
    case PH_LIST: {
      if (sel == 0) {
        if (quickActive()) { phase = PH_QACT; mSel = 0; }
        else { qd = 15; if (qd > quickMax()) qd = (uint16_t)(quickMax() > 1 ? quickMax() : 1); phase = PH_QSET; }
      } else if (n < Alm::MAXA && sel == 1) {
        Alm::Al &a = al[n];
        memset(&a, 0, sizeof(a));
        snprintf(a.name, sizeof(a.name), "ALARM %d", n + 1);
        a.h = 7; a.m = 0; a.days = 0x7F; a.pat = 4; a.on = 1;
        Alm::dis[n] = 0;
        cur = n; n++;
        Alm::save();
        phase = PH_EDIT; mSel = 1; edit = 0;
      } else {
        int i = sel - firstAlarmRow();
        if (i >= 0 && i < n) { cur = (uint8_t)i; phase = PH_EDIT; mSel = 0; edit = 0; }
      }
      break;
    }
    case PH_EDIT: {
      Alm::Al &a = al[cur];
      if (mSel == 0) { a.on ^= 1; Alm::save(); }
      else if (mSel == 1 || mSel == 2) edit ^= 1;
      else if (mSel == 3) {
        if (!edit) { edit = 1; dayCur = 0; }
        else { a.days ^= (uint8_t)(1u << dayCur); Alm::save(); }
      } else if (mSel == 4) { a.pat = (uint8_t)((a.pat + 1) % 5); Alm::save(); Alm::morse(Alm::MORSE[a.pat]); }
      else if (mSel == 5) { tbuf[0] = 0; tpos = 0; tchar = 0; phase = PH_NAME; }
      else { phase = PH_DEL; mSel = 1; }
      break;
    }
    case PH_QACT:
      if (mSel == 0) { qd = 15; if (qd > quickMax()) qd = (uint16_t)(quickMax() > 1 ? quickMax() : 1); phase = PH_QSET; }
      else if (mSel == 1) { Alm::q.pat = (uint8_t)((Alm::q.pat + 1) % 5); Alm::saveQ(); Alm::morse(Alm::MORSE[Alm::q.pat]); }
      else { Alm::q.on = 0; Alm::saveQ(); phase = PH_LIST; sel = 0; }
      break;
    case PH_QSET:
      if (quickMax() >= 1) {
        int t = (int)qwGameMin + qd;
        if (t > 1439) t = 1439;
        if (!Alm::q.on && Alm::q.pat == 0 && Alm::q.day == 0) Alm::q.pat = 4;
        Alm::q.on = 1; Alm::q.h = (uint8_t)(t / 60); Alm::q.m = (uint8_t)(t % 60); Alm::q.day = (uint16_t)qwGameDay;
        Alm::dis[Alm::MAXA] = 0;
        Alm::saveQ();
        phase = PH_LIST; sel = 0;
      }
      break;
    case PH_NAME:
      if (tchar >= NCH) { finishName(); break; }
      tbuf[tpos++] = CS[tchar]; tbuf[tpos] = 0; tchar = 0;
      if (tpos >= Alm::NAMEL) finishName();
      break;
    case PH_DEL:
      if (mSel == 0) {
        for (int i = cur; i + 1 < n; i++) al[i] = al[i + 1];
        if (n) n--;
        memset(&al[n], 0, sizeof(Alm::Al));
        for (int i = 0; i <= Alm::MAXA; i++) Alm::dis[i] = 0;
        Alm::save();
        cur = 0; phase = PH_LIST; sel = 0;
      } else { phase = PH_EDIT; mSel = 6; }
      break;
  }
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d, int sz = 1) { d.setTextSize(sz); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
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
inline void fieldBar(Adafruit_GFX &d, int16_t y, const char *lab, const char *val, bool inv) {
  if (inv) d.fillRect(4, y, 192, 22, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  txt(d, lab, 10, y + 16);
  txt(d, val, 190 - 8 * (int)strlen(val), y + 16);
  d.setTextColor(qwInk());
}
inline void rowText(Adafruit_GFX &d, const char *s, int16_t y, bool inv) {
  if (inv) d.fillRect(4, y, 192, 22, qwInk());
  d.setTextColor(inv ? qwPaper() : qwInk());
  txt(d, s, 8, y + 16);
  d.setTextColor(qwInk());
}

inline void drawList(Adafruit_GFX &d) {
  char r[12];
  snprintf(r, sizeof(r), "%d/%d", n, Alm::MAXA);
  header(d, "ALARMS", r);
  setup13(d);
  int rows = rowsOf(), pg = sel / PERP, fa = firstAlarmRow();
  for (int k = 0; k < PERP; k++) {
    int i = pg * PERP + k;
    if (i >= rows) break;
    char b[32];
    if (i == 0) { if (quickActive()) snprintf(b, sizeof(b), "QUICK %02d:%02d       ON", Alm::q.h, Alm::q.m); else snprintf(b, sizeof(b), "QUICK ALARM (TODAY)"); }
    else if (i < fa) snprintf(b, sizeof(b), "+ NEW ALARM");
    else { const Alm::Al &a = al[i - fa]; snprintf(b, sizeof(b), "%02d:%02d %-10.10s %s", a.h, a.m, a.name, a.on ? " ON" : "OFF"); }
    rowText(d, b, 22 + k * 25, i == sel);
  }
  if (n == 0) { centered(d, "Add your first alarm.", 130); centered(d, "Vibrates in Morse code.", 148); }
}
inline void drawEdit(Adafruit_GFX &d) {
  const Alm::Al &a = al[cur < n ? cur : 0];
  char t[8], v[8];
  snprintf(t, sizeof(t), "%02d:%02d", a.h, a.m);
  header(d, a.name, t);
  setup13(d);
  bar(d, a.on ? "ALARM: ON" : "ALARM: OFF", 19, 22, mSel == 0);
  snprintf(v, sizeof(v), "%02d", a.h); fieldBar(d, 43, edit && mSel == 1 ? "HOUR  [edit]" : "HOUR", v, mSel == 1);
  snprintf(v, sizeof(v), "%02d", a.m); fieldBar(d, 67, edit && mSel == 2 ? "MINUTE  [edit]" : "MINUTE", v, mSel == 2);
  static const char L[7] = {'M', 'T', 'W', 'T', 'F', 'S', 'S'};
  if (mSel == 3) d.drawRect(4, 91, 192, 32, qwInk());
  for (int i = 0; i < 7; i++) {
    int x = 10 + i * 26;
    bool on = (a.days >> i) & 1;
    if (on) d.fillRect(x, 95, 22, 24, qwInk()); else d.drawRect(x, 95, 22, 24, qwInk());
    d.setTextColor(on ? qwPaper() : qwInk());
    char c[2] = {L[i], 0};
    txt(d, c, x + 7, 112);
    d.setTextColor(qwInk());
    if (edit && mSel == 3 && i == dayCur) d.drawRect(x - 2, 93, 26, 28, qwInk());
  }
  char b[28];
  snprintf(b, sizeof(b), "VIBRATION: %s", Alm::PATN[a.pat % 5]); bar(d, b, 127, 22, mSel == 4);
  snprintf(b, sizeof(b), "NAME: %s", a.name); bar(d, b, 151, 22, mSel == 5);
  bar(d, "DELETE", 175, 22, mSel == 6);
}
inline void drawQact(Adafruit_GFX &d) {
  char t[16], b[28];
  snprintf(t, sizeof(t), "QUICK %02d:%02d", Alm::q.h, Alm::q.m);
  header(d, t, nullptr);
  setup13(d);
  centered(d, "Rings today only.", 42);
  bar(d, "CHANGE TIME", 56, 26, mSel == 0);
  snprintf(b, sizeof(b), "VIBRATION: %s", Alm::PATN[Alm::q.pat % 5]); bar(d, b, 88, 26, mSel == 1);
  bar(d, "CANCEL ALARM", 120, 26, mSel == 2);
}
inline void drawQset(Adafruit_GFX &d) {
  header(d, "QUICK ALARM", nullptr);
  setup13(d);
  if (quickMax() < 1) { centered(d, "Too late for today.", 90); centered(d, "BACK: return", 120); return; }
  char b[28];
  int t = (int)qwGameMin + qd;
  if (t > 1439) t = 1439;
  setup13(d, 2);
  snprintf(b, sizeof(b), "IN %u MIN", qd);
  txt(d, b, (200 - 16 * (int)strlen(b)) / 2, 62);
  setup13(d);
  snprintf(b, sizeof(b), "TODAY AT %02d:%02d", t / 60, t % 60);
  centered(d, b, 100);
  centered(d, "UP/DOWN: change delay", 140);
  centered(d, "hold = faster", 158);
  centered(d, "MENU: set   BACK: cancel", 188);
}
inline void drawName(Adafruit_GFX &d) {
  header(d, "ALARM NAME", nullptr);
  setup13(d, 2);
  for (int i = 0; i < Alm::NAMEL; i++) {
    int16_t x = 20 + i * 16;
    char c[2] = {0, 0};
    bool c0 = (i == tpos);
    if (i < tpos) c[0] = tbuf[i];
    else if (c0) c[0] = tchar < NCH ? CS[tchar] : '>';
    if (c0) d.fillRect(x, 40, 16, 26, qwInk());
    d.setTextColor(c0 ? qwPaper() : qwInk());
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
  header(d, "DELETE ALARM?", nullptr);
  setup13(d);
  centered(d, al[cur < n ? cur : 0].name, 50);
  centered(d, "This cannot be undone.", 76);
  bar(d, "YES, DELETE", 96, 26, mSel == 0);
  bar(d, "NO", 128, 26, mSel == 1);
}
inline void draw(Adafruit_GFX &d) {
  Alm::load();
  d.fillScreen(qwPaper());
  switch (phase) {
    case PH_EDIT: drawEdit(d); break;
    case PH_NAME: drawName(d); break;
    case PH_DEL: drawDel(d); break;
    case PH_QACT: drawQact(d); break;
    case PH_QSET: drawQset(d); break;
    default: drawList(d);
  }
  d.setFont(nullptr);
}

}  // namespace Am
