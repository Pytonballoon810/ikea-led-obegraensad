#include "plugins/GameOfLifePlugin.h"

void GameOfLifePlugin::drawBoard()
{
  for (int i = 0; i < ROWS; i++)
  {
    for (int j = 0; j < COLS; j++)
    {
      Screen.setPixelAtIndex(i * COLS + j, this->buffer[i * COLS + j]);
    }
  }
}
