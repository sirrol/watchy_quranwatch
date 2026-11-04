#pragma once
/*
  GameCapitals.h
  ------------------------------------------------------------
  CAPITALS QUIZ for QuranWatch (included by Games.h).

  10 questions per game, 3 possible answers each:
    - "What is the capital of <country>?"   or
    - "Which country has the capital <city>?"
  Levels:
    EASY: well-known countries, wrong answers taken from anywhere.
    HARD: all countries in the list, wrong answers taken from the SAME
          continent (so they look plausible).

  Answer with ONE button press: MENU = A, UP = B, DOWN = C. The result
  screen shows the three answers again (right one in black, your wrong
  choice crossed out) and a short sentence. Any button = next question.

  Countries with several capitals or a recent change use the OFFICIAL /
  constitutional capital and carry a short "note" that explains it.
  Left out on purpose: Israel and other contested cases, and
  city-states whose capital has the same name as the country.
  To add a country, add one line to DATA[]: name, capital, continent,
  well-known (1) or not (0), and optionally a note. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#include "CapitalsMap.h"   // world map bitmap + coordinates of the capitals
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

namespace Cp {

enum Region : uint8_t { EU = 0, AS, AF, NA, SA, OC };

struct Entry {
  const char *country;
  const char *capital;
  uint8_t region;
  uint8_t famous;
  const char *note;  // optional sentence shown instead of "X is the capital of Y"
};

static const Entry DATA[] = {
    // ---- Europe ----
    {"Albania", "Tirana", EU, 0}, {"Andorra", "Andorra la Vella", EU, 0},
    {"Austria", "Vienna", EU, 1}, {"Belarus", "Minsk", EU, 0},
    {"Belgium", "Brussels", EU, 1}, {"Bosnia and Herzegovina", "Sarajevo", EU, 0},
    {"Bulgaria", "Sofia", EU, 0}, {"Croatia", "Zagreb", EU, 0},
    {"Cyprus", "Nicosia", EU, 0}, {"Czechia", "Prague", EU, 0},
    {"Denmark", "Copenhagen", EU, 1}, {"Estonia", "Tallinn", EU, 0},
    {"Finland", "Helsinki", EU, 1}, {"France", "Paris", EU, 1},
    {"Germany", "Berlin", EU, 1}, {"Greece", "Athens", EU, 1},
    {"Hungary", "Budapest", EU, 0}, {"Iceland", "Reykjavik", EU, 0},
    {"Ireland", "Dublin", EU, 1}, {"Italy", "Rome", EU, 1},
    {"Latvia", "Riga", EU, 0}, {"Liechtenstein", "Vaduz", EU, 0},
    {"Lithuania", "Vilnius", EU, 0}, {"Malta", "Valletta", EU, 0},
    {"Moldova", "Chisinau", EU, 0}, {"Montenegro", "Podgorica", EU, 0},
    {"North Macedonia", "Skopje", EU, 0}, {"Norway", "Oslo", EU, 1},
    {"Netherlands", "Amsterdam", EU, 1,
     "Amsterdam is the capital of the Netherlands. The government sits in The Hague."},
    {"Poland", "Warsaw", EU, 1}, {"Portugal", "Lisbon", EU, 1},
    {"Romania", "Bucharest", EU, 0}, {"Russia", "Moscow", EU, 1},
    {"Serbia", "Belgrade", EU, 0}, {"Slovakia", "Bratislava", EU, 0},
    {"Slovenia", "Ljubljana", EU, 0}, {"Spain", "Madrid", EU, 1},
    {"Sweden", "Stockholm", EU, 1}, {"Switzerland", "Bern", EU, 1},
    {"Ukraine", "Kyiv", EU, 1}, {"United Kingdom", "London", EU, 1},
    // ---- Asia ----
    {"Afghanistan", "Kabul", AS, 1}, {"Armenia", "Yerevan", AS, 0},
    {"Azerbaijan", "Baku", AS, 0}, {"Bahrain", "Manama", AS, 0},
    {"Bangladesh", "Dhaka", AS, 0}, {"Bhutan", "Thimphu", AS, 0},
    {"Brunei", "Bandar Seri Begawan", AS, 0}, {"Cambodia", "Phnom Penh", AS, 0},
    {"China", "Beijing", AS, 1}, {"Georgia", "Tbilisi", AS, 0},
    {"India", "New Delhi", AS, 1}, {"Iran", "Tehran", AS, 1},
    {"Iraq", "Baghdad", AS, 1}, {"Japan", "Tokyo", AS, 1},
    {"Indonesia", "Jakarta", AS, 1,
     "Jakarta is still the capital of Indonesia. A new capital, Nusantara, is planned."},
    {"Sri Lanka", "Sri Jayawardenepura Kotte", AS, 0,
     "Sri Jayawardenepura Kotte is the official capital of Sri Lanka. Colombo is its commercial centre."},
    {"Yemen", "Sana'a", AS, 0,
     "Sana'a is the official capital of Yemen. The government also uses Aden as a temporary seat."},
    {"Jordan", "Amman", AS, 0}, {"Kazakhstan", "Astana", AS, 0},
    {"Kuwait", "Kuwait City", AS, 0}, {"Kyrgyzstan", "Bishkek", AS, 0},
    {"Laos", "Vientiane", AS, 0}, {"Lebanon", "Beirut", AS, 0},
    {"Malaysia", "Kuala Lumpur", AS, 0}, {"Maldives", "Male", AS, 0},
    {"Mongolia", "Ulaanbaatar", AS, 0}, {"Myanmar", "Naypyidaw", AS, 0},
    {"Nepal", "Kathmandu", AS, 0}, {"North Korea", "Pyongyang", AS, 0},
    {"Oman", "Muscat", AS, 0}, {"Pakistan", "Islamabad", AS, 1},
    {"Philippines", "Manila", AS, 1}, {"Qatar", "Doha", AS, 0},
    {"Saudi Arabia", "Riyadh", AS, 1}, {"South Korea", "Seoul", AS, 1},
    {"Syria", "Damascus", AS, 0}, {"Tajikistan", "Dushanbe", AS, 0},
    {"Thailand", "Bangkok", AS, 1}, {"Timor-Leste", "Dili", AS, 0},
    {"Turkey", "Ankara", AS, 1}, {"Turkmenistan", "Ashgabat", AS, 0},
    {"United Arab Emirates", "Abu Dhabi", AS, 1}, {"Uzbekistan", "Tashkent", AS, 0},
    {"Vietnam", "Hanoi", AS, 1},
    // ---- Africa ----
    {"Algeria", "Algiers", AF, 1}, {"Angola", "Luanda", AF, 0},
    {"Botswana", "Gaborone", AF, 0}, {"Burkina Faso", "Ouagadougou", AF, 0},
    {"Benin", "Porto-Novo", AF, 0,
     "Porto-Novo is the official capital of Benin. The government sits in Cotonou."},
    {"Burundi", "Gitega", AF, 0,
     "Gitega is the political capital of Burundi since 2019. Bujumbura is the economic capital."},
    {"Cote d'Ivoire", "Yamoussoukro", AF, 0,
     "Yamoussoukro is the official capital of Cote d'Ivoire. Abidjan is the economic centre."},
    {"Equatorial Guinea", "Ciudad de la Paz", AF, 0,
     "Ciudad de la Paz became the capital of Equatorial Guinea by decree in January 2026. Malabo was the capital before."},
    {"Eswatini", "Mbabane", AF, 0,
     "Mbabane is the administrative capital of Eswatini. Lobamba is the legislative and royal capital."},
    {"South Africa", "Pretoria", AF, 1,
     "South Africa has 3 capitals: Pretoria (executive), Cape Town (legislative) and Bloemfontein (judicial)."},
    {"Cameroon", "Yaounde", AF, 0}, {"Cape Verde", "Praia", AF, 0},
    {"Central African Republic", "Bangui", AF, 0}, {"Chad", "N'Djamena", AF, 0},
    {"Comoros", "Moroni", AF, 0}, {"Democratic Republic of the Congo", "Kinshasa", AF, 0},
    {"Republic of the Congo", "Brazzaville", AF, 0}, {"Egypt", "Cairo", AF, 1},
    {"Eritrea", "Asmara", AF, 0}, {"Ethiopia", "Addis Ababa", AF, 1},
    {"Gabon", "Libreville", AF, 0}, {"Gambia", "Banjul", AF, 0},
    {"Ghana", "Accra", AF, 1}, {"Guinea", "Conakry", AF, 0},
    {"Guinea-Bissau", "Bissau", AF, 0}, {"Kenya", "Nairobi", AF, 1},
    {"Lesotho", "Maseru", AF, 0}, {"Liberia", "Monrovia", AF, 0},
    {"Libya", "Tripoli", AF, 1}, {"Madagascar", "Antananarivo", AF, 1},
    {"Malawi", "Lilongwe", AF, 0}, {"Mali", "Bamako", AF, 0},
    {"Mauritania", "Nouakchott", AF, 0}, {"Mauritius", "Port Louis", AF, 0},
    {"Morocco", "Rabat", AF, 1}, {"Mozambique", "Maputo", AF, 0},
    {"Namibia", "Windhoek", AF, 0}, {"Niger", "Niamey", AF, 0},
    {"Nigeria", "Abuja", AF, 1}, {"Rwanda", "Kigali", AF, 0},
    {"Senegal", "Dakar", AF, 1}, {"Seychelles", "Victoria", AF, 0},
    {"Sierra Leone", "Freetown", AF, 0}, {"Somalia", "Mogadishu", AF, 0},
    {"South Sudan", "Juba", AF, 0}, {"Sudan", "Khartoum", AF, 0},
    {"Tanzania", "Dodoma", AF, 0}, {"Togo", "Lome", AF, 0},
    {"Tunisia", "Tunis", AF, 0}, {"Uganda", "Kampala", AF, 0},
    {"Zambia", "Lusaka", AF, 0}, {"Zimbabwe", "Harare", AF, 0},
    // ---- North and Central America, Caribbean ----
    {"Antigua and Barbuda", "Saint John's", NA, 0}, {"Bahamas", "Nassau", NA, 0},
    {"Barbados", "Bridgetown", NA, 0}, {"Belize", "Belmopan", NA, 0},
    {"Canada", "Ottawa", NA, 1}, {"Costa Rica", "San Jose", NA, 0},
    {"Cuba", "Havana", NA, 1}, {"Dominica", "Roseau", NA, 0},
    {"Dominican Republic", "Santo Domingo", NA, 0}, {"El Salvador", "San Salvador", NA, 0},
    {"Grenada", "Saint George's", NA, 0}, {"Guatemala", "Guatemala City", NA, 0},
    {"Haiti", "Port-au-Prince", NA, 0}, {"Honduras", "Tegucigalpa", NA, 0},
    {"Jamaica", "Kingston", NA, 1}, {"Mexico", "Mexico City", NA, 1},
    {"Nicaragua", "Managua", NA, 0}, {"Panama", "Panama City", NA, 0},
    {"Saint Kitts and Nevis", "Basseterre", NA, 0}, {"Saint Lucia", "Castries", NA, 0},
    {"Saint Vincent and the Grenadines", "Kingstown", NA, 0},
    {"Trinidad and Tobago", "Port of Spain", NA, 0},
    {"United States", "Washington, D.C.", NA, 1},
    // ---- South America ----
    {"Argentina", "Buenos Aires", SA, 1}, {"Brazil", "Brasilia", SA, 1},
    {"Bolivia", "Sucre", SA, 0,
     "Sucre is the constitutional capital of Bolivia. The government sits in La Paz."},
    {"Chile", "Santiago", SA, 1}, {"Colombia", "Bogota", SA, 1},
    {"Ecuador", "Quito", SA, 0}, {"Guyana", "Georgetown", SA, 0},
    {"Paraguay", "Asuncion", SA, 0}, {"Peru", "Lima", SA, 1},
    {"Suriname", "Paramaribo", SA, 0}, {"Uruguay", "Montevideo", SA, 0},
    {"Venezuela", "Caracas", SA, 1},
    // ---- Oceania ----
    {"Australia", "Canberra", OC, 1}, {"Fiji", "Suva", OC, 1},
    {"Kiribati", "Tarawa", OC, 0}, {"Marshall Islands", "Majuro", OC, 0},
    {"Micronesia", "Palikir", OC, 0}, {"New Zealand", "Wellington", OC, 1},
    {"Palau", "Ngerulmud", OC, 0}, {"Papua New Guinea", "Port Moresby", OC, 0},
    {"Samoa", "Apia", OC, 0}, {"Solomon Islands", "Honiara", OC, 0},
    {"Tonga", "Nuku'alofa", OC, 0}, {"Tuvalu", "Funafuti", OC, 0},
    {"Vanuatu", "Port Vila", OC, 0},
};
constexpr int N = sizeof(DATA) / sizeof(DATA[0]);
// QW_CAP_LATLON[] (CapitalsMap.h) must have one line per DATA[] entry, in the same order.
static_assert(sizeof(QW_CAP_LATLON) / sizeof(QW_CAP_LATLON[0]) == N,
              "CapitalsMap.h and DATA[] do not have the same number of countries");

// ---------------- random numbers ----------------
// In RTC memory: the generator must keep its state during deep sleep,
// otherwise every wake-up restarts it from the same value and the right
// answer always lands on the same button.
RTC_DATA_ATTR uint32_t rngState = 2463534242u;
inline uint32_t rnd() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}
inline void seedRng() {  // adds fresh entropy to the saved state
#if defined(ARDUINO_ARCH_ESP32)
  rngState ^= esp_random();
#endif
  rngState ^= (uint32_t)micros() * 2654435761u;
  rngState ^= (uint32_t)millis() * 2246822519u;
  if (!rngState) rngState = 2463534242u;
  for (int i = 0; i < 4; i++) rnd();
}

// ---------------- game state (survives deep sleep) ----------------
enum Phase : uint8_t { PH_MODE = 0, PH_LEVEL, PH_Q, PH_FB, PH_MAP, PH_END };
constexpr int QUESTIONS = 10;

RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t askCapital = 1;      // 1: guess the capital, 0: the country
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t qList[QUESTIONS];    // entries asked, in order
RTC_DATA_ATTR uint8_t qNum = 0;            // current question, 0..9
RTC_DATA_ATTR uint8_t opt[3];              // entries shown as answers A, B, C
RTC_DATA_ATTR uint8_t correctSlot = 0;     // 0..2
RTC_DATA_ATTR uint8_t picked = 0;          // slot the player chose
RTC_DATA_ATTR uint8_t score = 0;

inline bool inPool(int e) { return hard || DATA[e].famous; }

// Chooses the answers for question qNum.
inline void buildOptions() {
  seedRng();
  int e = qList[qNum];
  uint8_t cand[N];
  int n = 0;
  for (int i = 0; i < N; i++) {
    if (i == e) continue;
    if (hard ? (DATA[i].region == DATA[e].region) : (DATA[i].famous != 0)) cand[n++] = (uint8_t)i;
  }
  // two different wrong answers (n >= 12 in every case)
  int a = rnd() % n;
  int b = rnd() % (n - 1);
  if (b >= a) b++;
  correctSlot = (uint8_t)(rnd() % 3);
  uint8_t wrong[2] = {cand[a], cand[b]};
  int w = 0;
  for (int s = 0; s < 3; s++) opt[s] = (s == correctSlot) ? (uint8_t)e : wrong[w++];
}

inline void startGame() {
  seedRng();
  uint8_t pool[N];
  int n = 0;
  for (int i = 0; i < N; i++) if (inPool(i)) pool[n++] = (uint8_t)i;
  for (int k = 0; k < QUESTIONS; k++) {  // partial shuffle: 10 distinct entries
    int j = k + (int)(rnd() % (uint32_t)(n - k));
    uint8_t t = pool[k]; pool[k] = pool[j]; pool[j] = t;
    qList[k] = pool[k];
  }
  qNum = 0;
  score = 0;
  buildOptions();
  phase = PH_Q;
}

inline void start() {
  phase = PH_MODE;
  menuSel = 0;
}

inline void answer(int slot) {
  picked = (uint8_t)slot;
  if (slot == correctSlot) score++;
  phase = PH_FB;
}

inline void nextQuestion() {
  if (qNum + 1 >= QUESTIONS) {
    phase = PH_END;
    return;
  }
  qNum++;
  buildOptions();
  phase = PH_Q;
}

// ---------------- buttons: MENU = A, UP = B, DOWN = C ----------------
inline void press(int slot) {  // slot 0 = MENU, 1 = UP, 2 = DOWN
  switch (phase) {
    case PH_MODE:
      if (slot == 0) { askCapital = (menuSel == 0); menuSel = 0; phase = PH_LEVEL; }
      else menuSel = (uint8_t)(1 - menuSel);
      break;
    case PH_LEVEL:
      if (slot == 0) { hard = menuSel; startGame(); }
      else menuSel = (uint8_t)(1 - menuSel);
      break;
    case PH_Q:
      answer(slot);
      break;
    case PH_FB:
      phase = PH_MAP;  // after the correction: where is it on the map?
      break;
    case PH_MAP:
      nextQuestion();
      break;
    case PH_END:
      if (slot == 0) { phase = PH_MODE; menuSel = 0; }
      break;
  }
}

inline void up() { press(1); }
inline void down() { press(2); }
inline void select() { press(0); }

// ---------------- drawing ----------------
// Word wrap: fills lines[] (at most maxLines), returns how many lines.
inline int wrapText(const char *s, int maxChars, char lines[][36], int maxLines) {
  int n = 0, len = 0;
  lines[0][0] = 0;
  const char *p = s;
  while (*p && n < maxLines) {
    const char *e = p;
    while (*e && *e != ' ') e++;
    int wl = (int)(e - p);
    if (wl > maxChars) wl = maxChars;
    int need = (len ? len + 1 : 0) + wl;
    if (need > maxChars && len > 0) {
      if (++n >= maxLines) return maxLines;
      lines[n][0] = 0;
      len = 0;
      continue;
    }
    if (len) lines[n][len++] = ' ';
    memcpy(lines[n] + len, p, wl);
    len += wl;
    lines[n][len] = 0;
    p += (e - p > maxChars) ? maxChars : (e - p);
    while (*p == ' ') p++;
  }
  return lines[0][0] ? n + 1 : 0;
}

inline void centerText(Adafruit_GFX &d, const char *t, int16_t y, uint8_t size) {
  qwSetText(d, size);
  d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(t), size) / 2, y);
  d.print(t);
}

inline void drawOptionScreen(Adafruit_GFX &d, const char *title, const char *a,
                             const char *b, const char *note) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(6, 4);
  d.print(title);
  d.drawFastHLine(0, 24, 200, qwInk());
  const char *o[2] = {a, b};
  for (int i = 0; i < 2; i++) {
    int16_t y = 50 + i * 36;
    bool sel = (i == menuSel);
    if (sel) {
      d.fillRect(10, y - 6, 180, 28, qwInk());
      d.setTextColor(qwPaper());
    } else {
      d.drawRect(10, y - 6, 180, 28, qwInk());
      d.setTextColor(qwInk());
    }
    centerText(d, o[i], y, 2);
  }
  d.setTextColor(qwInk());
  centerText(d, note, 150, 1);
  centerText(d, "UP/DOWN=select  MENU=ok", 186, 1);
}

constexpr int16_t ROW_H = 34, BOX_H = 31, BOX_X = 4, BOX_W = 192, TXT_X = 34;

// One answer row. style: 0 plain, 1 right answer (black), 2 wrong choice (crossed).
inline void drawAnswer(Adafruit_GFX &d, int16_t y, char letter, const char *text,
                       int style) {
  uint16_t ink = qwInk(), paper = qwPaper();
  if (style == 1) d.fillRect(BOX_X, y, BOX_W, BOX_H, ink);
  else d.drawRect(BOX_X, y, BOX_W, BOX_H, ink);
  uint16_t fg = (style == 1) ? paper : ink;
  d.drawFastVLine(TXT_X - 6, y, BOX_H, style == 1 ? paper : ink);
  char l[2] = {letter, 0};
  d.setTextColor(fg);
  qwSetText(d, 2);
  d.setCursor(BOX_X + 8, y + 8);
  d.print(l);
  int len = (int)strlen(text);
  int16_t tw = 0;
  if (len <= 11) {  // size 2 fits (11 chars = 131 px, room for the marks)
    qwSetText(d, 2);
    tw = qwTextInkWidth((uint8_t)len, 2);
    d.setCursor(TXT_X, y + 8);
    d.print(text);
  } else {
    char lines[2][36];
    int n = wrapText(text, 24, lines, 2);
    qwSetText(d, 1);
    int16_t top = y + (n == 1 ? 11 : 6);
    for (int i = 0; i < n; i++) {
      d.setCursor(TXT_X, top + i * 10);
      d.print(lines[i]);
      int16_t w = qwTextInkWidth((uint8_t)strlen(lines[i]), 1);
      if (w > tw) tw = w;
    }
  }
  if (style == 2) {  // wrong choice: strike through + cross at the right
    d.drawFastHLine(TXT_X - 2, y + BOX_H / 2, tw + 4, ink);
    d.drawFastHLine(TXT_X - 2, y + BOX_H / 2 + 1, tw + 4, ink);
  }
  if (style != 0) {
    int16_t mx = BOX_X + BOX_W - 20, my = y + 8;
    for (int t = 0; t <= 1; t++) {
      if (style == 1) {  // check mark
        d.drawLine(mx + t, my + 8, mx + 5 + t, my + 14, paper);
        d.drawLine(mx + 5 + t, my + 14, mx + 14 + t, my, paper);
      } else {           // cross
        d.drawLine(mx + t, my, mx + 14 + t, my + 14, ink);
        d.drawLine(mx + t, my + 14, mx + 14 + t, my, ink);
      }
    }
  }
  d.setTextColor(ink);
}

inline const char *answerText(int slot) {
  const Entry &e = DATA[opt[slot]];
  return askCapital ? e.capital : e.country;
}

inline void drawQuestion(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  char h[40];
  snprintf(h, sizeof(h), "Q %d/%d   SCORE %d", qNum + 1, QUESTIONS, score);
  qwSetText(d, 1);
  d.setCursor(4, 3);
  d.print(h);
  centerText(d, "A = MENU   B = UP   C = DOWN", 13, 1);
  d.drawFastHLine(0, 23, 200, qwInk());

  const Entry &e = DATA[qList[qNum]];
  centerText(d, askCapital ? "WHAT IS THE CAPITAL OF:" : "WHICH COUNTRY HAS THE CAPITAL:",
             28, 1);
  char lines[3][36];
  int n = wrapText(askCapital ? e.country : e.capital, 15, lines, 3);
  int16_t y = 38 + (3 - n) * 8;
  for (int i = 0; i < n; i++) centerText(d, lines[i], y + i * 16, 2);
  for (int s = 0; s < 3; s++) drawAnswer(d, 88 + s * ROW_H, (char)('A' + s), answerText(s), 0);
}

inline void drawFeedback(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  bool ok = (picked == correctSlot);
  centerText(d, ok ? "CORRECT!" : "WRONG!", 4, 2);
  d.drawFastHLine(0, 23, 200, qwInk());
  for (int s = 0; s < 3; s++) {
    int style = (s == correctSlot) ? 1 : ((s == picked) ? 2 : 0);
    drawAnswer(d, 27 + s * ROW_H, (char)('A' + s), answerText(s), style);
  }
  const Entry &e = DATA[qList[qNum]];
  char sentence[128];
  if (e.note) snprintf(sentence, sizeof(sentence), "%s", e.note);
  else snprintf(sentence, sizeof(sentence), "%s is the capital of %s.", e.capital, e.country);
  char lines[4][36];
  int n = wrapText(sentence, 31, lines, 4);
  qwSetText(d, 1);
  for (int i = 0; i < n; i++) {
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(lines[i]), 1) / 2, 134 + i * 10);
    d.print(lines[i]);
  }
  centerText(d, "any button = see the map", 188, 1);
}

// Map screen: where is the capital? Equal Earth world map, the place is
// marked with a dot inside a ring (the map under the ring is cleared).
inline void drawMap(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  uint16_t ink = qwInk(), paper = qwPaper();
  d.setTextColor(ink);
  centerText(d, "WHERE IS IT?", 4, 2);
  d.drawFastHLine(0, 23, 200, ink);

  const int16_t MAP_Y = 30;
  d.drawRect(0, MAP_Y - 1, QW_MAP_W, QW_MAP_H + 2, ink);   // frame
  d.drawBitmap(0, MAP_Y, QW_MAP_BITS, QW_MAP_W, QW_MAP_H, ink);

  int e = qList[qNum];
  int16_t lat10 = QW_CAP_LATLON[e][0], lon10 = QW_CAP_LATLON[e][1];
  int px, py;
  qwMapProject(lat10 / 10.0f, lon10 / 10.0f, px, py);
  py += MAP_Y;
  if (px < 6) px = 6;                       // keep the whole ring on the screen
  if (px > QW_MAP_W - 7) px = QW_MAP_W - 7;
  d.fillCircle(px, py, 6, paper);   // clear the map around the marker
  d.drawCircle(px, py, 6, ink);
  d.drawCircle(px, py, 5, ink);
  d.fillCircle(px, py, 2, ink);

  // caption: capital, country, coordinates
  char cap[80];
  snprintf(cap, sizeof(cap), "%s, %s", DATA[e].capital, DATA[e].country);
  char lines[2][36];
  int n = wrapText(cap, 31, lines, 2);
  qwSetText(d, 1);
  for (int i = 0; i < n; i++) {
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(lines[i]), 1) / 2, 128 + i * 10);
    d.print(lines[i]);
  }
  char pos[32];
  int la = lat10 < 0 ? -lat10 : lat10, lo = lon10 < 0 ? -lon10 : lon10;
  snprintf(pos, sizeof(pos), "%d.%d %c   %d.%d %c", la / 10, la % 10, lat10 < 0 ? 'S' : 'N',
           lo / 10, lo % 10, lon10 < 0 ? 'W' : 'E');
  centerText(d, pos, 156, 1);
  centerText(d, qNum + 1 >= QUESTIONS ? "any button = see your score"
                                      : "any button = next question", 188, 1);
}

inline void drawEnd(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  centerText(d, "YOUR SCORE", 14, 2);
  char s[16];
  snprintf(s, sizeof(s), "%d / %d", score, QUESTIONS);
  centerText(d, s, 52, 4);
  const char *msg = score >= 9 ? "WORLD EXPERT!" : score >= 7 ? "GREAT JOB!"
                    : score >= 5 ? "NOT BAD" : "KEEP PRACTISING";
  centerText(d, msg, 106, 2);
  centerText(d, askCapital ? "Guess the capital" : "Guess the country", 140, 1);
  centerText(d, hard ? "HARD level" : "EASY level", 152, 1);
  centerText(d, "MENU=play again  BACK=exit", 186, 1);
}

inline void draw(Adafruit_GFX &d) {
  switch (phase) {
    case PH_MODE:
      drawOptionScreen(d, "CAPITALS", "GUESS CAPITAL", "GUESS COUNTRY",
                       "10 questions, 3 answers each");
      break;
    case PH_LEVEL:
      drawOptionScreen(d, "DIFFICULTY", "EASY", "HARD",
                       "HARD: all countries, same continent");
      break;
    case PH_Q:
      drawQuestion(d);
      break;
    case PH_FB:
      drawFeedback(d);
      break;
    case PH_MAP:
      drawMap(d);
      break;
    default:
      drawEnd(d);
      break;
  }
}

}  // namespace Cp
