#pragma once
/*
  Tea.h
  ------------------------------------------------------------
  TEA TIMER - an app of the APPS menu.

  LIST     Green 2:00 / White 3:00 / Oolong 4:00 / Black 4:00 / Herbal 6:00 /
           Rooibos 6:00 with the water temperature, and CUSTOM.
  SET      UP/DOWN = +/- 15 seconds (hold = faster), MENU = start the timer.
           The duration you chose is remembered for each tea.
  BREWING  end time and minutes left (as of the last screen refresh);
           MENU = cancel (asks), BACK = leave (the timer keeps running).
  When the tea is ready the watch vibrates, the cup picture appears in the
  info zone of the watch face, and it vibrates once more a minute later
  unless BACK was pressed on the watch face (or here).
*/
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "TeaCore.h"
#include "NotoMono13.h"

namespace Te {

enum Ph : uint8_t { PH_LIST = 0, PH_SET, PH_RUN, PH_CANCEL };
RTC_DATA_ATTR uint8_t phase = PH_LIST, sel = 0, mSel = 1;

inline int mulT() { int s = qwGameSteps(); return s == 1 ? 1 : (s == 3 ? 4 : 12); }   // x 15 s

inline void start() {
  Tea::load();
  phase = (Tea::state != 0) ? PH_RUN : PH_LIST;
  if (Tea::state != 0 && Tea::idx < Tea::N) sel = Tea::idx;
}
inline bool busy() { return false; }
inline void stopTimer() { Tea::state = 0; Tea::buzz = 0; }
inline bool back() {
  Tea::load();
  switch (phase) {
    case PH_SET: Tea::saveDur(); phase = PH_LIST; return true;
    case PH_RUN: if (Tea::state == 2) stopTimer(); phase = PH_LIST; return true;
    case PH_CANCEL: phase = PH_RUN; return true;
    default: return false;
  }
}
inline void up() {
  Tea::load();
  if (phase == PH_LIST) sel = (uint8_t)((sel + Tea::N - qwGameSteps() % Tea::N) % Tea::N);
  else if (phase == PH_SET) {
    int v = Tea::dur[sel] + mulT() * Tea::STEP;
    Tea::dur[sel] = (uint16_t)(v > Tea::MAXS ? Tea::MAXS : v);
  } else if (phase == PH_CANCEL) mSel ^= 1;
}
inline void down() {
  Tea::load();
  if (phase == PH_LIST) sel = (uint8_t)((sel + qwGameSteps()) % Tea::N);
  else if (phase == PH_SET) {
    int v = Tea::dur[sel] - mulT() * Tea::STEP;
    Tea::dur[sel] = (uint16_t)(v < Tea::MINS ? Tea::MINS : v);
  } else if (phase == PH_CANCEL) mSel ^= 1;
}
inline void select() {
  Tea::load();
  switch (phase) {
    case PH_LIST: if (sel >= Tea::N) sel = 0; phase = PH_SET; break;
    case PH_SET:
      Tea::saveDur();
      Tea::idx = sel;
      Tea::end = qwGameNow + Tea::dur[sel];
      Tea::state = 1; Tea::buzz = 0;
      phase = PH_RUN;
      break;
    case PH_RUN:
      if (Tea::state == 2) { stopTimer(); phase = PH_LIST; }
      else if (Tea::state == 1) { phase = PH_CANCEL; mSel = 1; }
      break;
    case PH_CANCEL:
      if (mSel == 0) { stopTimer(); phase = PH_LIST; } else phase = PH_RUN;
      break;
  }
}

// ---------------------------------------------------------------- drawing
inline void setup13(Adafruit_GFX &d, int sz = 1) { d.setTextSize(sz); d.setTextColor(qwInk()); d.setFont(&NotoMono13); }
inline void txt(Adafruit_GFX &d, const char *s, int16_t x, int16_t base) { d.setCursor(x, base); d.print(s); }
inline void centered(Adafruit_GFX &d, const char *s, int16_t base, int cw = 8) { txt(d, s, (200 - cw * (int)strlen(s)) / 2, base); }
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
inline void tempStr(char *b, size_t n, int i) {
  if (Tea::TEMP[i]) snprintf(b, n, "%dC", Tea::TEMP[i]); else b[0] = 0;
}

inline void drawList(Adafruit_GFX &d) {
  header(d, "TEA TIMER", Tea::state == 1 ? "BREWING" : nullptr);
  setup13(d);
  for (int i = 0; i < Tea::N; i++) {
    int y = 22 + i * 25;
    char t[6], b[32];
    tempStr(t, sizeof(t), i);
    snprintf(b, sizeof(b), "%-8s%2d:%02d %4s", Tea::NAME[i], Tea::dur[i] / 60, Tea::dur[i] % 60, t);
    if (i == sel) d.fillRect(4, y, 192, 22, qwInk());
    d.setTextColor(i == sel ? qwPaper() : qwInk());
    txt(d, b, 8, y + 16);
    d.setTextColor(qwInk());
  }
}
inline void drawSet(Adafruit_GFX &d) {
  char t[6], b[24];
  tempStr(t, sizeof(t), sel);
  header(d, Tea::NAME[sel], Tea::TEMP[sel] ? t : nullptr);
  setup13(d, 3);
  snprintf(b, sizeof(b), "%d:%02d", Tea::dur[sel] / 60, Tea::dur[sel] % 60);
  centered(d, b, 88, 24);
  setup13(d);
  if (Tea::TEMP[sel]) { snprintf(b, sizeof(b), "WATER %d C", Tea::TEMP[sel]); centered(d, b, 118); }
  centered(d, "UP/DOWN: +/- 15 s", 144);
  centered(d, "hold = faster", 162);
  centered(d, "MENU: start  BACK: list", 188);
}
inline void drawRun(Adafruit_GFX &d) {
  int i = Tea::idx < Tea::N ? Tea::idx : 0;
  header(d, Tea::NAME[i], nullptr);
  setup13(d);
  if (Tea::state == 2) {
    setup13(d, 2);
    centered(d, "TEA READY", 80, 16);
    setup13(d);
    centered(d, "Enjoy your tea.", 118);
    centered(d, "MENU / BACK: stop", 188);
    return;
  }
  char b[28];
  uint32_t tod = Tea::end % 86400u;
  int left = Tea::end > qwGameNow ? (int)(Tea::end - qwGameNow) : 0;
  centered(d, "READY AT", 44);
  setup13(d, 2);
  snprintf(b, sizeof(b), "%02u:%02u:%02u", (unsigned)(tod / 3600), (unsigned)(tod / 60 % 60), (unsigned)(tod % 60));
  centered(d, b, 82, 16);
  setup13(d);
  snprintf(b, sizeof(b), "ABOUT %d MIN LEFT", (left + 59) / 60);
  centered(d, b, 116);
  centered(d, "Vibrates when ready.", 144);
  centered(d, "BACK keeps it running.", 162);
  centered(d, "MENU: cancel timer", 188);
}
inline void drawCancel(Adafruit_GFX &d) {
  header(d, "CANCEL TIMER?", nullptr);
  setup13(d);
  centered(d, Tea::NAME[Tea::idx < Tea::N ? Tea::idx : 0], 50);
  bar(d, "YES, CANCEL", 70, 26, mSel == 0);
  bar(d, "NO", 102, 26, mSel == 1);
}
inline void draw(Adafruit_GFX &d) {
  Tea::load();
  if ((phase == PH_RUN || phase == PH_CANCEL) && Tea::state == 0) phase = PH_LIST;
  if (phase == PH_CANCEL && Tea::state == 2) phase = PH_RUN;
  d.fillScreen(qwPaper());
  switch (phase) {
    case PH_SET: drawSet(d); break;
    case PH_RUN: drawRun(d); break;
    case PH_CANCEL: drawCancel(d); break;
    default: drawList(d);
  }
  d.setFont(nullptr);
}

}  // namespace Te
