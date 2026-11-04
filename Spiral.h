#pragma once
/*
  Spiral.h
  ------------------------------------------------------------
  Mathematical contemplation mode: an Ulam spiral of the prime
  numbers that reveals itself with time.

  Open it from the watch face with a long press (3 s) on the BACK
  button. BACK then returns to the watch face.

  How it works
    - 1 is at the centre, then 2, 3, 4... wind outwards in a square
      spiral. Every PRIME number gets a black dot, the others stay white.
    - The number line advances with time: ONE NUMBER PER MINUTE
      (see MINUTES_PER_NUMBER). A prime therefore appears about every
      4 to 9 minutes, less and less often as numbers grow - exactly like
      the primes themselves.
    - Stage 1: dots of 2x2 px, numbers up to 90 x 90 = 8100 (about 5.6 days).
      Stage 2: dots of 1x1 px, numbers up to 180 x 180 = 32400
      (about 22.5 days in total). The diagonal lines of the spiral
      are the famous feature to look for.
    - A small square marks the number reached "now".
    - Nothing is computed while the mode is closed: the number only
      depends on the time elapsed since the start (stored in flash),
      so it costs no battery.
    - In the mode, long press MENU (3 s) asks to restart from 1; a second
      long press confirms, any other button cancels.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"

namespace Sp {

constexpr uint32_t MINUTES_PER_NUMBER = 1;   // time pace
constexpr int AREA = 180;                    // spiral area, in px
constexpr int X0 = 10, Y0 = 16;              // its top-left corner
constexpr uint32_t MAX_BIG = 90UL * 90UL;    // 8100: end of the 2 px stage
constexpr uint32_t MAX_N = 180UL * 180UL;    // 32400: end of the 1 px stage

// One bit per number: 1 = composite. 4 KB, only used while drawing.
inline uint8_t composite[MAX_N / 8 + 2];

inline bool isComposite(uint32_t i) { return composite[i >> 3] & (1 << (i & 7)); }

inline void buildSieve(uint32_t limit) {
  memset(composite, 0, sizeof(composite));
  for (uint32_t i = 2; i * i <= limit; i++) {
    if (isComposite(i)) continue;
    for (uint32_t j = i * i; j <= limit; j += i) composite[j >> 3] |= (1 << (j & 7));
  }
}

// Number reached after `elapsedMin` minutes (1 .. MAX_N).
inline uint32_t numberAt(uint32_t nowMin, uint32_t startMin) {
  if (nowMin < startMin) return 1;
  uint32_t n = 1 + (nowMin - startMin) / MINUTES_PER_NUMBER;
  return n > MAX_N ? MAX_N : n;
}

// Draws the whole screen for number N. Returns the number of primes shown.
inline int drawSpiral(Adafruit_GFX &d, uint32_t N) {
  d.fillScreen(qwPaper());
  uint16_t ink = qwInk();
  const int cell = (N <= MAX_BIG) ? 2 : 1;
  const int cx = X0 + AREA / 2, cy = Y0 + AREA / 2;

  buildSieve(N);
  static const int8_t DX[4] = {1, 0, -1, 0};
  static const int8_t DY[4] = {0, 1, 0, -1};
  int x = 0, y = 0, dir = 0, seg = 1, left = 1;
  int primes = 0;
  int tipX = cx, tipY = cy;
  for (uint32_t n = 2; n <= N; n++) {
    x += DX[dir];
    y += DY[dir];
    if (--left == 0) {
      dir = (dir + 1) & 3;
      if ((dir & 1) == 0) seg++;
      left = seg;
    }
    int px = cx + x * cell, py = cy - y * cell;
    if (!isComposite(n)) {
      primes++;
      if (cell == 2) d.fillRect(px, py, 2, 2, ink);
      else d.drawPixel(px, py, ink);
    }
    tipX = px;
    tipY = py;
  }
  // marker of the number reached now
  if (cell == 2) d.drawRect(tipX - 2, tipY - 2, 6, 6, ink);
  else d.drawRect(tipX - 3, tipY - 3, 7, 7, ink);

  // header
  char h[40];
  if (N >= MAX_N) snprintf(h, sizeof(h), "COMPLETE  N=%lu  P=%d", (unsigned long)N, primes);
  else snprintf(h, sizeof(h), "N=%lu   PRIMES=%d", (unsigned long)N, primes);
  d.setTextColor(ink);
  qwSetText(d, 1);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(h), 1) / 2, 3);
  d.print(h);
  return primes;
}

// "Restart?" box, drawn over the spiral after a first long press.
inline void drawResetBox(Adafruit_GFX &d) {
  d.fillRect(12, 70, 176, 64, qwPaper());
  d.drawRect(12, 70, 176, 64, qwInk());
  d.drawRect(14, 72, 172, 60, qwInk());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(100 - qwTextInkWidth(8, 2) / 2, 80);
  d.print("RESTART?");
  qwSetText(d, 1);
  const char *a = "hold MENU 3 s = restart";
  const char *b = "any other button = cancel";
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(a), 1) / 2, 104);
  d.print(a);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(b), 1) / 2, 116);
  d.print(b);
}

}  // namespace Sp
