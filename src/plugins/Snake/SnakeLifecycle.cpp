#include "plugins/SnakePlugin.h"

void SnakePlugin::setup()
{
  this->gameState = SnakePlugin::GAME_STATE_END;
}

void SnakePlugin::loop()
{
  switch (this->gameState)
  {
  case SnakePlugin::GAME_STATE_RUNNING:
    this->findDirection();
    delay(100);
    break;
  case SnakePlugin::GAME_STATE_END:
    this->initGame();
    break;
  }
}

const char *SnakePlugin::getName() const
{
  return "Snake";
}
