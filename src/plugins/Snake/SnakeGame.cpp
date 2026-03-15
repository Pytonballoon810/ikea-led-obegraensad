#include "plugins/SnakePlugin.h"

void SnakePlugin::initGame()
{
  Screen.clear();

  this->position = {240, 241, 242};
  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_ON);
  }

  newDot();
}

void SnakePlugin::newDot()
{
  while (true)
  {
    this->dot = random(0, SnakePlugin::BOARD_CELLS);
    bool occupied = false;
    for (const uint &n : this->position)
    {
      if (n == this->dot)
      {
        occupied = true;
        break;
      }
    }
    if (!occupied)
      break;
  }

  Screen.setPixelAtIndex(this->dot, SnakePlugin::LED_TYPE_ON, 40);

  this->gameState = SnakePlugin::GAME_STATE_RUNNING;
}

void SnakePlugin::moveSnake(uint newpos)
{
  if (newpos == this->dot)
  {
    Screen.setPixelAtIndex(this->dot, SnakePlugin::LED_TYPE_ON);
    this->position.push_back(newpos);
    newDot();
  }
  else
  {

    Screen.setPixelAtIndex(newpos, SnakePlugin::LED_TYPE_ON);
    this->position.push_back(newpos); // adding element (head) to snake

    Screen.setPixelAtIndex(this->position[0], SnakePlugin::LED_TYPE_OFF);
    this->position.erase(this->position.begin()); // removing first element (end) of snake
  }
}

void SnakePlugin::end()
{
  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_OFF);
  }
  delay(200);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_ON);
  }
  delay(200);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_OFF);
  }
  delay(200);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_ON);
  }
  delay(200);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_OFF);
  }
  delay(200);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_ON);
  }
  delay(500);

  for (const int &n : this->position)
  {
    Screen.setPixelAtIndex(n, SnakePlugin::LED_TYPE_OFF);
    delay(200);
  }

  delay(200);
  Screen.setPixelAtIndex(this->dot, SnakePlugin::LED_TYPE_OFF);
  delay(500);

  this->gameState = SnakePlugin::GAME_STATE_END;
}
