#pragma once

#include "PluginManager.h"

class BreakoutPlugin : public Plugin
{
private:
  static const uint8_t X_MAX = 16;
  static const uint8_t Y_MAX = 16;
  static const uint8_t BRICK_AMOUNT = X_MAX * 4;
  static const uint8_t BALL_DELAY_MAX = 110;
  static const uint8_t BALL_DELAY_MIN = 35;
  static const uint8_t BALL_DELAY_STEP = 4;
  static const uint8_t PADDLE_WIDTH = 5;
  static const uint8_t TRAIL_LENGTH = 3;
  static const uint8_t LED_TYPE_OFF = 0;
  static const uint8_t LED_TYPE_ON = 1;
  static const uint8_t GAME_STATE_RUNNING = 1;
  static const uint8_t GAME_STATE_END = 2;
  static const uint8_t GAME_STATE_LEVEL = 3;
  static const uint8_t GAME_STATE_WIN = 4;

  struct Coords
  {
    int8_t x;
    int8_t y;
  };

  unsigned char gameState;
  unsigned char level;
  unsigned char destroyedBricks;
  Coords paddle[BreakoutPlugin::PADDLE_WIDTH];
  Coords bricks[BreakoutPlugin::BRICK_AMOUNT];
  Coords ballTrail[BreakoutPlugin::TRAIL_LENGTH];
  Coords ball;

  int ballMovement[2];
  uint8_t ballDelay;
  uint8_t score;
  int8_t lastPaddleMoveDirection = 0;
  uint8_t curveFramesRemaining = 0;
  int8_t curveDirection = 0;
  unsigned long lastBallUpdate = 0;
  unsigned long winAnimStart = 0;
  uint8_t zeroDxStreak = 0;
  uint8_t repeatedStateStreak = 0;
  int8_t lastStateX = -1;
  int8_t lastStateY = -1;
  int8_t lastStateDx = 0;
  int8_t lastStateDy = 0;

  int brickIndexAt(int x, int y) const;
  bool isPaddleCell(int x, int y) const;
  bool isBrickCell(int x, int y) const;
  bool isForegroundCell(int x, int y) const;
  void clearTrail();
  void shiftTrail(int prevX, int prevY);
  void renderTrail();
  void removeBrick(int index);
  int predictBallLandingX() const;
  void renderPaddle();
  void renderBall();
  void playWinAnimation();
  void initGame();
  void initBricks();
  void resetLoopDetector();
  void detectAndRecoverFromLoop(bool destroyedBrickThisFrame);
  void newLevel();
  void updateBall();
  void hitBrick(unsigned char i);
  void checkPaddleCollision();
  void updatePaddle();
  void end();

public:
  void setup() override;
  void loop() override;
  const char *getName() const override;
};
