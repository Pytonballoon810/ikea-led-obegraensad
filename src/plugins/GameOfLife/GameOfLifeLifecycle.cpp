#include "plugins/GameOfLifePlugin.h"

void GameOfLifePlugin::setup()
{
  Screen.clear();

  this->generationsRemaining = 80;
  this->ticksInSeed = 0;
  this->activeSeed = static_cast<SeedPreset>(random(5));
  seedPreset(this->activeSeed);

  drawBoard();
  this->next();
}

void GameOfLifePlugin::loop()
{
  if (this->generationsRemaining > 0)
    this->generationsRemaining--;

  this->ticksInSeed++;

  if (this->activeSeed == SEED_JET_LAUNCHER && (this->ticksInSeed % 10) == 0)
  {
    // Periodically emit moving jets from the left edge.
    spawnJetFromLeft();
  }

  this->next();
  drawBoard();
  delay(150);

  if (this->generationsRemaining == 0)
  {
    this->setup();
  }
}

const char *GameOfLifePlugin::getName() const
{
  return "GameOfLife";
}
