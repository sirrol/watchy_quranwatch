#pragma once
/*
  Garden.h
  ------------------------------------------------------------
  DHIKR BLOOM: a heart protected by a ring of flowers, one flower per day
  on which you did some dhikr.

  Open it from the watch face with a short press on DOWN (bottom-right).
    MENU    : open the list (ADD 33, ADD 99, ADD 100, ADD FREE, UNDO LAST)
    UP/DOWN : move in the list / change the number (hold = faster).
              On the main screen: DOWN = the trophies page.
    MENU    : confirm (the flower of the day opens)
    BACK    : back one step, then back to the watch face

  Rules (24 h periods on the raw RTC, so changing the clock never matters)
    - Each new day with an entry (at least 24 h after the previous counted
      day) adds a flower around the heart: 1 flower, 2 opposite, 3 in a
      triangle, 4 in a square... up to 7 (a heptagon).
    - The number in the middle of each flower is that day's dhikr total.
      Every time you add dhikr that flower gets more petals and new
      "colours". There are 90 species (daisy, rose, tulip, hydrangea...).
    - With 7 flowers the heart is fully protected: a heart-shaped envelope
      surrounds it. It disappears after 36 h without dhikr.
    - When the ring is full and a new day starts, the oldest flower leaves
      the frame (it goes to the TROPHIES page, up to TMAX of them) and the
      new flower opens in its place.
    - 24 h without dhikr: the oldest flower of the ring loses one petal
      every 6 h. When its last petal is gone it dies (it is not a trophy)
      and the next oldest flower is next. Adding dhikr gives all petals
      back (a dead flower stays dead).
    - UNDO LAST restores everything as it was before the last entry.

  Everything is stored in flash (Storage.h). Nothing runs while the screen
  is closed: the picture is computed from the record and the current time.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include "UiCommon.h"
#include "Storage.h"

namespace Gd {

constexpr uint32_t DAY_MIN = 1440;     // a day, in minutes
constexpr uint32_t PETAL_MIN = 360;    // one petal falls every 6 h once wilting
constexpr uint32_t ENV_MIN = 2160;     // the envelope lasts 36 h
constexpr int FLOWERS = 7;             // flowers around the heart
constexpr int TMAX = 56;               // trophies kept (8 x 7 on the page)
constexpr int NUM_MAX = 999;
constexpr int ANIM_FRAMES = 3;         // the flower of the day opens
constexpr int MOVE_FRAMES = 3;         // the oldest flower leaves the frame

// ---------------- drawing ----------------
constexpr int16_t CX = 100, CY = 114;  // centre of the whole drawing
constexpr float RING = 62.0f;          // radius of the circle of tiles
constexpr float TILE_MAX = 26.0f;      // largest tile radius
constexpr float PI_F = 3.14159265f;

// Arabic word (dhikr), 21 x 16, 1 bit, rendered offline from Scheherazade New (SIL OFL 1.1)
constexpr int ZIKR_W = 21, ZIKR_H = 16;
static const uint8_t ZIKR[] PROGMEM = {
  0x00,0x07,0x00,0x00,0x3E,0x00,0x00,0xFE,0x60,0x01,0xF0,0x60,0x03,0x80,0x60,0x03,0xC0,0x70,0x01,0xE0,0xF0,0x00,0xF0,0x78,0x00,0x70,0x18,0x0F,0xF1,0xF8,0x0F,0xE1,0xF8,0x0F,0xC1,0xE0,0x0C,0x00,0x00,0x1C,0x00,0x00,0xFC,0x00,0x00,0xF8,0x00,0x00};

inline float h01(uint32_t seed, uint32_t i) {
  uint32_t x = seed ^ (i * 2654435761u);
  x ^= x >> 15;
  x *= 2246822519u;
  x ^= x >> 13;
  x *= 3266489917u;
  x ^= x >> 16;
  return (x & 0xFFFFFF) / 16777216.0f;
}

// The heart: a solid heart drawn from the heart curve
// (x^2 + y^2 - 1)^3 - x^2 y^3 <= 0, with the word
// DHIKR in paper colour in the middle. `scale` makes it beat (1.0 = normal).
inline bool heartIn(int px, int py, float S) {
  float x = (px - CX) / S, y = -(py - CY) / S + 0.125f;
  float a = x * x + y * y - 1.0f;
  return a * a * a - x * x * y * y * y <= 0;
}

inline void drawHeart(Adafruit_GFX &d, float scale) {
  uint16_t ink = qwInk(), paper = qwPaper();
  const float S = 22.0f * scale;
  int ext = (int)(1.3f * S) + 1;
  for (int py = CY - ext; py <= CY + ext; py++)
    for (int px = CX - ext; px <= CX + ext; px++)
      if (heartIn(px, py, S)) d.drawPixel(px, py, ink);
  d.drawBitmap(CX - 10, CY - 9, ZIKR, ZIKR_W, ZIKR_H, paper);
}


// The protective envelope: a heart-shaped line around the heart.
inline void drawEnvelope(Adafruit_GFX &d, float scale) {
  uint16_t ink = qwInk();
  const float So = 28.0f * (1.0f + (scale - 1.0f) * 0.5f), Si = So * 0.935f;
  int ext = (int)(1.3f * So) + 1;
  for (int py = CY - ext; py <= CY + ext; py++)
    for (int px = CX - ext; px <= CX + ext; px++)
      if (heartIn(px, py, So) && !heartIn(px, py, Si)) d.drawPixel(px, py, ink);
}

// ---- the flower of a day ----
// Ninety species, drawn with polygons filled with 1-bit "colours"
// (paper, solid, dots, checker). The species of a day is fixed
// (it depends on the cycle and the day), but every time dhikr is added the
// flower gets MORE PETALS and new colours.
constexpr int NSP = 90;
enum { SP_HYDRANGEA = 13 };

// "colours" of a monochrome screen: paper, solid, light dots, checker
inline bool patInk(int lv, int x, int y) {
  switch (lv) {
    case 1: return true;                                  // solid
    case 2: return ((x & 1) == 0) && ((y & 1) == 0);      // light dots
    case 3: return ((x + y) & 1) == 0;                    // checker (mid grey)
    default: return false;                                // paper
  }
}

// Fills a polygon (even-odd rule) with a pattern.
inline void polyFill(Adafruit_GFX &d, const int16_t *X, const int16_t *Y, int n, int lv) {
  uint16_t ink = qwInk(), paper = qwPaper();
  int y0 = Y[0], y1 = Y[0];
  for (int i = 1; i < n; i++) {
    if (Y[i] < y0) y0 = Y[i];
    if (Y[i] > y1) y1 = Y[i];
  }
  for (int y = y0; y <= y1; y++) {
    int xs[26], m = 0;
    for (int i = 0; i < n; i++) {
      int j = (i + 1) % n, ya = Y[i], yb = Y[j];
      if (ya == yb) continue;
      int lo = ya < yb ? ya : yb, hi = ya < yb ? yb : ya;
      if (y < lo || y >= hi) continue;
      xs[m++] = (int)lroundf(X[i] + (float)(X[j] - X[i]) * (float)(y - ya) / (float)(yb - ya));
    }
    for (int a = 1; a < m; a++) {  // insertion sort
      int v = xs[a], b = a - 1;
      while (b >= 0 && xs[b] > v) { xs[b + 1] = xs[b]; b--; }
      xs[b + 1] = v;
    }
    for (int k = 0; k + 1 < m; k += 2)
      for (int x = xs[k]; x <= xs[k + 1]; x++) d.drawPixel(x, y, patInk(lv, x, y) ? ink : paper);
  }
}

// Species: petal shape, tip notch, petals in the outer layer, layers, motif,
// layer rotation mode, edge frill, width factor, bend, length of each layer.
struct Sp {
  uint8_t shape, notch, base, layers, motif, rot;
  float frill, wf, bend, lenF[4];
};
static const Sp SPECIES[NSP] = {
  /* 0 daisy      */ {1, 0, 8, 1, 1, 0, 0.00f, 0.70f, 0.00f, {1.0f, 0, 0, 0}},
  /* 1 sunflower  */ {0, 0, 8, 2, 4, 0, 0.00f, 0.95f, 0.00f, {1.0f, 0.72f, 0, 0}},
  /* 2 plumeria   */ {2, 0, 5, 1, 3, 0, 0.00f, 1.00f, 0.90f, {1.0f, 0, 0, 0}},
  /* 3 poppy      */ {2, 0, 4, 2, 3, 0, 0.00f, 1.25f, 0.25f, {1.0f, 0.70f, 0, 0}},
  /* 4 pansy      */ {2, 0, 5, 1, 6, 0, 0.00f, 1.15f, 0.00f, {1.0f, 0, 0, 0}},
  /* 5 tulip      */ {0, 1, 5, 3, 1, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0.75f, 0.50f, 0}},
  /* 6 marigold   */ {2, 0, 7, 3, 2, 0, 0.18f, 1.00f, 0.00f, {1.0f, 0.72f, 0.46f, 0}},
  /* 7 dahlia     */ {0, 0, 6, 3, 1, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0.74f, 0.48f, 0}},
  /* 8 lily       */ {3, 0, 6, 1, 3, 0, 0.12f, 1.00f, 0.60f, {1.0f, 0, 0, 0}},
  /* 9 peony      */ {2, 0, 5, 3, 2, 1, 0.15f, 1.15f, 0.15f, {1.0f, 0.74f, 0.50f, 0}},
  /* 10 slender   */ {1, 0, 7, 2, 0, 2, 0.00f, 0.38f, 0.00f, {1.0f, 1.0f, 0, 0}},
  /* 11 clover    */ {2, 0, 4, 2, 2, 0, 0.10f, 1.20f, 0.00f, {1.0f, 0.60f, 0, 0}},
  /* 12 rose      */ {2, 0, 6, 4, 5, 1, 0.10f, 1.10f, 0.30f, {1.0f, 0.78f, 0.58f, 0.40f}},
  /* 13 hydrangea */ {2, 0, 4, 1, 0, 0, 0.00f, 1.30f, 0.00f, {1.0f, 0, 0, 0}},  // florets
  /* 14 sakura    */ {2, 1, 5, 1, 4, 0, 0.00f, 1.10f, 0.00f, {1.0f, 0, 0, 0}},  // notched tips, stamens
  /* 15 aster     */ {0, 0, 11, 3, 0, 2, 0.00f, 0.30f, 0.00f, {1.0f, 0.78f, 0.56f, 0}},  // fringe of thin petals
  /* 16 swirl     */ {2, 0, 5, 1, 0, 0, 0.00f, 1.30f, 1.10f, {1.0f, 0, 0, 0}},  // plum-blossom pinwheel
  /* 17 heart     */ {2, 1, 5, 1, 1, 0, 0.00f, 1.25f, 0.00f, {1.0f, 0, 0, 0}},  // heart-shaped petals
  /* 18 cosmos    */ {3, 1, 8, 1, 1, 0, 0.08f, 1.00f, 0.00f, {1.0f, 0, 0, 0}},
  /* 19 camellia  */ {5, 0, 5, 4, 0, 1, 0.00f, 1.20f, 0.20f, {1.0f, 0.80f, 0.60f, 0.42f}},
  /* 20 iris      */ {3, 0, 3, 2, 3, 0, 0.15f, 1.10f, 0.50f, {1.0f, 0.60f, 0, 0}},
  /* 21 hibiscus  */ {2, 0, 5, 1, 5, 0, 0.20f, 1.30f, 0.40f, {1.0f, 0, 0, 0}},
  /* 22 lotus     */ {0, 0, 8, 3, 1, 0, 0.00f, 0.80f, 0.00f, {1.0f, 0.78f, 0.55f, 0}},
  /* 23 waterlily */ {0, 0, 10, 2, 0, 0, 0.00f, 0.65f, 0.00f, {1.0f, 0.70f, 0, 0}},
  /* 24 jasmine   */ {0, 0, 5, 1, 0, 0, 0.00f, 0.55f, 0.00f, {1.0f, 0, 0, 0}},
  /* 25 anemone   */ {1, 0, 6, 1, 4, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0, 0, 0}},
  /* 26 gerbera   */ {1, 0, 12, 2, 0, 2, 0.00f, 0.32f, 0.00f, {1.0f, 0.85f, 0, 0}},
  /* 27 chrysanth */ {1, 0, 9, 4, 0, 2, 0.00f, 0.40f, 0.00f, {1.0f, 0.85f, 0.70f, 0.55f}},
  /* 28 carnation */ {2, 0, 7, 3, 0, 1, 0.35f, 1.10f, 0.00f, {1.0f, 0.80f, 0.60f, 0}},
  /* 29 magnolia  */ {3, 0, 6, 2, 1, 0, 0.00f, 1.00f, 0.70f, {1.0f, 0.75f, 0, 0}},
  /* 30 daffodil  */ {3, 0, 6, 2, 2, 0, 0.00f, 1.10f, 0.00f, {1.0f, 0.40f, 0, 0}},
  /* 31 buttercup */ {5, 0, 5, 1, 0, 0, 0.00f, 1.35f, 0.00f, {1.0f, 0, 0, 0}},
  /* 32 clematis  */ {4, 0, 6, 1, 1, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0, 0, 0}},
  /* 33 edelweiss */ {4, 0, 9, 2, 0, 2, 0.00f, 0.70f, 0.00f, {1.0f, 0.60f, 0, 0}},
  /* 34 petunia   */ {5, 0, 5, 1, 3, 0, 0.25f, 1.20f, 0.00f, {1.0f, 0, 0, 0}},
  /* 35 zinnia    */ {2, 0, 8, 4, 0, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0.80f, 0.60f, 0.40f}},
  /* 36 primrose  */ {7, 1, 5, 1, 3, 0, 0.00f, 1.30f, 0.00f, {1.0f, 0, 0, 0}},
  /* 37 crocus    */ {6, 0, 3, 2, 1, 0, 0.00f, 1.10f, 0.15f, {1.0f, 0.70f, 0, 0}},
  /* 38 snowdrop  */ {6, 0, 3, 2, 0, 0, 0.00f, 1.00f, 0.90f, {1.0f, 0.60f, 0, 0}},
  /* 39 bluebell  */ {6, 0, 6, 1, 0, 0, 0.10f, 0.90f, 1.10f, {1.0f, 0, 0, 0}},
  /* 40 orchid    */ {2, 0, 3, 2, 2, 0, 0.12f, 1.40f, 0.30f, {1.0f, 0.60f, 0, 0}},
  /* 41 delphin.  */ {6, 0, 5, 2, 4, 1, 0.00f, 1.00f, 0.00f, {1.0f, 0.75f, 0, 0}},
  /* 42 cornflower*/ {3, 1, 9, 1, 0, 2, 0.25f, 0.60f, 0.00f, {1.0f, 0, 0, 0}},
  /* 43 sweet pea */ {7, 0, 4, 2, 3, 0, 0.15f, 1.20f, 0.20f, {1.0f, 0.70f, 0, 0}},
  /* 44 violet    */ {5, 0, 5, 1, 6, 0, 0.00f, 1.15f, 0.00f, {1.0f, 0, 0, 0}},
  /* 45 peach     */ {5, 1, 5, 1, 0, 0, 0.00f, 1.15f, 0.00f, {1.0f, 0, 0, 0}},
  /* 46 apple     */ {2, 0, 5, 1, 3, 0, 0.00f, 1.20f, 0.30f, {1.0f, 0, 0, 0}},
  /* 47 geranium  */ {7, 0, 5, 1, 3, 0, 0.00f, 1.25f, 0.00f, {1.0f, 0, 0, 0}},
  /* 48 nasturt.  */ {7, 0, 5, 2, 6, 1, 0.10f, 1.30f, 0.00f, {1.0f, 0.65f, 0, 0}},
  /* 49 amaryllis */ {6, 0, 6, 2, 1, 0, 0.00f, 1.10f, 0.50f, {1.0f, 0.85f, 0, 0}},
  /* 50 protea    */ {0, 0, 12, 3, 0, 1, 0.00f, 0.45f, 0.00f, {1.0f, 0.80f, 0.55f, 0}},
  /* 51 dandelion */ {1, 0, 14, 3, 0, 2, 0.00f, 0.22f, 0.00f, {1.0f, 0.85f, 0.70f, 0}},
  /* 52 ranuncul. */ {5, 0, 6, 4, 0, 1, 0.00f, 1.15f, 0.00f, {1.0f, 0.85f, 0.70f, 0.55f}},
  /* 53 poinsett. */ {0, 0, 6, 2, 1, 0, 0.00f, 1.15f, 0.00f, {1.0f, 0.70f, 0, 0}},
  /* 54 freesia    */ {9, 0, 6, 1, 1, 0, 0.00f, 0.90f, 0.70f, {1.0f, 0, 0, 0}},
  /* 55 lilac      */ {8, 0, 4, 1, 0, 0, 0.00f, 1.20f, 0.00f, {1.0f, 0, 0, 0}},
  /* 56 forsythia  */ {8, 0, 4, 1, 3, 0, 0.10f, 1.00f, 0.30f, {1.0f, 0, 0, 0}},
  /* 57 jonquil    */ {9, 0, 6, 2, 2, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0.40f, 0, 0}},
  /* 58 hellebore  */ {5, 0, 5, 1, 6, 0, 0.15f, 1.30f, 0.20f, {1.0f, 0, 0, 0}},
  /* 59 gardenia   */ {2, 0, 6, 3, 1, 1, 0.12f, 1.10f, 0.40f, {1.0f, 0.78f, 0.55f, 0}},
  /* 60 fuchsia    */ {6, 0, 4, 2, 4, 0, 0.10f, 0.80f, 1.00f, {1.0f, 0.60f, 0, 0}},
  /* 61 hyacinth   */ {8, 0, 6, 2, 0, 1, 0.00f, 0.90f, 0.80f, {1.0f, 0.75f, 0, 0}},
  /* 62 verbena    */ {7, 1, 5, 1, 0, 0, 0.00f, 1.00f, 0.00f, {1.0f, 0, 0, 0}},
  /* 63 phlox      */ {7, 0, 5, 1, 1, 0, 0.00f, 1.15f, 0.00f, {1.0f, 0, 0, 0}},
  /* 64 lantana    */ {5, 0, 4, 2, 0, 0, 0.00f, 1.25f, 0.00f, {1.0f, 0.60f, 0, 0}},
  /* 65 begonia    */ {2, 0, 4, 3, 2, 0, 0.10f, 1.20f, 0.00f, {1.0f, 0.70f, 0.45f, 0}},
  /* 66 azalea     */ {2, 0, 5, 1, 5, 0, 0.30f, 1.30f, 0.50f, {1.0f, 0, 0, 0}},
  /* 67 rhododend. */ {3, 0, 5, 2, 3, 0, 0.25f, 1.20f, 0.50f, {1.0f, 0.70f, 0, 0}},
  /* 68 morn. glory*/ {7, 0, 5, 1, 6, 0, 0.00f, 1.50f, 0.00f, {1.0f, 0, 0, 0}},
  /* 69 periwinkle */ {7, 0, 5, 1, 0, 0, 0.00f, 1.00f, 0.50f, {1.0f, 0, 0, 0}},
  /* 70 nigella    */ {0, 0, 5, 2, 0, 1, 0.00f, 0.60f, 0.00f, {1.0f, 0.90f, 0, 0}},
  /* 71 scabiosa   */ {3, 0, 10, 2, 0, 2, 0.20f, 0.50f, 0.00f, {1.0f, 0.70f, 0, 0}},
  /* 72 echinacea  */ {9, 0, 10, 1, 1, 0, 0.00f, 0.50f, 0.60f, {1.0f, 0, 0, 0}},
  /* 73 rudbeckia  */ {0, 0, 12, 1, 0, 0, 0.00f, 0.60f, 0.00f, {1.0f, 0, 0, 0}},
  /* 74 helianthus */ {0, 0, 13, 2, 4, 0, 0.00f, 0.70f, 0.00f, {1.0f, 0.70f, 0, 0}},
  /* 75 campanula  */ {6, 1, 5, 1, 0, 0, 0.00f, 1.10f, 0.20f, {1.0f, 0, 0, 0}},
  /* 76 lisianthus */ {2, 0, 5, 4, 0, 1, 0.00f, 1.00f, 0.30f, {1.0f, 0.82f, 0.64f, 0.46f}},
  /* 77 dianthus   */ {3, 1, 5, 1, 0, 0, 0.40f, 0.90f, 0.00f, {1.0f, 0, 0, 0}},
  /* 78 snapdragon */ {8, 1, 4, 2, 2, 0, 0.00f, 1.20f, 0.00f, {1.0f, 0.60f, 0, 0}},
  /* 79 marguerite */ {8, 0, 12, 1, 0, 0, 0.00f, 0.45f, 0.00f, {1.0f, 0, 0, 0}},
  /* 80 chamomile  */ {8, 0, 13, 1, 0, 0, 0.00f, 0.40f, 0.00f, {1.0f, 0, 0, 0}},
  /* 81 daylily    */ {9, 0, 6, 1, 3, 0, 0.10f, 1.00f, 0.80f, {1.0f, 0, 0, 0}},
  /* 82 tiger lily */ {9, 0, 6, 2, 3, 0, 0.20f, 0.90f, 1.10f, {1.0f, 0.60f, 0, 0}},
  /* 83 gladiolus  */ {9, 0, 6, 3, 0, 0, 0.00f, 1.00f, 0.50f, {1.0f, 0.70f, 0.45f, 0}},
  /* 84 bougainv.  */ {7, 0, 3, 1, 1, 0, 0.00f, 1.40f, 0.00f, {1.0f, 0, 0, 0}},
  /* 85 oleander   */ {5, 0, 5, 1, 3, 0, 0.00f, 1.20f, 0.60f, {1.0f, 0, 0, 0}},
  /* 86 hollyhock  */ {7, 1, 5, 2, 2, 0, 0.20f, 1.30f, 0.00f, {1.0f, 0.65f, 0, 0}},
  /* 87 tree peony */ {5, 0, 7, 4, 2, 1, 0.10f, 1.10f, 0.20f, {1.0f, 0.82f, 0.64f, 0.46f}},
  /* 88 mallow     */ {7, 0, 5, 1, 6, 0, 0.00f, 1.20f, 0.30f, {1.0f, 0, 0, 0}},
  /* 89 pomegran.  */ {6, 1, 6, 2, 1, 0, 0.15f, 1.00f, 0.60f, {1.0f, 0.70f, 0, 0}},
};

inline float pf(int shape, float u) {
  switch (shape) {
    case 0: return sinf(PI_F * powf(u, 0.6f));                  // pointed
    case 1: return sinf(PI_F * powf(u, 0.85f));                 // narrow oval
    case 2: return powf(fmaxf(0.0f, sinf(PI_F * powf(u, 0.8f))), 0.65f);   // broad, round
    case 4: return u < 0.5f ? 2.0f * u : 2.0f * (1.0f - u);       // diamond (star point)
    case 5: return sqrtf(fmaxf(0.0f, 1.0f - (2.0f * u - 1.0f) * (2.0f * u - 1.0f)));  // round
    case 6: return powf(fmaxf(0.0f, sinf(PI_F * powf(u, 0.45f))), 0.9f);      // teardrop (widest near the base)
    case 7: return fminf(1.0f, 3.0f * u) * sqrtf(fmaxf(0.0f, 1.0f - powf(u, 4.0f)));  // fan
    case 8: return sqrtf(u) * sqrtf(fmaxf(0.0f, 1.0f - powf(u, 8.0f)));   // paddle
    case 9: return powf(1.0f - u, 0.8f) * fminf(1.0f, 6.0f * u);           // dagger (tapers to a point)
    default: return powf(u, 0.55f) * sqrtf(1.0f - powf(u, 5.0f)) * 1.05f;  // spatulate
  }
}

// One petal starting at distance d0 from (cx, cy), pointing at `ang` (clockwise
// from up), length len, half-width hw, filled with pattern lv.
inline void petal(Adafruit_GFX &d, float cx, float cy, float ang, float d0, float len, float hw,
                  const Sp &sp, int lv) {
  const int S = 10;
  uint16_t ink = qwInk(), paper = qwPaper();
  float px[S + 1], py[S + 1], qx[S + 1], qy[S + 1], w[S + 1];
  px[0] = cx + sinf(ang) * d0;
  py[0] = cy - cosf(ang) * d0;
  for (int k = 0; k <= S; k++) {
    float u = (float)k / S, a = ang + sp.bend * u;
    qx[k] = cosf(a);
    qy[k] = sinf(a);
    w[k] = pf(sp.shape, u) * hw * (1.0f + sp.frill * sinf(u * PI_F * 5.0f));
    if (k < S) {
      px[k + 1] = px[k] + sinf(a) * len / S;
      py[k + 1] = py[k] - cosf(a) * len / S;
    }
  }
  if (sp.notch) w[S] = hw * 0.3f;
  int16_t X[2 * S + 2], Y[2 * S + 2];
  int n = 0;
  for (int k = 0; k <= S; k++) {
    X[n] = (int16_t)lroundf(px[k] + qx[k] * w[k]);
    Y[n++] = (int16_t)lroundf(py[k] + qy[k] * w[k]);
  }
  if (sp.notch) {  // V-shaped cleft in the tip
    X[n] = (int16_t)lroundf((px[S - 2] + px[S - 1]) * 0.5f);
    Y[n++] = (int16_t)lroundf((py[S - 2] + py[S - 1]) * 0.5f);
  }
  for (int k = S; k >= 1; k--) {
    X[n] = (int16_t)lroundf(px[k] - qx[k] * w[k]);
    Y[n++] = (int16_t)lroundf(py[k] - qy[k] * w[k]);
  }
  polyFill(d, X, Y, n, lv);
  for (int i = 0; i < n; i++) d.drawLine(X[i], Y[i], X[(i + 1) % n], Y[(i + 1) % n], ink);
  if (len < 9 || sp.motif == 0) return;
  uint16_t mc = (lv == 1 || lv == 3) ? paper : ink;  // motif lines stay visible on dark fills
  int16_t bx = (int16_t)lroundf(px[0]), by = (int16_t)lroundf(py[0]);
#define PTX(k, f) ((int16_t)lroundf(px[k] + qx[k] * w[k] * (f)))
#define PTY(k, f) ((int16_t)lroundf(py[k] + qy[k] * w[k] * (f)))
  switch (sp.motif) {
    case 1:  // vein
      d.drawLine(bx, by, PTX(S - 2, 0), PTY(S - 2, 0), mc);
      break;
    case 2:  // inner arch
      for (int k = 1; k < S - 2; k++)
        for (int sgn = -1; sgn <= 1; sgn += 2)
          d.drawLine(PTX(k, 0.5f * sgn), PTY(k, 0.5f * sgn), PTX(k + 1, 0.5f * sgn),
                     PTY(k + 1, 0.5f * sgn), mc);
      d.drawLine(PTX(S - 2, 0.5f), PTY(S - 2, 0.5f), PTX(S - 2, -0.5f), PTY(S - 2, -0.5f), mc);
      break;
    case 3:  // three veins
      for (int f = -1; f <= 1; f++) d.drawLine(bx, by, PTX(S - 3, 0.7f * f), PTY(S - 3, 0.7f * f), mc);
      break;
    case 4:  // vein ending in a dot
      d.drawLine(bx, by, PTX(S - 3, 0), PTY(S - 3, 0), mc);
      d.fillRect(PTX(S - 2, 0) - 1, PTY(S - 2, 0) - 1, 3, 3, mc);
      break;
    case 5:  // short cross strokes near the tip
      for (int k = S - 4; k <= S - 2; k += 2)
        d.drawLine(PTX(k, -0.55f), PTY(k, -0.55f), PTX(k, 0.55f), PTY(k, 0.55f), mc);
      break;
    default:  // fan of five short lines
      for (int f = -2; f <= 2; f++) d.drawLine(bx, by, PTX(5, 0.4f * f), PTY(5, 0.4f * f), mc);
      break;
  }
#undef PTX
#undef PTY
}

// More dhikr, more petals: +1 for 1, +2 for 4, +3 for 9, +4 for 33, +7 from 136.
inline int extraPetals(uint16_t count) {
  int e = (int)(0.6f * sqrtf((float)count) + 0.99f);
  return e > 7 ? 7 : e;
}


inline int speciesOf(int sq) { return (sq * 7) % NSP; }   // 7 and NSP are coprime: all species come round

// Number of petals of a flower (florets for the hydrangea).
inline int flowerUnits(int species, uint16_t count) {
  const Sp &sp = SPECIES[species % NSP];
  int e = extraPetals(count);
  if (species % NSP == SP_HYDRANGEA) return 7 + e;
  int P = sp.base + e, U = 0;
  for (int l = 0; l < sp.layers; l++) {
    int Pl = P - 2 * l;
    U += Pl < 4 ? 4 : Pl;
  }
  return U;
}

// Petal u of U has fallen when `lost` petals are gone (random but fixed order).
inline bool fallen(uint32_t seed, int u, int U, int lost) {
  if (lost <= 0) return false;
  if (lost >= U) return true;
  float ku = h01(seed, u * 13 + 5);
  int rank = 0;
  for (int k = 0; k < U; k++) {
    if (k == u) continue;
    float kk = h01(seed, k * 13 + 5);
    if (kk < ku || (kk == ku && k < u)) rank++;
  }
  return rank < lost;
}

// ---------------- stored record ----------------
struct State {
  uint32_t lastEntry;   // RTC minute of the last entry
  uint32_t lastCredit;  // RTC minute of the last counted day
  uint32_t total;       // all dhikr ever added
  uint16_t seq;         // flowers created so far (decides the species)
  uint8_t n;            // flowers around the heart (0..FLOWERS)
  uint8_t head;         // index of the oldest flower (ring buffer when full)
  uint8_t valid;
  uint8_t pad;
  uint16_t day[FLOWERS];  // dhikr of each flower
  uint16_t sq[FLOWERS];   // sequence number of each flower
};

struct Troph {  // flowers that left the ring, oldest first
  uint16_t n;
  uint16_t cnt[TMAX];
  uint16_t sq[TMAX];
};

inline void load(State &s, bool prev) {
  memset(&s, 0, sizeof(s));
  if (!QWStorage::loadBlob(prev ? "gdPrev" : "gd", &s, sizeof(s)) || s.valid != 1)
    memset(&s, 0, sizeof(s));
}
inline bool readRec(State &s, const char *key) {  // true if the record exists
  memset(&s, 0, sizeof(s));
  if (QWStorage::loadBlob(key, &s, sizeof(s))) return true;
  memset(&s, 0, sizeof(s));
  return false;
}
inline void save(const State &s, bool prev) {
  QWStorage::saveBlob(prev ? "gdPrev" : "gd", &s, sizeof(s));
}
inline void loadT(Troph &t, bool prev) {
  memset(&t, 0, sizeof(t));
  if (!QWStorage::loadBlob(prev ? "gdTrPrev" : "gdTr", &t, sizeof(t)) || t.n > TMAX)
    memset(&t, 0, sizeof(t));
}
inline void saveT(const Troph &t, bool prev) {
  QWStorage::saveBlob(prev ? "gdTrPrev" : "gdTr", &t, sizeof(t));
}

inline uint32_t elapsed(const State &s, uint32_t now) {
  return now > s.lastEntry ? now - s.lastEntry : 0;
}
inline int slot(const State &s, int i) { return (s.head + i) % s.n; }  // i-th oldest

struct View {
  bool any;       // there is at least one flower
  bool wilting;   // more than 24 h without dhikr
  bool env;       // the protective envelope is shown
  uint8_t dead;   // oldest flowers that have lost all their petals
  uint16_t lost;  // petals lost by the oldest living flower
  uint32_t e;     // minutes since the last entry
};

inline View makeView(const State &s, uint32_t now) {
  View v;
  memset(&v, 0, sizeof(v));
  if (!s.valid || s.n == 0) return v;
  v.any = true;
  v.e = elapsed(s, now);
  v.wilting = v.e >= DAY_MIN;
  if (v.wilting) {
    uint32_t L = (v.e - DAY_MIN) / PETAL_MIN + 1;  // petals fallen so far
    for (int i = 0; i < s.n; i++) {
      int j = slot(s, i);
      uint32_t U = (uint32_t)flowerUnits(speciesOf(s.sq[j]), s.day[j]);
      if (L >= U) { v.dead++; L -= U; }
      else { v.lost = (uint16_t)L; break; }
    }
  }
  v.env = (s.n >= FLOWERS && v.dead == 0 && v.e < ENV_MIN);
  return v;
}

struct AddInfo {
  bool evict;  // the oldest flower left the ring
  uint16_t evCnt, evSq;
  uint8_t pos;  // ring position of the new flower (= the evicted one)
};

inline void compact(State &s, int k) {  // removes the k oldest flowers
  uint16_t d2[FLOWERS] = {0}, q2[FLOWERS] = {0};
  int m = s.n - k;
  for (int i = 0; i < m; i++) {
    int j = (s.head + k + i) % s.n;
    d2[i] = s.day[j];
    q2[i] = s.sq[j];
  }
  memcpy(s.day, d2, sizeof(d2));
  memcpy(s.sq, q2, sizeof(q2));
  s.n = (uint8_t)m;
  s.head = 0;
}

inline AddInfo addEntry(State &s, uint32_t now, uint16_t amount) {
  AddInfo a;
  memset(&a, 0, sizeof(a));
  if (!s.valid) {
    memset(&s, 0, sizeof(s));
    s.valid = 1;
  }
  View v = makeView(s, now);
  if (v.dead > 0) compact(s, v.dead);  // flowers that died stay dead
  bool newDay = (s.n == 0) || (now > s.lastCredit && now - s.lastCredit >= DAY_MIN);
  if (newDay) {
    if (s.n >= FLOWERS) {  // the oldest flower leaves, the new one takes its place
      a.evict = true;
      a.evCnt = s.day[s.head];
      a.evSq = s.sq[s.head];
      a.pos = s.head;
      s.day[s.head] = 0;
      s.sq[s.head] = s.seq++;
      s.head = (s.head + 1) % FLOWERS;
    } else {
      s.day[s.n] = 0;
      s.sq[s.n] = s.seq++;
      a.pos = s.n;
      s.n++;
    }
    s.lastCredit = now;
  }
  int cur = (s.head + s.n - 1) % s.n;  // the newest flower
  uint32_t t = (uint32_t)s.day[cur] + amount;
  s.day[cur] = (uint16_t)(t > 65535 ? 65535 : t);
  s.total += amount;
  s.lastEntry = now;
  return a;
}

inline void drawTile(Adafruit_GFX &d, float cx, float cy, float r, uint16_t count, int sq,
                     float rot, int lost, float grow, bool mini) {
  uint16_t ink = qwInk(), paper = qwPaper();
  int species = speciesOf(sq);
  const Sp &sp = SPECIES[species];
  int16_t icx = (int16_t)lroundf(cx), icy = (int16_t)lroundf(cy);
  if (grow <= 0.02f) {  // closed: a bud
    d.fillCircle(icx, icy, mini ? 2 : 4, ink);
    return;
  }
  int e = extraPetals(count);
  uint32_t hc = (uint32_t)(h01((uint32_t)count * 2654u + (uint32_t)sq * 31u + 1u, 77) * 4096.0f);
  const int rc = mini ? 2 : (((count >= 100 && count < 1000) || count >= 10000) ? 11 : 8);
  float d0 = rc * 0.6f;
  uint32_t seed = (uint32_t)sq * 7919u + 13u;
  int U = flowerUnits(species, count);
  if (species == SP_HYDRANGEA) {  // a ball of small four-petal florets
    int M = 7 + e;
    float fr = mini ? 2.6f : (M > 12 ? 4.2f : 5.0f);
    float rm = rc - 1.0f;
    for (int k = M - 1; k >= 0; k--) {
      if (fallen(seed, k, U, lost)) continue;
      float rad = (rm + (r - fr - rm) * sqrtf((k + 0.5f) / M)) * grow;
      float a = k * 2.39996f + rot;
      float fx = cx + rad * cosf(a), fy = cy + rad * sinf(a);
      int lv = (int)((hc + k) % 2u);  // paper / solid
      for (int j = 0; j < 4; j++)
        petal(d, fx, fy, a + j * PI_F / 2, 0.8f, fr, fr * 0.6f, sp, lv);
      d.drawPixel((int16_t)lroundf(fx), (int16_t)lroundf(fy), ink);
    }
  } else {
    int P = sp.base + e;
    int lvMax = (mini || P >= 11) ? 2 : 4;  // thin petals: only paper / solid
    int u = 0;
    for (int l = 0; l < sp.layers; l++) {
      int Pl = P - 2 * l;
      if (Pl < 4) Pl = 4;
      float len = (r - d0 - 1.5f) * grow * sp.lenF[l];
      if (len < 3) { u += Pl; continue; }
      float rot0 = rot + sq * 0.3f;
      if (sp.rot == 0) rot0 += (l & 1) ? PI_F / Pl : 0.0f;
      else if (sp.rot == 1) rot0 += l * 2.39996f;
      else rot0 += l * 0.18f;
      float hw = (d0 + 0.5f * len) * sinf(PI_F / Pl) * sp.wf;
      if (hw > len * 0.7f) hw = len * 0.7f;
      float hmin = mini ? 0.8f : 1.3f;
      if (hw < hmin) hw = hmin;
      int lv = (int)((hc + l) % (uint32_t)lvMax);
      for (int j = 0; j < Pl; j++, u++) {
        if (fallen(seed, u, U, lost)) continue;  // fallen petal
        petal(d, cx, cy, rot0 + j * 2.0f * PI_F / Pl, d0, len, hw, sp, lv);
      }
    }
  }
  // centre: the day's total
  float rcs = rc * grow;
  if (rcs < 2) rcs = 2;
  int16_t rr = (int16_t)lroundf(mini ? (float)rc : rcs);
  d.fillCircle(icx, icy, rr, paper);
  d.drawCircle(icx, icy, rr, ink);
  if (mini) return;
  if (grow >= 1.0f) {
    char b[8];
    if (count >= 1000) snprintf(b, sizeof(b), "%uK", (unsigned)(count / 1000));
    else snprintf(b, sizeof(b), "%u", (unsigned)count);
    d.setTextColor(ink);
    qwSetText(d, 1);
    d.setCursor(icx - qwTextInkWidth((uint8_t)strlen(b), 1) / 2, icy - 4);
    d.print(b);
  } else {
    d.fillCircle(icx, icy, 1, ink);
  }
}

struct Scene {
  int growIdx = -1;    // ring position of the flower that is opening
  float grow = 1.0f;
  float all = 1.0f;    // opening of all the other flowers (0 = buds)
  float phi = 0.0f;    // the whole ring turned by this angle
  float hs = 1.0f;     // heart beat
  int skip = -1;       // ring position left empty
  int gPos = -1;       // a flower leaving the frame: its ring position...
  uint16_t gCnt = 0, gSq = 0;
  float gDist = 0.0f;  // ...and how far it has moved outwards
};

// The heart, its envelope and the flowers.
inline void drawBloom(Adafruit_GFX &d, const State &s, const View &v, const Scene &sc) {
  drawHeart(d, sc.hs);
  if (v.env) drawEnvelope(d, sc.hs);
  int m = (int)s.n - (int)v.dead;
  if (m <= 0) return;
  uint16_t pc[FLOWERS], ps[FLOWERS];
  int oldPos = 0;
  if (v.dead == 0) {  // physical positions
    for (int j = 0; j < s.n; j++) { pc[j] = s.day[j]; ps[j] = s.sq[j]; }
    oldPos = s.head;
  } else {  // the dead are gone: the survivors in age order
    for (int i = 0; i < m; i++) {
      int j = slot(s, v.dead + i);
      pc[i] = s.day[j];
      ps[i] = s.sq[j];
    }
  }
  float r = TILE_MAX;
  if (m >= 3) {
    float rr = RING * sinf(PI_F / m) * 0.94f;  // flowers shrink as they get numerous
    if (rr < r) r = rr;
  }
  // odd m: first flower on top; even m: flat polygon (m = 2: left and right)
  float base = -PI_F / 2 + ((m % 2 == 0) ? PI_F / m : 0.0f);
  for (int i = 0; i < m; i++) {
    if (i == sc.skip) continue;
    float ang = base + 2.0f * PI_F * i / m + sc.phi;
    float g = (i == sc.growIdx) ? sc.grow : sc.all;
    drawTile(d, CX + RING * cosf(ang), CY + RING * sinf(ang), r, pc[i], ps[i], sc.phi,
             i == oldPos ? v.lost : 0, g, false);
  }
  if (sc.gPos >= 0 && sc.gPos < m) {  // the flower that leaves the frame
    float ang = base + 2.0f * PI_F * sc.gPos / m;
    float rr = RING + sc.gDist;
    drawTile(d, CX + rr * cosf(ang), CY + rr * sinf(ang), r, sc.gCnt, sc.gSq, 0.0f, 0, 1.0f, false);
  }
}

// ---------------- screens ----------------
enum Phase : uint8_t { PH_VIEW = 0, PH_CHOOSE, PH_NUMBER, PH_TROPHY };

// sakura crest on the watch face (see kamonInfo)
RTC_DATA_ATTR uint32_t kamonHidden = 0;        // fallen-petal count when it was hidden
RTC_DATA_ATTR uint8_t kamonDismissPending = 0; // set by the BACK button, applied by main.cpp
inline uint32_t faceNow = 0;                   // raw RTC minute, set by main.cpp before drawing
RTC_DATA_ATTR uint8_t phase = PH_VIEW;
RTC_DATA_ATTR uint8_t sel = 0;
RTC_DATA_ATTR uint16_t num = 33;
RTC_DATA_ATTR uint16_t pendingAdd = 0;   // set by MENU, applied by main.cpp
RTC_DATA_ATTR uint8_t pendingUndo = 0;
RTC_DATA_ATTR uint8_t animPending = 0;   // play the opening flower before the next draw
RTC_DATA_ATTR uint8_t introPending = 0;  // play the intro (turn + heartbeat) on entering
RTC_DATA_ATTR uint8_t evFlag = 0;        // the oldest flower leaves the frame in the animation
RTC_DATA_ATTR uint8_t evPos = 0;
RTC_DATA_ATTR uint16_t evCnt = 0, evSq = 0;

constexpr int ITEMS = 5;
static const char *const ITEM_NAME[ITEMS] = {"ADD 33", "ADD 99", "ADD 100", "ADD FREE", "UNDO LAST"};

inline void open() {
  phase = PH_VIEW;
  sel = 0;
  introPending = 1;
}

// Applies a pending entry or undo. Called by main.cpp with the raw RTC minute.
inline void commit(uint32_t now) {
  if (pendingUndo) {
    static State p;
    if (readRec(p, "gdPrev")) {
      save(p, false);
      QWStorage::removeKey("gdPrev");
      static Troph t;
      if (QWStorage::loadBlob("gdTrPrev", &t, sizeof(t))) {
        QWStorage::saveBlob("gdTr", &t, sizeof(t));
        QWStorage::removeKey("gdTrPrev");
      }
    }
  } else if (pendingAdd) {
    static State s;
    static Troph t;
    load(s, false);
    loadT(t, false);
    save(s, true);  // for UNDO (an empty bloom is stored too)
    saveT(t, true);
    AddInfo a = addEntry(s, now, pendingAdd);
    if (a.evict && t.n < TMAX) {  // the oldest flower becomes a trophy
      t.cnt[t.n] = a.evCnt;
      t.sq[t.n] = a.evSq;
      t.n++;
      saveT(t, false);
    }
    save(s, false);
    evFlag = a.evict ? 1 : 0;
    evPos = a.pos;
    evCnt = a.evCnt;
    evSq = a.evSq;
    animPending = 1;
  }
  pendingAdd = 0;
  pendingUndo = 0;
  kamonHidden = 0;  // a new wilting starts from scratch
}

inline void select() {
  if (phase == PH_VIEW) {
    phase = PH_CHOOSE;
    sel = 0;
  } else if (phase == PH_CHOOSE) {
    if (sel == 0) { pendingAdd = 33; phase = PH_VIEW; }
    else if (sel == 1) { pendingAdd = 99; phase = PH_VIEW; }
    else if (sel == 2) { pendingAdd = 100; phase = PH_VIEW; }
    else if (sel == 3) { phase = PH_NUMBER; if (num < 1) num = 33; }
    else { pendingUndo = 1; phase = PH_VIEW; }
  } else if (phase == PH_NUMBER) {
    pendingAdd = num;
    phase = PH_VIEW;
  }
}

// true = leave the bloom (back to the watch face)
inline bool back() {
  if (phase == PH_VIEW) return true;
  phase = (phase == PH_NUMBER) ? PH_CHOOSE : PH_VIEW;
  return false;
}

// UP (dir +1) / DOWN (dir -1), with the time the button was held.
inline void button(int dir, unsigned long heldMs) {
  if (phase == PH_VIEW) {
    if (dir < 0) phase = PH_TROPHY;  // DOWN: the trophies
  } else if (phase == PH_CHOOSE) {
    int m = (int)sel - dir;
    if (m < 0) m = ITEMS - 1;
    if (m >= ITEMS) m = 0;
    sel = (uint8_t)m;
  } else if (phase == PH_NUMBER) {
    int step = heldMs > 1000 ? 50 : (heldMs > 400 ? 10 : 1);
    int v = (int)num + dir * step;
    if (v < 1) v = 1;
    if (v > NUM_MAX) v = NUM_MAX;
    num = (uint16_t)v;
  }
}

inline void centerText(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(t), size) / 2, y);
  d.print(t);
}

// The main screen.
inline void drawMainScreen(Adafruit_GFX &d, uint32_t now, const Scene &sc) {
  static State s;
  load(s, false);
  View v = makeView(s, now);
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char l1[40], l2[40];
  int m = (int)s.n - (int)v.dead;
  if (!v.any) {
    snprintf(l1, sizeof(l1), "DHIKR BLOOM");
    snprintf(l2, sizeof(l2), "MENU = ADD DHIKR");
  } else if (m <= 0) {
    snprintf(l1, sizeof(l1), "NO FLOWERS LEFT");
    snprintf(l2, sizeof(l2), "ADD DHIKR TO START AGAIN");
  } else {
    if (v.env) snprintf(l1, sizeof(l1), "HEART PROTECTED");
    else snprintf(l1, sizeof(l1), "FLOWERS %d/%d", m, FLOWERS);
    if (!v.wilting) {
      unsigned long h = (DAY_MIN - v.e + 59) / 60;
      snprintf(l2, sizeof(l2), "TOTAL %lu  WILTS IN %luH", (unsigned long)s.total, h);
    } else {
      snprintf(l2, sizeof(l2), "PETALS FALLING - ADD DHIKR");
    }
  }
  centerText(d, l1, 3, 1);
  centerText(d, l2, 14, 1);
  if (v.any) drawBloom(d, s, v, sc);
  else drawHeart(d, sc.hs);
  d.fillTriangle(184, 187, 196, 187, 190, 196, qwInk());  // DOWN = trophies
}

// One picture of the animation (frame 0..animCount()-1): the oldest flower
// leaves the frame (only when the ring was full), then today's flower opens.
inline int animCount() { return (evFlag ? MOVE_FRAMES : 0) + ANIM_FRAMES; }

inline void drawAnim(Adafruit_GFX &d, int frame, uint32_t now) {
  static State s;
  load(s, false);
  Scene sc;
  int mf = evFlag ? MOVE_FRAMES : 0;
  int newest = s.n > 0 ? (s.head + s.n - 1) % s.n : 0;
  if (frame < mf) {
    static const float DIST[3] = {22.0f, 48.0f, 84.0f};
    sc.skip = evPos;  // the new flower is not there yet
    sc.gPos = evPos;
    sc.gCnt = evCnt;
    sc.gSq = evSq;
    sc.gDist = DIST[frame < 3 ? frame : 2];
  } else {
    sc.growIdx = newest;
    sc.grow = (frame - mf + 1) * 0.25f;
  }
  drawMainScreen(d, now, sc);
}

// One picture of the intro: all the flowers are closed while the ring makes
// a full turn in steps of 45 degrees (the heart beats), then they open.
inline int introCount() {
  static State s;
  load(s, false);
  return (s.n > 0 ? 8 : 4) + 2;
}

inline void drawIntro(Adafruit_GFX &d, int frame, uint32_t now) {
  static State s;
  load(s, false);
  static const float BEAT[4] = {1.10f, 1.0f, 1.07f, 1.0f};
  int tf = s.n > 0 ? 8 : 4;
  Scene sc;
  sc.hs = BEAT[frame % 4];
  if (frame < tf) {
    sc.phi = s.n > 0 ? 2.0f * PI_F * (frame + 1) / tf : 0.0f;
    sc.all = 0.0f;
  } else {
    sc.all = 0.4f + 0.35f * (frame - tf);
  }
  drawMainScreen(d, now, sc);
}

// The trophies: flowers that left the ring, in lines, oldest first.
inline void drawTrophyScreen(Adafruit_GFX &d) {
  static Troph t;
  loadT(t, false);
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char b[32];
  snprintf(b, sizeof(b), "TROPHIES  %d/%d", (int)t.n, TMAX);
  centerText(d, b, 3, 1);
  centerText(d, t.n ? "BACK = RETURN" : "NONE YET  -  BACK = RETURN", 14, 1);
  d.drawFastHLine(0, 25, 200, qwInk());
  for (int i = 0; i < t.n; i++)
    drawTile(d, 16.0f + 24 * (i % 8), 38.0f + 24 * (i / 8), 11.0f, t.cnt[i], t.sq[i], 0.0f, 0, 1.0f, true);
}

inline void drawChooseScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(6, 4);
  d.print("ADD DHIKR");
  d.drawFastHLine(0, 24, 200, qwInk());
  for (int i = 0; i < ITEMS; i++) {
    int16_t y = 32 + i * 28;
    bool on = (i == sel);
    if (on) {
      d.fillRect(10, y - 4, 180, 24, qwInk());
      d.setTextColor(qwPaper());
    } else {
      d.drawRect(10, y - 4, 180, 24, qwInk());
      d.setTextColor(qwInk());
    }
    centerText(d, ITEM_NAME[i], y + 1, 2);
  }
  d.setTextColor(qwInk());
  centerText(d, "UP/DOWN=select  MENU=ok", 186, 1);
}

inline void drawNumberScreen(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(6, 4);
  d.print("HOW MANY?");
  d.drawFastHLine(0, 24, 200, qwInk());
  char b[8];
  snprintf(b, sizeof(b), "%u", (unsigned)num);
  d.drawRect(30, 56, 140, 70, qwInk());
  d.drawRect(32, 58, 136, 66, qwInk());
  centerText(d, b, 76, 5);
  centerText(d, "UP = more   DOWN = less", 146, 1);
  centerText(d, "hold = faster (10, then 50)", 158, 1);
  centerText(d, "MENU=ok  BACK=cancel", 186, 1);
}

// ---------------- the sakura crest on the watch face ----------------
// While the oldest living flower is losing petals, the watch face shows a
// five-petal crest (a kamon) in place of the info zone. Whole petals are
// black, a lost petal is hollow. BACK (short press) hides it until the next
// petal falls; a new entry resets it.

inline bool kamonInfo(uint32_t now, int &hollow, int &lost, int &units, bool &none) {
  static State s;
  load(s, false);
  View v = makeView(s, now);
  if (!v.any || !v.wilting) return false;
  uint32_t fallen = (v.e - DAY_MIN) / PETAL_MIN + 1;
  if (kamonHidden == fallen) return false;
  int m = (int)s.n - (int)v.dead;
  none = m <= 0;
  hollow = 5;
  lost = 0;
  units = 0;
  if (!none) {
    int j = slot(s, v.dead);
    units = flowerUnits(speciesOf(s.sq[j]), s.day[j]);
    lost = v.lost;
    hollow = (5 * lost + units - 1) / units;  // the first lost petal already shows
    if (hollow > 5) hollow = 5;
  }
  return true;
}

inline void kamonDismiss(uint32_t now) {
  static State s;
  load(s, false);
  View v = makeView(s, now);
  if (v.any && v.wilting) kamonHidden = (v.e - DAY_MIN) / PETAL_MIN + 1;
}

// Petal outline (u along the petal, w across it, in units of the petal
// length): broad shoulders and a stepped cleft at the tip.
inline void kamonPetal(Adafruit_GFX &d, float cx, float cy, float L, float a, bool hollowPetal) {
  static const float H[10][2] = {{0.10f, 0.05f}, {0.22f, 0.14f}, {0.36f, 0.25f}, {0.50f, 0.33f},
                                 {0.64f, 0.38f}, {0.78f, 0.41f}, {0.88f, 0.39f}, {0.95f, 0.32f},
                                 {1.00f, 0.20f}, {0.95f, 0.13f}};
  int16_t X[22], Y[22];
  int n = 0;
  float ca = cosf(a), sa = sinf(a);
  auto put = [&](float u, float w) {
    X[n] = (int16_t)lroundf(cx + L * (u * ca - w * sa));
    Y[n++] = (int16_t)lroundf(cy + L * (u * sa + w * ca));
  };
  put(0.06f, 0.0f);
  for (int i = 0; i < 10; i++) put(H[i][0], H[i][1]);
  put(0.86f, 0.0f);  // the bottom of the cleft
  for (int i = 9; i >= 0; i--) put(H[i][0], -H[i][1]);
  uint16_t ink = qwInk(), paper = qwPaper();
  for (int i = 1; i + 1 < n; i++)
    d.fillTriangle(X[0], Y[0], X[i], Y[i], X[i + 1], Y[i + 1], hollowPetal ? paper : ink);
  for (int i = 0; i < n; i++) {
    int j = (i + 1) % n;
    if (hollowPetal) {  // thick outline, like the original crest
      d.drawLine(X[i], Y[i], X[j], Y[j], ink);
      d.drawLine(X[i] + 1, Y[i], X[j] + 1, Y[j], ink);
      d.drawLine(X[i], Y[i] + 1, X[j], Y[j] + 1, ink);
    } else {
      d.drawLine(X[i], Y[i], X[j], Y[j], paper);  // thin gap between black petals
    }
  }
}

inline void drawKamon(Adafruit_GFX &d, int hollow, int lost, int units, bool none) {
  uint16_t ink = qwInk(), paper = qwPaper();
  const float cx = 100.0f, cy = 27.0f, L = 24.0f;
  for (int pass = 0; pass < 2; pass++)  // whole petals first, hollow ones over them
    for (int k = 0; k < 5; k++) {
      bool h = k < hollow;
      if (h == (pass == 1)) kamonPetal(d, cx, cy, L, -PI_F / 2 + 2.0f * PI_F * k / 5.0f, h);
    }
  int16_t icx = (int16_t)cx, icy = (int16_t)cy;
  for (int r = 0; r < 8; r++) {  // the eight short rays around the centre
    float a = -PI_F / 2 + r * PI_F / 4;
    int pk = (int)lroundf((a + PI_F / 2) / (2.0f * PI_F / 5.0f)) % 5;
    uint16_t c = (pk >= hollow) ? paper : ink;
    d.drawLine((int16_t)lroundf(cx + 5.5f * cosf(a)), (int16_t)lroundf(cy + 5.5f * sinf(a)),
               (int16_t)lroundf(cx + 8.5f * cosf(a)), (int16_t)lroundf(cy + 8.5f * sinf(a)), c);
  }
  d.drawCircle(icx, icy, 4, paper);
  d.fillCircle(icx, icy, 3, ink);
  char b[16];
  if (none) snprintf(b, sizeof(b), "NO FLOWERS");
  else snprintf(b, sizeof(b), "%d/%d LOST", lost, units);
  d.setTextColor(ink);
  centerText(d, b, 54, 1);
}

// hour: displayed hour (not used by the bloom, kept for the main.cpp call).
inline void draw(Adafruit_GFX &d, uint32_t now, int hour) {
  (void)hour;
  if (phase == PH_CHOOSE) drawChooseScreen(d);
  else if (phase == PH_NUMBER) drawNumberScreen(d);
  else if (phase == PH_TROPHY) drawTrophyScreen(d);
  else drawMainScreen(d, now, Scene());
}

}  // namespace Gd
