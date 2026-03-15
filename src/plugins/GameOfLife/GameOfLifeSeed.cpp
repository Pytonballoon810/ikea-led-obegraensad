// Module: Conway's Game of Life simulation plugin.
#include "plugins/GameOfLifePlugin.h"
#include <cstring>

void GameOfLifePlugin::clearBoard()
{
  memset(this->buffer, 0, sizeof(this->buffer));
  memset(this->previous, 0, sizeof(this->previous));
}

void GameOfLifePlugin::setCell(int row, int col, uint8_t alive)
{
  if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
    return;

  this->buffer[row * COLS + col] = alive ? 1 : 0;
}

void GameOfLifePlugin::seedRandom()
{
  for (int i = 0; i < ROWS * COLS; i++)
  {
    this->buffer[i] = (random(100) < 28) ? 1 : 0;
  }
}

void GameOfLifePlugin::spawnJetFromLeft()
{
  // Right-moving lightweight spaceship (LWSS), used as a periodic "jet".
  static const int8_t lwss[9][2] = {
      {0, 1}, {0, 4}, {1, 0}, {2, 0}, {2, 4}, {3, 0}, {3, 1}, {3, 2}, {3, 3}};

  const int originRow = 2 + random(max(1, ROWS - 6));
  const int originCol = 0;
  for (const auto &cell : lwss)
  {
    setCell(originRow + cell[0], originCol + cell[1]);
  }
}

void GameOfLifePlugin::seedPreset(SeedPreset preset)
{
  clearBoard();

  switch (preset)
  {
  case SEED_RANDOM:
    seedRandom();
    break;
  case SEED_GLIDER_DUEL:
    // Two gliders in opposite corners.
    setCell(1, 2);
    setCell(2, 3);
    setCell(3, 1);
    setCell(3, 2);
    setCell(3, 3);

    setCell(ROWS - 2, COLS - 3);
    setCell(ROWS - 3, COLS - 4);
    setCell(ROWS - 4, COLS - 2);
    setCell(ROWS - 4, COLS - 3);
    setCell(ROWS - 4, COLS - 4);
    break;
  case SEED_OSCILLATOR_GARDEN:
    // Blinker cluster.
    setCell(3, 3);
    setCell(3, 4);
    setCell(3, 5);
    setCell(7, 10);
    setCell(8, 10);
    setCell(9, 10);

    // Toads.
    setCell(5, 7);
    setCell(5, 8);
    setCell(5, 9);
    setCell(6, 6);
    setCell(6, 7);
    setCell(6, 8);

    // Beacon pair.
    setCell(10, 3);
    setCell(10, 4);
    setCell(11, 3);
    setCell(11, 4);
    setCell(12, 5);
    setCell(12, 6);
    setCell(13, 5);
    setCell(13, 6);
    break;
  case SEED_PULSAR:
    // Compact pulsar centered for a dramatic long oscillator.
    for (int d : {2, 3, 4, 8, 9, 10})
    {
      setCell(1, d);
      setCell(6, d);
      setCell(8, d);
      setCell(13, d);
      setCell(d, 1);
      setCell(d, 6);
      setCell(d, 8);
      setCell(d, 13);
    }
    break;
  case SEED_JET_LAUNCHER:
    // Stable station accents at the left edge plus periodic jet spawns.
    setCell(6, 0);
    setCell(6, 1);
    setCell(7, 0);
    setCell(7, 1);

    setCell(4, 2);
    setCell(5, 2);
    setCell(6, 2);

    setCell(8, 2);
    setCell(9, 2);
    setCell(10, 2);

    spawnJetFromLeft();
    break;
  }

  memcpy(this->previous, this->buffer, sizeof(this->buffer));
}
