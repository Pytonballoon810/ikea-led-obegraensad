#include "plugins/GameOfLifePlugin.h"
#include <cstring>

uint8_t GameOfLifePlugin::countNeighbours(int row, int col)
{
  int count = 0;
  for (int i = row - 1; i <= row + 1; i++)
  {
    for (int j = col - 1; j <= col + 1; j++)
    {
      if (i < 0 || i >= ROWS || j < 0 || j >= COLS)
        continue;

      count += this->previous[i * COLS + j];
    }
  }
  count -= this->previous[row * COLS + col];
  return count;
}

uint8_t GameOfLifePlugin::updateCell(int row, int col)
{
  uint8_t total = this->countNeighbours(row, col);
  const uint8_t current = this->previous[row * COLS + col];

  if (current == 1)
  {
    // Live cell survives with 2 or 3 neighbours.
    return (total == 2 || total == 3) ? 1 : 0;
  }

  if (total == 3)
  {
    // Dead cell is born with exactly 3 neighbours.
    return 1;
  }

  return 0;
}

void GameOfLifePlugin::next()
{
  memcpy(this->previous, this->buffer, sizeof(this->buffer));
  for (int i = 0; i < ROWS; i++)
  {
    for (int j = 0; j < COLS; j++)
    {
      this->buffer[i * COLS + j] = this->updateCell(i, j);
    }
  }
}
