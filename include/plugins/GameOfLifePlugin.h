#pragma once

#include "PluginManager.h"

class GameOfLifePlugin : public Plugin
{
private:
  enum SeedPreset : uint8_t
  {
    SEED_RANDOM = 0,
    SEED_GLIDER_DUEL = 1,
    SEED_OSCILLATOR_GARDEN = 2,
    SEED_PULSAR = 3,
    SEED_JET_LAUNCHER = 4,
  };

  uint8_t previous[ROWS * COLS];
  uint8_t buffer[ROWS * COLS];
  uint8_t generationsRemaining = 0;
  uint8_t ticksInSeed = 0;
  SeedPreset activeSeed = SEED_RANDOM;

  void clearBoard();
  void setCell(int row, int col, uint8_t alive = 1);
  void seedRandom();
  void seedPreset(SeedPreset preset);
  void spawnJetFromLeft();
  void drawBoard();
  uint8_t updateCell(int row, int col);
  uint8_t countNeighbours(int row, int col);
  void next();

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
