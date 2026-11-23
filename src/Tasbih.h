#pragma once
/*
  Tasbih.h
  ------------------------------------------------------------
  TASBIH: a geometric bead counter (long press UP on the watch face).

    MENU : one more bead              DOWN : remove the last bead
    UP   : change the goal (FREE, 33, 99, 100)
    BACK : leave. With beads counted it asks what to do with them:
           add them to the dhikr (today's flower), keep the count, or reset.

  The beads sit on the corners of a regular polygon: 1 bead, 2 beads, a
  triangle, a square... the more beads, the closer the polygon gets to a
  circle. After 100 beads a second ring starts inside the first (3 rings in
  all). The count is saved in flash at every press, so it is still there next
  time. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include "UiCommon.h"
#include "Storage.h"
#include "Garden.h"

namespace Tb {

constexpr int CMAX = 999;   // same maximum as the dhikr entry
constexpr int PER = 100;    // beads per ring
constexpr int RINGS = 3;
constexpr int16_t CX = 100, CY = 98, R0 = 76, RSTEP = 8;
constexpr float PI2 = 6.2831853f;
constexpr int MORPH_FRAMES = 3;
constexpr int PULSE_FRAMES = 3;

enum Phase : uint8_t { PH_COUNT = 0, PH_ASK };
enum Anim : uint8_t { A_NONE = 0, A_ADD, A_UNDO, A_GOAL };
enum Result : uint8_t { STAY = 0, TO_GARDEN, TO_FACE };

RTC_DATA_ATTR uint16_t count = 0;
RTC_DATA_ATTR uint8_t target = 0;    // 0 FREE, 1 = 33, 2 = 99, 3 = 100
RTC_DATA_ATTR uint8_t phase = PH_COUNT;
RTC_DATA_ATTR uint8_t askSel = 0;
RTC_DATA_ATTR uint8_t loaded = 0;
RTC_DATA_ATTR uint8_t anim = A_NONE;  // what main.cpp must animate before the next draw
RTC_DATA_ATTR uint32_t lastSec = 0;   // RTC second of the previous press

inline int goalValue() {
  static const int G[4] = {0, 33, 99, 100};
  return G[target & 3];
}

struct Rec {
  uint16_t count;
  uint8_t target, pad;
};

inline void saveRec() {
  Rec r = {count, target, 0};
  QWStorage::saveBlob("tasbih", &r, sizeof(r));
}

inline void open() {
  if (!loaded) {
    Rec r;
    if (QWStorage::loadBlob("tasbih", &r, sizeof(r)) && r.count <= CMAX && r.target < 4) {
      count = r.count;
      target = r.target;
    }
    loaded = 1;
  }
  phase = PH_COUNT;
  anim = A_NONE;
}

// ---------------- buttons ----------------
inline uint8_t select() {
  if (phase == PH_COUNT) {
    if (count < CMAX) {
      count++;
      saveRec();
      anim = (target && (int)count == goalValue()) ? A_GOAL : A_ADD;
    }
    return STAY;
  }
  if (askSel == 0 && count > 0) {  // into today's flower
    Gd::phase = Gd::PH_VIEW;
    Gd::sel = 0;
    Gd::pendingAdd = count;  // applied by main.cpp
    count = 0;
    saveRec();
    phase = PH_COUNT;
    return TO_GARDEN;
  }
  if (askSel == 2) count = 0;  // RESET
  saveRec();
  phase = PH_COUNT;
  return TO_FACE;
}

inline void button(int dir) {  // dir = +1 for UP, -1 for DOWN
  if (phase == PH_COUNT) {
    if (dir > 0) {
      target = (target + 1) & 3;
      saveRec();
    } else if (count > 0) {
      count--;
      saveRec();
      anim = A_UNDO;
    }
  } else {
    askSel = (uint8_t)((askSel + (dir < 0 ? 1 : 2)) % 3);
  }
}

inline uint8_t back() {
  if (phase == PH_ASK) {  // cancel: back to counting
    phase = PH_COUNT;
    return STAY;
  }
  if (count == 0) return TO_FACE;
  phase = PH_ASK;
  askSel = 0;
  return STAY;
}

// ---------------- geometry ----------------
// Angle of bead k on a ring holding c beads: always a vertex at the top, so
// one bead, then two beads (top and bottom, the middle is the centre), then a
// triangle... grow into each other without turning.
inline float ang(int c, int k) { return -1.5707963f + PI2 * (float)k / (float)(c < 1 ? 1 : c); }

inline float beadR(int c, int ring) {
  if (ring > 0) return 1.0f;
  if (c <= 1) return 7.0f;
  if (c <= 5) return 6.0f;
  if (c <= 8) return 5.0f;
  if (c <= 14) return 4.0f;
  if (c <= 25) return 3.0f;
  if (c <= 60) return 2.0f;
  return 1.0f;
}

inline float shortest(float d) {
  while (d > 3.14159265f) d -= PI2;
  while (d <= -3.14159265f) d += PI2;
  return d;
}

inline void bead(Adafruit_GFX &d, float x, float y, float r) {
  int ri = (int)lroundf(r);
  uint16_t ink = qwInk();
  if (ri >= 1) d.fillCircle((int16_t)lroundf(x), (int16_t)lroundf(y), ri, ink);
  else if (r > 0.3f) d.drawPixel((int16_t)lroundf(x), (int16_t)lroundf(y), ink);
}

// One ring going from a beads to b beads (a <= b) at progress e (0..1).
inline void drawRing(Adafruit_GFX &d, int ring, int a, int b, float e) {
  float R = (float)(R0 - RSTEP * ring);
  float px = 0, py = 0, fx = 0, fy = 0;
  bool edges = (ring == 0 && b >= 3 && b <= 40);
  for (int k = 0; k < b; k++) {
    float a1 = ang(b, k), r1 = beadR(b, ring);
    float a0, r0;
    if (k < a) {
      a0 = ang(a, k);
      r0 = beadR(a, ring);
    } else {  // a new bead leaves the position of the last old one
      a0 = (a > 0) ? ang(a, a - 1) : -1.5707963f;
      r0 = 0;
    }
    float aa = a0 + shortest(a1 - a0) * e;
    float rr = r0 + (r1 - r0) * e;
    float x = CX + R * cosf(aa), y = CY + R * sinf(aa);
    if (edges) {
      if (k == 0) { fx = x; fy = y; }
      else d.drawLine((int16_t)lroundf(px), (int16_t)lroundf(py), (int16_t)lroundf(x), (int16_t)lroundf(y), qwInk());
      px = x;
      py = y;
    }
    bead(d, x, y, rr);
  }
  if (edges) d.drawLine((int16_t)lroundf(px), (int16_t)lroundf(py), (int16_t)lroundf(fx), (int16_t)lroundf(fy), qwInk());
}

// All rings for a count going from c0 to c1 at progress e (c1 < c0 plays backwards).
inline void drawBeads(Adafruit_GFX &d, int c0, int c1, float e) {
  if (c1 < c0) {
    int t = c0;
    c0 = c1;
    c1 = t;
    e = 1.0f - e;
  }
  for (int r = 0; r < RINGS; r++) {
    int a = c0 - PER * r, b = c1 - PER * r;
    a = a < 0 ? 0 : (a > PER ? PER : a);
    b = b < 0 ? 0 : (b > PER ? PER : b);
    if (b == 0) continue;
    drawRing(d, r, a, b, a == b ? 1.0f : e);
  }
}

inline void centered(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  int16_t w = qwTextWidth((uint8_t)strlen(t), size) - size;
  d.setCursor(CX - w / 2, y);
  d.print(t);
}

inline void number(Adafruit_GFX &d, int n) {
  char b[8];
  snprintf(b, sizeof(b), "%d", n);
  uint8_t size = (n >= 100) ? 4 : 5;
  d.setTextColor(qwInk());
  centered(d, b, CY - (7 * size) / 2, size);
}

// ---------------- drawing ----------------
inline void drawCount(Adafruit_GFX &d, int c0, int c1, float e, int ripple) {
  d.fillScreen(qwPaper());
  drawBeads(d, c0, c1, e);
  if (ripple > 0) d.drawCircle(CX, CY, R0 + 3 + 4 * ripple, qwInk());
  number(d, c1);
  d.setTextColor(qwInk());
  char b[24];
  int g = goalValue();
  if (!g) snprintf(b, sizeof(b), "FREE");
  else if ((int)count >= g) snprintf(b, sizeof(b), "GOAL %d DONE", g);
  else snprintf(b, sizeof(b), "GOAL %d", g);
  centered(d, b, CY + 26, 1);
  centered(d, "MENU +1  DOWN -1  UP GOAL", 188, 1);
}

inline void drawAsk(Adafruit_GFX &d) {
  static const char *const NAMES[3] = {"ADD TO DHIKR", "KEEP COUNT", "RESET COUNT"};
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[24];
  snprintf(b, sizeof(b), "%d BEADS", count);
  centered(d, b, 14, 3);
  centered(d, "WHAT NEXT?", 52, 2);
  for (int i = 0; i < 3; i++) {
    int16_t y = 84 + i * 30;
    if (i == askSel) {
      d.fillRect(10, y - 5, 180, 26, qwInk());
      d.setTextColor(qwPaper());
    } else {
      d.setTextColor(qwInk());
    }
    qwSetText(d, 2);
    d.setCursor(CX - (qwTextWidth((uint8_t)strlen(NAMES[i]), 2) - 2) / 2, y);
    d.print(NAMES[i]);
  }
  d.setTextColor(qwInk());
  centered(d, "UP/DOWN pick  MENU ok", 174, 1);
  centered(d, "BACK = keep counting", 186, 1);
}

inline void draw(Adafruit_GFX &d) {
  if (phase == PH_ASK) drawAsk(d);
  else drawCount(d, count, count, 1.0f, 0);
}

// ---------------- the calm transition ----------------
inline int animFrames(bool quick) {
  if (anim == A_NONE || phase != PH_COUNT) return 0;
  return (quick ? 0 : MORPH_FRAMES) + (anim == A_GOAL ? PULSE_FRAMES : 0);
}

inline void drawAnim(Adafruit_GFX &d, int f, bool quick) {
  int morph = quick ? 0 : MORPH_FRAMES;
  if (f < morph) {
    float t = (float)(f + 1) / (float)(MORPH_FRAMES + 1);
    float e = 1.0f - (1.0f - t) * (1.0f - t);  // slows down on arrival
    int prev = (anim == A_UNDO) ? (int)count + 1 : (int)count - 1;
    drawCount(d, prev, count, e, 0);
  } else {
    drawCount(d, count, count, 1.0f, f - morph + 1);  // the goal ripple
  }
}

}  // namespace Tb
