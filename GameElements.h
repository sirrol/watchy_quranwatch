#pragma once
/*
  GameElements.h
  ------------------------------------------------------------
  Periodic table quiz for QuranWatch (part of the GAMES menu).

  A game = 10 questions, 3 possible answers each:
    A = MENU button, B = UP button, C = DOWN button.
  After each answer, the three answers are shown again (the right one
  in black, your wrong choice crossed out) with a short explanation.
  Any button = next question.

  Modes:
    MIXED    : the five kinds of questions below, at random
    SYMBOLS  : symbol -> name, name -> symbol
    NUMBERS  : atomic number -> name, name -> atomic number
    FAMILIES : name -> family (alkali metal, noble gas, ...)
  Levels:
    EASY : about 40 well-known elements, answers clearly different
    HARD : all 118 elements, answers that look alike

  Family questions only use elements whose family is not disputed:
  hydrogen, Sc, Y, La, Lu, Zn, Cd, Hg, Po, At, Ac, Lr and elements
  104-118 are never asked about their family.
  Names follow the IUPAC spelling (Aluminium, Caesium, Sulfur).

  The table is stored in atomic-number order: the atomic number of an
  element is its position in DATA[] + 1. Plain ASCII only.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif

namespace El {

enum Cat : uint8_t { ALK = 0, ALE, TRM, PTM, MTL, NMT, HAL, NBG, LAN, ACT, AMB = 255 };
constexpr int CAT_COUNT = 10;
static const char *const CAT_NAME[CAT_COUNT] = {
    "Alkali metal",  "Alkaline earth metal", "Transition metal", "Post-transition metal",
    "Metalloid",     "Nonmetal",             "Halogen",          "Noble gas",
    "Lanthanide",    "Actinide"};

struct Entry {
  const char *name;
  const char *sym;
  uint8_t cat;     // family, AMB = not asked / disputed
  uint8_t famous;  // 1 = used at EASY level
};

static const Entry DATA[] = {
    {"Hydrogen", "H", AMB, 1},      {"Helium", "He", NBG, 1},
    {"Lithium", "Li", ALK, 1},      {"Beryllium", "Be", ALE, 0},
    {"Boron", "B", MTL, 1},         {"Carbon", "C", NMT, 1},
    {"Nitrogen", "N", NMT, 1},      {"Oxygen", "O", NMT, 1},
    {"Fluorine", "F", HAL, 1},      {"Neon", "Ne", NBG, 1},
    {"Sodium", "Na", ALK, 1},       {"Magnesium", "Mg", ALE, 1},
    {"Aluminium", "Al", PTM, 1},    {"Silicon", "Si", MTL, 1},
    {"Phosphorus", "P", NMT, 1},    {"Sulfur", "S", NMT, 1},
    {"Chlorine", "Cl", HAL, 1},     {"Argon", "Ar", NBG, 1},
    {"Potassium", "K", ALK, 1},     {"Calcium", "Ca", ALE, 1},
    {"Scandium", "Sc", AMB, 0},     {"Titanium", "Ti", TRM, 1},
    {"Vanadium", "V", TRM, 0},      {"Chromium", "Cr", TRM, 1},
    {"Manganese", "Mn", TRM, 1},    {"Iron", "Fe", TRM, 1},
    {"Cobalt", "Co", TRM, 1},       {"Nickel", "Ni", TRM, 1},
    {"Copper", "Cu", TRM, 1},       {"Zinc", "Zn", AMB, 1},
    {"Gallium", "Ga", PTM, 0},      {"Germanium", "Ge", MTL, 0},
    {"Arsenic", "As", MTL, 0},      {"Selenium", "Se", NMT, 0},
    {"Bromine", "Br", HAL, 1},      {"Krypton", "Kr", NBG, 1},
    {"Rubidium", "Rb", ALK, 0},     {"Strontium", "Sr", ALE, 0},
    {"Yttrium", "Y", AMB, 0},       {"Zirconium", "Zr", TRM, 0},
    {"Niobium", "Nb", TRM, 0},      {"Molybdenum", "Mo", TRM, 0},
    {"Technetium", "Tc", TRM, 0},   {"Ruthenium", "Ru", TRM, 0},
    {"Rhodium", "Rh", TRM, 0},      {"Palladium", "Pd", TRM, 0},
    {"Silver", "Ag", TRM, 1},       {"Cadmium", "Cd", AMB, 0},
    {"Indium", "In", PTM, 0},       {"Tin", "Sn", PTM, 1},
    {"Antimony", "Sb", MTL, 0},     {"Tellurium", "Te", MTL, 0},
    {"Iodine", "I", HAL, 1},        {"Xenon", "Xe", NBG, 1},
    {"Caesium", "Cs", ALK, 0},      {"Barium", "Ba", ALE, 0},
    {"Lanthanum", "La", AMB, 0},    {"Cerium", "Ce", LAN, 0},
    {"Praseodymium", "Pr", LAN, 0}, {"Neodymium", "Nd", LAN, 0},
    {"Promethium", "Pm", LAN, 0},   {"Samarium", "Sm", LAN, 0},
    {"Europium", "Eu", LAN, 0},     {"Gadolinium", "Gd", LAN, 0},
    {"Terbium", "Tb", LAN, 0},      {"Dysprosium", "Dy", LAN, 0},
    {"Holmium", "Ho", LAN, 0},      {"Erbium", "Er", LAN, 0},
    {"Thulium", "Tm", LAN, 0},      {"Ytterbium", "Yb", LAN, 0},
    {"Lutetium", "Lu", AMB, 0},     {"Hafnium", "Hf", TRM, 0},
    {"Tantalum", "Ta", TRM, 0},     {"Tungsten", "W", TRM, 0},
    {"Rhenium", "Re", TRM, 0},      {"Osmium", "Os", TRM, 0},
    {"Iridium", "Ir", TRM, 0},      {"Platinum", "Pt", TRM, 1},
    {"Gold", "Au", TRM, 1},         {"Mercury", "Hg", AMB, 1},
    {"Thallium", "Tl", PTM, 0},     {"Lead", "Pb", PTM, 1},
    {"Bismuth", "Bi", PTM, 0},      {"Polonium", "Po", AMB, 0},
    {"Astatine", "At", AMB, 0},     {"Radon", "Rn", NBG, 0},
    {"Francium", "Fr", ALK, 0},     {"Radium", "Ra", ALE, 0},
    {"Actinium", "Ac", AMB, 0},     {"Thorium", "Th", ACT, 0},
    {"Protactinium", "Pa", ACT, 0}, {"Uranium", "U", ACT, 1},
    {"Neptunium", "Np", ACT, 0},    {"Plutonium", "Pu", ACT, 1},
    {"Americium", "Am", ACT, 0},    {"Curium", "Cm", ACT, 0},
    {"Berkelium", "Bk", ACT, 0},    {"Californium", "Cf", ACT, 0},
    {"Einsteinium", "Es", ACT, 0},  {"Fermium", "Fm", ACT, 0},
    {"Mendelevium", "Md", ACT, 0},  {"Nobelium", "No", ACT, 0},
    {"Lawrencium", "Lr", AMB, 0},   {"Rutherfordium", "Rf", AMB, 0},
    {"Dubnium", "Db", AMB, 0},      {"Seaborgium", "Sg", AMB, 0},
    {"Bohrium", "Bh", AMB, 0},      {"Hassium", "Hs", AMB, 0},
    {"Meitnerium", "Mt", AMB, 0},   {"Darmstadtium", "Ds", AMB, 0},
    {"Roentgenium", "Rg", AMB, 0},  {"Copernicium", "Cn", AMB, 0},
    {"Nihonium", "Nh", AMB, 0},     {"Flerovium", "Fl", AMB, 0},
    {"Moscovium", "Mc", AMB, 0},    {"Livermorium", "Lv", AMB, 0},
    {"Tennessine", "Ts", AMB, 0},   {"Oganesson", "Og", AMB, 0},
};
constexpr int N = sizeof(DATA) / sizeof(DATA[0]);  // 118

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
enum Phase : uint8_t { PH_MODE = 0, PH_LEVEL, PH_Q, PH_FB, PH_END };
// Question kinds
enum QType : uint8_t { Q_SYM2NAME = 0, Q_NAME2SYM, Q_NUM2NAME, Q_NAME2NUM, Q_FAMILY, Q_KINDS };
// Modes
enum Mode : uint8_t { M_MIXED = 0, M_SYMBOLS, M_NUMBERS, M_FAMILIES, M_COUNT };
static const char *const MODE_NAME[M_COUNT] = {"MIXED", "SYMBOLS", "NUMBERS", "FAMILIES"};

constexpr int QUESTIONS = 10;

RTC_DATA_ATTR uint8_t phase = PH_MODE;
RTC_DATA_ATTR uint8_t menuSel = 0;
RTC_DATA_ATTR uint8_t mode = M_MIXED;
RTC_DATA_ATTR uint8_t hard = 0;
RTC_DATA_ATTR uint8_t qList[QUESTIONS];   // element index asked, in order
RTC_DATA_ATTR uint8_t qKind[QUESTIONS];   // kind of each question
RTC_DATA_ATTR uint8_t qNum = 0;           // current question, 0..9
RTC_DATA_ATTR uint8_t opt[3];             // answers A, B, C (element index, or family for Q_FAMILY)
RTC_DATA_ATTR uint8_t correctSlot = 0;    // 0..2
RTC_DATA_ATTR uint8_t picked = 0;         // slot chosen by the player
RTC_DATA_ATTR uint8_t score = 0;

inline bool kindValid(int kind, int e) {
  return kind != Q_FAMILY || DATA[e].cat != AMB;
}

inline int pickKind() {
  switch (mode) {
    case M_SYMBOLS:  return (rnd() & 1) ? Q_NAME2SYM : Q_SYM2NAME;
    case M_NUMBERS:  return (rnd() & 1) ? Q_NAME2NUM : Q_NUM2NAME;
    case M_FAMILIES: return Q_FAMILY;
    default:         return (int)(rnd() % Q_KINDS);
  }
}

// "Looks alike" test used for the HARD level.
inline bool lookAlike(int i, int e) {
  int dz = i - e;
  if (dz < 0) dz = -dz;
  return dz <= 8 || DATA[i].cat == DATA[e].cat || DATA[i].sym[0] == DATA[e].sym[0];
}

// Chooses the answers for question qNum.
inline void buildOptions() {
  seedRng();
  int e = qList[qNum];
  int kind = qKind[qNum];
  uint8_t wrong[2];
  if (kind == Q_FAMILY) {
    int f = DATA[e].cat;
    int a = (int)(rnd() % (CAT_COUNT - 1));
    if (a >= f) a++;
    int b = (int)(rnd() % (CAT_COUNT - 2));
    if (b >= (f < a ? f : a)) b++;
    if (b >= (f < a ? a : f)) b++;
    wrong[0] = (uint8_t)a;
    wrong[1] = (uint8_t)b;
    correctSlot = (uint8_t)(rnd() % 3);
    int w = 0;
    for (int s = 0; s < 3; s++) opt[s] = (s == correctSlot) ? (uint8_t)f : wrong[w++];
    return;
  }
  uint8_t cand[N];
  int n = 0;
  for (int i = 0; i < N; i++) {
    if (i == e) continue;
    bool ok;
    if (hard) ok = (kind == Q_NAME2NUM) ? (i - e <= 6 && e - i <= 6) : lookAlike(i, e);
    else ok = DATA[i].famous != 0;
    if (ok) cand[n++] = (uint8_t)i;
  }
  int a = (int)(rnd() % (uint32_t)n);
  int b = (int)(rnd() % (uint32_t)(n - 1));
  if (b >= a) b++;
  wrong[0] = cand[a];
  wrong[1] = cand[b];
  correctSlot = (uint8_t)(rnd() % 3);
  int w = 0;
  for (int s = 0; s < 3; s++) opt[s] = (s == correctSlot) ? (uint8_t)e : wrong[w++];
}

inline void startGame() {
  seedRng();
  uint8_t pool[N];
  int n = 0;
  for (int i = 0; i < N; i++) if (hard || DATA[i].famous) pool[n++] = (uint8_t)i;
  for (int i = n - 1; i > 0; i--) {  // full shuffle
    int j = (int)(rnd() % (uint32_t)(i + 1));
    uint8_t t = pool[i]; pool[i] = pool[j]; pool[j] = t;
  }
  bool used[N] = {false};
  for (int q = 0; q < QUESTIONS; q++) {
    int kind = pickKind();
    int pick = -1;
    for (int k = 0; k < n && pick < 0; k++)
      if (!used[pool[k]] && kindValid(kind, pool[k])) pick = pool[k];
    if (pick < 0) {  // cannot happen with this table; stay safe anyway
      kind = Q_SYM2NAME;
      for (int k = 0; k < n && pick < 0; k++) if (!used[pool[k]]) pick = pool[k];
    }
    used[pick] = true;
    qList[q] = (uint8_t)pick;
    qKind[q] = (uint8_t)kind;
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
      if (slot == 0) { mode = menuSel; menuSel = 0; phase = PH_LEVEL; }
      else if (slot == 1) menuSel = (uint8_t)((menuSel + M_COUNT - 1) % M_COUNT);
      else menuSel = (uint8_t)((menuSel + 1) % M_COUNT);
      break;
    case PH_LEVEL:
      if (slot == 0) { hard = menuSel; startGame(); }
      else menuSel = (uint8_t)(1 - menuSel);
      break;
    case PH_Q:
      answer(slot);
      break;
    case PH_FB:
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

// Menu with 2 to 4 options (selected one in black).
inline void drawOptionScreen(Adafruit_GFX &d, const char *title, const char *const *o,
                             int count, const char *note) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(6, 4);
  d.print(title);
  d.drawFastHLine(0, 24, 200, qwInk());
  int16_t top = (count <= 2) ? 50 : 36;
  int16_t step = (count <= 2) ? 36 : 34;
  for (int i = 0; i < count; i++) {
    int16_t y = top + i * step;
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
  centerText(d, note, (count <= 2) ? 150 : 172, 1);
  centerText(d, "UP/DOWN=select  MENU=ok", 188, 1);
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

// Text of answer `slot` for the current question (buf of at least 24 chars).
inline const char *answerText(int slot, char *buf) {
  int kind = qKind[qNum];
  int v = opt[slot];
  switch (kind) {
    case Q_NAME2SYM: return DATA[v].sym;
    case Q_NAME2NUM: snprintf(buf, 8, "%d", v + 1); return buf;
    case Q_FAMILY:   return CAT_NAME[v];
    default:         return DATA[v].name;  // symbol->name, number->name
  }
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
  int kind = qKind[qNum];
  const char *head = "";
  switch (kind) {
    case Q_SYM2NAME: head = "WHAT IS THE ELEMENT WITH SYMBOL:"; break;
    case Q_NAME2SYM: head = "WHAT IS THE SYMBOL OF:"; break;
    case Q_NUM2NAME: head = "WHICH ELEMENT HAS ATOMIC NUMBER:"; break;
    case Q_NAME2NUM: head = "WHAT IS THE ATOMIC NUMBER OF:"; break;
    default:         head = "WHAT FAMILY IS THIS ELEMENT IN:"; break;
  }
  centerText(d, head, 28, 1);
  if (kind == Q_SYM2NAME) {
    centerText(d, e.sym, 46, 4);
  } else if (kind == Q_NUM2NAME) {
    char num[8];
    snprintf(num, sizeof(num), "%d", qList[qNum] + 1);
    centerText(d, num, 46, 4);
  } else {
    centerText(d, e.name, 52, 2);
  }
  char buf[8];
  for (int s = 0; s < 3; s++)
    drawAnswer(d, 88 + s * ROW_H, (char)('A' + s), answerText(s, buf), 0);
}

inline void drawFeedback(Adafruit_GFX &d) {
  d.fillScreen(qwPaper());
  d.setTextColor(qwInk());
  bool ok = (picked == correctSlot);
  centerText(d, ok ? "CORRECT!" : "WRONG!", 4, 2);
  d.drawFastHLine(0, 23, 200, qwInk());
  char buf[8];
  for (int s = 0; s < 3; s++) {
    int style = (s == correctSlot) ? 1 : ((s == picked) ? 2 : 0);
    drawAnswer(d, 27 + s * ROW_H, (char)('A' + s), answerText(s, buf), style);
  }
  const Entry &e = DATA[qList[qNum]];
  char sentence[96];
  if (e.cat != AMB)
    snprintf(sentence, sizeof(sentence), "%s (%s) is element number %d. Family: %s.",
             e.name, e.sym, qList[qNum] + 1, CAT_NAME[e.cat]);
  else
    snprintf(sentence, sizeof(sentence), "%s (%s) is element number %d.", e.name, e.sym,
             qList[qNum] + 1);
  char lines[4][36];
  int n = wrapText(sentence, 31, lines, 4);
  qwSetText(d, 1);
  for (int i = 0; i < n; i++) {
    d.setCursor(100 - qwTextInkWidth((uint8_t)strlen(lines[i]), 1) / 2, 134 + i * 10);
    d.print(lines[i]);
  }
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
  const char *msg = score >= 9 ? "CHEMISTRY WIZARD!" : score >= 7 ? "GREAT JOB!"
                    : score >= 5 ? "NOT BAD" : "KEEP PRACTISING";
  centerText(d, msg, 106, 2);
  char m[24];
  snprintf(m, sizeof(m), "Mode: %s", MODE_NAME[mode]);
  centerText(d, m, 140, 1);
  centerText(d, hard ? "HARD level" : "EASY level", 152, 1);
  centerText(d, "MENU=play again  BACK=exit", 186, 1);
}

inline void draw(Adafruit_GFX &d) {
  switch (phase) {
    case PH_MODE:
      drawOptionScreen(d, "ELEMENTS", MODE_NAME, M_COUNT, "10 questions, 3 answers each");
      break;
    case PH_LEVEL: {
      static const char *const lv[2] = {"EASY", "HARD"};
      drawOptionScreen(d, "DIFFICULTY", lv, 2, "HARD: all 118 elements");
      break;
    }
    case PH_Q:
      drawQuestion(d);
      break;
    case PH_FB:
      drawFeedback(d);
      break;
    default:
      drawEnd(d);
      break;
  }
}

}  // namespace El
