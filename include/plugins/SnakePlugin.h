#pragma once

#include "PluginManager.h"

class SnakePlugin : public Plugin
{
private:
  static const uint8_t BOARD_WIDTH = 16;
  static const uint8_t BOARD_HEIGHT = 16;
  static const uint16_t BOARD_CELLS = BOARD_WIDTH * BOARD_HEIGHT;
  static const uint8_t LED_TYPE_OFF = 0;
  static const uint8_t LED_TYPE_ON = 1;
  static const uint8_t GAME_STATE_RUNNING = 1;
  static const uint8_t GAME_STATE_END = 2;

  unsigned char gameState;
  unsigned char lastDirection = 0; // 0=unset 1=up 2=right 3=down 4 =left

  std::vector<uint> position = {240, 241, 242};

  uint8_t dot;

  void initGame();
  void newDot();
  uint toIndex(uint8_t x, uint8_t y) const;
  uint8_t toX(uint index) const;
  uint8_t toY(uint index) const;
  uint nextPosition(uint head, uint8_t direction) const;
  bool isInside(uint8_t x, uint8_t y) const;
  bool isOccupied(uint cell, bool tailIsFree) const;
  int manhattanDistance(uint a, uint b) const;
  int reachableSpace(uint startCell, bool tailIsFree) const;
  int evaluateMove(uint head, uint8_t direction) const;
  void findDirection();
  void moveSnake(uint newpos);
  void end();

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
