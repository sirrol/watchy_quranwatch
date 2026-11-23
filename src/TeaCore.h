#pragma once
/*
  TeaCore.h
  ------------------------------------------------------------
  Data and logic shared by the TEA TIMER app (GameTea.h), the watch face
  (SacredMonth.h / UiLayout.h) and the vibration check (main.cpp).

  state 0 = no timer, 1 = brewing, 2 = tea ready (the cup picture shows in the
  info zone until BACK). The watch vibrates when the tea is ready and once
  more one minute later if BACK was not pressed. While a timer runs, the
  ESP32 timer is armed (armWake) so the watch wakes at the exact second.
  Durations are saved in the flash blob "teaD" (7 x uint16 seconds).
*/
#include <Arduino.h>
#include <Watchy.h>
#include "Storage.h"
#if __has_include(<esp_sleep.h>)
#include <esp_sleep.h>
#endif

namespace Tea {

constexpr int N = 7;            // 6 teas + CUSTOM
constexpr uint16_t MINS = 15, MAXS = 3600, STEP = 15;
static const char *const NAME[N] = {"GREEN", "WHITE", "OOLONG", "BLACK", "HERBAL", "ROOIBOS", "CUSTOM"};
static const uint8_t TEMP[N] = {80, 80, 90, 95, 100, 100, 0};      // water, deg C (0 = none)
static const uint16_t DEF[N] = {120, 180, 240, 240, 360, 360, 300};

RTC_DATA_ATTR uint8_t loaded = 0, state = 0, idx = 0, buzz = 0;
RTC_DATA_ATTR uint32_t end = 0;    // end of the brew, local seconds (same clock as makeTime(effectiveTime()))
RTC_DATA_ATTR uint16_t dur[N];

inline void load() {
  if (loaded) return;
  uint16_t b[N];
  bool ok = QWStorage::loadBlob("teaD", b, sizeof(b));
  for (int i = 0; i < N; i++) dur[i] = (ok && b[i] >= MINS && b[i] <= MAXS) ? b[i] : DEF[i];
  loaded = 1;
}
inline void saveDur() { QWStorage::saveBlob("teaD", dur, sizeof(dur)); }

// Vibration when the tea is ready: three long pulses (the prayer buzz is two short ones).
inline void pulse() {
  pinMode(VIB_MOTOR_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(VIB_MOTOR_PIN, HIGH);
    delay(300);
    digitalWrite(VIB_MOTOR_PIN, LOW);
    delay(150);
  }
}

// Called once per wake-up with the current local time (seconds): handles the
// end of the brew and the repeat one minute later. Returns true if it vibrated.
inline bool check(uint32_t now, bool dismissPending) {
  if (state == 1) {
    if (now > end + 600) { state = 0; return false; }       // watch was off: forget it
    if (now + 2 >= end) { state = 2; buzz = 1; pulse(); return true; }
  } else if (state == 2) {
    if (now > end + 600) { state = 0; return false; }       // stop showing after 10 minutes
    if (dismissPending) return false;
    if (buzz == 1 && now + 2 >= end + 60) { buzz = 2; pulse(); return true; }
  }
  return false;
}

// Wake up at the exact second of the next event (the minute tick still works too).
inline void armWake(uint32_t now) {
  uint32_t target;
  if (state == 1) target = end;
  else if (state == 2 && buzz == 1) target = end + 60;
  else return;
  uint32_t d = target > now ? target - now : 1;
  if (d > 7200) return;
  esp_sleep_enable_timer_wakeup((uint64_t)d * 1000000ULL);
}

}  // namespace Tea
