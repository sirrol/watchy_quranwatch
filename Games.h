#pragma once
/*
  Games.h
  ------------------------------------------------------------
  Mini games for QuranWatch. Open the GAMES menu with the
  top-right button (UP) from the watch face.

  HOW TO ADD A GAME
  1. Write your game in its own header (see GameUttt.h), with:
       - its state in RTC_DATA_ATTR variables, so it survives the
         deep sleep between two button presses;
       - void start()               : the game was just opened
       - void draw(Adafruit_GFX&)   : draw the WHOLE 200x200 screen
       - void up(), down(), select(): the three buttons
         (UP = top-right, DOWN = bottom-right, MENU = top-left)
     and put it in its own namespace.
  2. #include it below and add ONE line to QW_GAMES[].
  That is all: the menu, the BACK button (= back to the games menu)
  and the 10-minute return to the watch face are handled for you.

  Remember the e-ink screen: it redraws slowly (about a second), so
  games must be turn based, not real time, and a move should need as
  few button presses as possible.
*/

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "UiCommon.h"
// How long the UP/DOWN button was held for the press being handled
// (set by InputHandler.h before up()/down() is called).
inline unsigned long qwGameHeldMs = 0;

// Cursor steps for this press: a tap moves 1, a short hold 3, a long
// hold 6 - so a game can move quickly without waiting for the screen.
inline int qwGameSteps() {
  if (qwGameHeldMs > 1200) return 6;
  if (qwGameHeldMs > 400) return 3;
  return 1;
}

#include "GameUttt.h"
#include "GameOrderChaos.h"
#include "GameBlackHole.h"
#include "GameSequencium.h"
#include "GameDomineering.h"
#include "GameCapitals.h"
#include "GameElements.h"

struct QwGame {
  const char *name;                 // shown in the menu (max ~14 chars)
  void (*start)();
  void (*draw)(Adafruit_GFX &);
  void (*up)();                     // read qwGameSteps() for hold speed
  void (*down)();
  void (*select)();
  bool (*busy)();                   // optional: true while a game is in
                                    // progress (BACK then asks to confirm)
};

// Game registry - ONE line per game.
static const QwGame QW_GAMES[] = {
    {"ULTIMATE TTT", Uttt::start, Uttt::draw, Uttt::up, Uttt::down,
     Uttt::select,
     []() { return Uttt::phase == Uttt::PH_BOARD || Uttt::phase == Uttt::PH_CELL; }},
    {"ORDER & CHAOS", Oc::start, Oc::draw, Oc::up, Oc::down, Oc::select,
     []() { return Oc::phase == Oc::PH_CELL || Oc::phase == Oc::PH_SYM; }},
    {"BLACK HOLE", Bh::start, Bh::draw, Bh::up, Bh::down, Bh::select,
     []() { return Bh::phase == Bh::PH_PLAY; }},
    {"SEQUENCIUM", Sq::start, Sq::draw, Sq::up, Sq::down, Sq::select,
     []() { return Sq::phase == Sq::PH_CELL || Sq::phase == Sq::PH_SRC; }},
    {"DOMINEERING", Dm::start, Dm::draw, Dm::up, Dm::down, Dm::select,
     []() { return Dm::phase == Dm::PH_PLAY; }},
    {"CAPITALS QUIZ", Cp::start, Cp::draw, Cp::up, Cp::down, Cp::select,
     []() { return Cp::phase == Cp::PH_Q || Cp::phase == Cp::PH_FB ||
            Cp::phase == Cp::PH_MAP; }},
    {"ELEMENTS QUIZ", El::start, El::draw, El::up, El::down, El::select,
     []() { return El::phase == El::PH_Q || El::phase == El::PH_FB; }},
    // {"MY GAME", MyGame::start, MyGame::draw, MyGame::up, MyGame::down,
    //  MyGame::select, []() { return MyGame::inProgress; }},
    // (the last entry is optional: without it BACK leaves at once)
};
constexpr uint8_t QW_GAME_COUNT = sizeof(QW_GAMES) / sizeof(QW_GAMES[0]);

// "Leave this game?" box, drawn over the game when BACK was pressed
// during a game in progress (InputHandler.h / main.cpp).
inline void qwDrawLeaveBox(Adafruit_GFX &d) {
  d.fillRect(12, 70, 176, 60, qwPaper());
  d.drawRect(12, 70, 176, 60, qwInk());
  d.drawRect(14, 72, 172, 56, qwInk());
  d.setTextColor(qwInk());
  qwSetText(d, 2);
  d.setCursor(100 - qwTextInkWidth(11, 2) / 2, 80);
  d.print("LEAVE GAME?");
  qwSetText(d, 1);
  d.setCursor(100 - qwTextInkWidth(14, 1) / 2, 104);
  d.print("BACK = leave");
  d.setCursor(100 - qwTextInkWidth(14, 1) / 2, 114);
  d.print("MENU = stay");
}
