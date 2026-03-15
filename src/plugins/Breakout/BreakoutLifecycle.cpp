#include "plugins/BreakoutPlugin.h"

void BreakoutPlugin::end()
{
  this->gameState = this->GAME_STATE_END;
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_ON);
}

void BreakoutPlugin::setup()
{
  this->gameState = this->GAME_STATE_END;
}

void BreakoutPlugin::loop()
{
  switch (this->gameState)
  {
  case this->GAME_STATE_LEVEL:
    this->newLevel();
    break;
  case this->GAME_STATE_RUNNING:
    this->updateBall();
    this->updatePaddle();
    delay(12);
    break;
  case this->GAME_STATE_WIN:
    this->playWinAnimation();
    delay(35);
    break;
  case this->GAME_STATE_END:
    this->initGame();
    break;
  }
}

const char *BreakoutPlugin::getName() const
{
  return "Breakout";
}
