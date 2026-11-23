#pragma once
/*
  AlarmCore.h
  ------------------------------------------------------------
  Data and logic shared by the ALARMS app (GameAlarms.h), the watch face
  (SacredMonth.h / UiLayout.h) and the vibration check (main.cpp).

  Up to 8 named alarms, each with a time, weekdays and a Morse vibration:
  UP, GO, RISE, ARISE or WAKE. An alarm "rings" from its time for 5 minutes
  (or until BACK on the watch face): the sonar picture with its name is shown
  in the info zone and the watch vibrates once per minute.
  Saved in the flash blob "alrmS" (cached in RTC memory between wake-ups).
*/
#include <Arduino.h>
#include <Watchy.h>
#include "Storage.h"

namespace Alm {

constexpr int MAXA = 8, NAMEL = 10, RING_MIN = 5;
struct Al { char name[NAMEL + 1]; uint8_t h, m, days, pat, on; };   // days: bit 0 = Monday
struct Blob { uint8_t n; Al a[MAXA]; };
struct Quick { uint8_t on, h, m, pat; uint16_t day; };   // one alarm for one given day (days since 1970); ring index MAXA

static const char *const PATN[5] = {"UP", "GO", "RISE", "ARISE", "WAKE"};
// Morse: dot 100 ms, dash 300 ms, gap 100 ms, space = letter gap
static const char *const MORSE[5] = {"..- .--.", "--. ---", ".-. .. ... .", ".- .-. .. ... .", ".-- .- -.- ."};

RTC_DATA_ATTR uint8_t loaded = 0, n = 0;
RTC_DATA_ATTR Al al[MAXA];
RTC_DATA_ATTR Quick q;
RTC_DATA_ATTR uint32_t dis[MAXA + 1];  // day on which each alarm (and the quick alarm, last) was dismissed by BACK
RTC_DATA_ATTR uint32_t lastRing = 0;   // minute already vibrated (day * 1440 + minute)

inline void load() {
  if (loaded) return;
  Blob b;
  memset(&b, 0, sizeof(b));
  if (QWStorage::loadBlob("alrmS", &b, sizeof(b)) && b.n <= MAXA) { n = b.n; memcpy(al, b.a, sizeof(al)); }
  Quick t;
  memset(&t, 0, sizeof(t));
  if (QWStorage::loadBlob("alrmQ", &t, sizeof(t)) && t.h < 24 && t.m < 60) q = t;
  loaded = 1;
}
inline void saveQ() { QWStorage::saveBlob("alrmQ", &q, sizeof(q)); }
inline uint8_t patOf(int i) { return i == MAXA ? q.pat : al[i].pat; }
inline void save() {
  Blob b;
  memset(&b, 0, sizeof(b));
  b.n = n;
  memcpy(b.a, al, sizeof(al));
  QWStorage::saveBlob("alrmS", &b, sizeof(b));
}
// index of the alarm ringing now (dayKey = days since 1970), or -1
inline int ringing(int nowMin, uint32_t dayKey) {
  load();
  if (nowMin < 0) return -1;
  int wd = (int)((dayKey + 3) % 7);
  for (int i = 0; i < n; i++) {
    if (!al[i].on || !((al[i].days >> wd) & 1) || dis[i] == dayKey) continue;
    int a = al[i].h * 60 + al[i].m;
    if (nowMin >= a && nowMin < a + RING_MIN) return i;
  }
  if (q.on && q.day == dayKey && dis[MAXA] != dayKey) {
    int a = q.h * 60 + q.m;
    if (nowMin >= a && nowMin < a + RING_MIN) return MAXA;
  }
  return -1;
}
inline void morse(const char *c) {
  pinMode(VIB_MOTOR_PIN, OUTPUT);
  for (; *c; c++) {
    if (*c == ' ') { delay(300); continue; }
    digitalWrite(VIB_MOTOR_PIN, HIGH);
    delay(*c == '.' ? 100 : 300);
    digitalWrite(VIB_MOTOR_PIN, LOW);
    delay(100);
  }
}

}  // namespace Alm
