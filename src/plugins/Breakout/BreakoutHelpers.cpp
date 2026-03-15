// Module: Autoplay breakout game plugin with collision, AI paddle, and win/loss flow.
// copyright https://elektro.turanis.de/html/prj104/index.html
#include "plugins/BreakoutPlugin.h"
#include <cstring>

int BreakoutPlugin::brickIndexAt(int x, int y) const
{
  for (int i = 0; i < this->BRICK_AMOUNT; i++)
  {
    if (this->bricks[i].x == x && this->bricks[i].y == y)
    {
      return i;
    }
  }
  return -1;
}

int BreakoutPlugin::getActiveBrickCenterX() const
{
  int sumX = 0;
  int count = 0;
  for (int i = 0; i < this->BRICK_AMOUNT; i++)
  {
    if (this->bricks[i].x >= 0 && this->bricks[i].y >= 0)
    {
      sumX += this->bricks[i].x;
      count++;
    }
  }

  if (count == 0)
    return this->X_MAX / 2;

  return sumX / count;
}

int BreakoutPlugin::getMagnetTargetX() const
{
  int bestIndex = -1;
  int bestScore = 32767;

  for (int i = 0; i < this->BRICK_AMOUNT; i++)
  {
    if (this->bricks[i].x < 0 || this->bricks[i].y < 0)
      continue;

    // Favor closer bricks, with extra weight for bricks above the ball where
    // upward trajectories can reach them quickly.
    int score = abs(this->bricks[i].x - this->ball.x) * 3;
    score += abs(this->bricks[i].y - this->ball.y);
    if (this->bricks[i].y <= this->ball.y)
      score -= 4;

    if (score < bestScore)
    {
      bestScore = score;
      bestIndex = i;
    }
  }

  if (bestIndex < 0)
    return this->X_MAX / 2;

  return this->bricks[bestIndex].x;
}

bool BreakoutPlugin::isPaddleCell(int x, int y) const
{
  for (int i = 0; i < this->PADDLE_WIDTH; i++)
  {
    if (this->paddle[i].x == x && this->paddle[i].y == y)
      return true;
  }
  return false;
}

bool BreakoutPlugin::isBrickCell(int x, int y) const
{
  return brickIndexAt(x, y) >= 0;
}

bool BreakoutPlugin::isForegroundCell(int x, int y) const
{
  return isPaddleCell(x, y) || isBrickCell(x, y);
}

void BreakoutPlugin::clearTrail()
{
  for (int i = 0; i < this->TRAIL_LENGTH; i++)
  {
    this->ballTrail[i].x = -1;
    this->ballTrail[i].y = -1;
  }
}

void BreakoutPlugin::shiftTrail(int prevX, int prevY)
{
  for (int i = this->TRAIL_LENGTH - 1; i > 0; i--)
  {
    this->ballTrail[i] = this->ballTrail[i - 1];
  }
  this->ballTrail[0].x = prevX;
  this->ballTrail[0].y = prevY;
}

void BreakoutPlugin::renderTrail()
{
  static const uint8_t trailBrightness[BreakoutPlugin::TRAIL_LENGTH] = {70, 34, 16};

  for (int i = 0; i < this->TRAIL_LENGTH; i++)
  {
    const int x = this->ballTrail[i].x;
    const int y = this->ballTrail[i].y;
    if (x < 0 || y < 0)
      continue;
    if (x == this->ball.x && y == this->ball.y)
      continue;
    if (isForegroundCell(x, y))
      continue;

    Screen.setPixelAtIndex(y * this->X_MAX + x, this->LED_TYPE_ON, trailBrightness[i]);
  }
}

void BreakoutPlugin::removeBrick(int index)
{
  if (index < 0 || index >= this->BRICK_AMOUNT)
    return;

  int x = this->bricks[index].x;
  int y = this->bricks[index].y;
  if (x < 0 || y < 0)
    return;

  Screen.setPixelAtIndex(y * this->X_MAX + x, this->LED_TYPE_OFF);
  this->bricks[index].x = -1;
  this->bricks[index].y = -1;
  this->score++;
  this->destroyedBricks++;

  if (this->ballDelay > this->BALL_DELAY_MIN)
  {
    this->ballDelay -= this->BALL_DELAY_STEP;
  }
}

int BreakoutPlugin::predictBallLandingX() const
{
  int x = this->ball.x;
  int y = this->ball.y;
  int dx = this->ballMovement[0];
  int dy = this->ballMovement[1];

  if (dy <= 0)
  {
    // When ball moves upward, pre-position near center to stay robust.
    return this->X_MAX / 2;
  }

  // Predict up to a reasonable number of steps to avoid long loops.
  for (int i = 0; i < 64; i++)
  {
    int nx = x + dx;
    int ny = y + dy;

    if (nx <= 0 || nx >= (this->X_MAX - 1))
    {
      dx *= -1;
      nx = x + dx;
    }

    if (ny <= 0)
    {
      dy *= -1;
      ny = y + dy;
    }

    x = nx;
    y = ny;

    if (y >= (this->Y_MAX - 2))
      return x;
  }

  return x;
}

void BreakoutPlugin::renderPaddle()
{
  for (byte i = 0; i < this->PADDLE_WIDTH; i++)
  {
    Screen.setPixelAtIndex(this->paddle[i].y * this->X_MAX + this->paddle[i].x, this->LED_TYPE_ON, 50);
  }
}

void BreakoutPlugin::renderBricks()
{
  for (int i = 0; i < this->BRICK_AMOUNT; i++)
  {
    if (this->bricks[i].x < 0 || this->bricks[i].y < 0)
      continue;

    Screen.setPixelAtIndex(this->bricks[i].y * this->X_MAX + this->bricks[i].x, this->LED_TYPE_ON, 50);
  }
}

void BreakoutPlugin::renderBall()
{
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_ON, 130);
}

void BreakoutPlugin::playWinAnimation()
{
  // Pulse/flood effect across the matrix for ~1 second.
  const unsigned long elapsed = millis() - this->winAnimStart;
  const int ring = (elapsed / 80) % (this->X_MAX / 2 + 1);

  Screen.clear();
  for (int y = 0; y < this->Y_MAX; y++)
  {
    for (int x = 0; x < this->X_MAX; x++)
    {
      const int dist = max(abs(x - (this->X_MAX / 2)), abs(y - (this->Y_MAX / 2)));
      if (dist <= ring)
      {
        Screen.setPixel(x, y, this->LED_TYPE_ON, 120);
      }
    }
  }

  if (elapsed > 1200)
  {
    this->gameState = this->GAME_STATE_END;
  }
}

void BreakoutPlugin::initGame()
{
  Screen.clear();

  this->ballDelay = this->BALL_DELAY_MAX;
  this->score = 0;
  this->level = 0;
  newLevel();
}

void BreakoutPlugin::initBricks()
{
  this->destroyedBricks = 0;
  for (byte i = 0; i < this->BRICK_AMOUNT; i++)
  {
    this->bricks[i].x = i % this->X_MAX;
    this->bricks[i].y = i / this->X_MAX;
    Screen.setPixelAtIndex(this->bricks[i].y * this->X_MAX + this->bricks[i].x, this->LED_TYPE_ON, 50);

    delay(25);
  }
}

void BreakoutPlugin::clearRallyPath(uint64_t *path)
{
  memset(path, 0, sizeof(uint64_t) * this->PATH_WORDS);
}

void BreakoutPlugin::markRallyPathCell(int x, int y)
{
  if (x < 0 || x >= this->X_MAX || y < 0 || y >= this->Y_MAX)
    return;

  const int index = y * this->X_MAX + x;
  const int word = index / 64;
  const int bit = index % 64;
  this->currentRallyPath[word] |= (uint64_t(1) << bit);
}

bool BreakoutPlugin::rallyPathIsEmpty(const uint64_t *path) const
{
  for (int i = 0; i < this->PATH_WORDS; i++)
  {
    if (path[i] != 0)
      return false;
  }
  return true;
}

bool BreakoutPlugin::rallyPathMatchesPrevious() const
{
  return memcmp(
             this->currentRallyPath,
             this->previousRallyPath,
             sizeof(uint64_t) * this->PATH_WORDS) == 0;
}

void BreakoutPlugin::recordPaddleHitX(int x)
{
  for (int i = 0; i < 3; i++)
  {
    this->recentPaddleHitX[i] = this->recentPaddleHitX[i + 1];
  }
  this->recentPaddleHitX[3] = (int8_t)x;

  if (this->paddleHitHistoryCount < 4)
    this->paddleHitHistoryCount++;

  if (this->recentPaddleHitX[2] == this->recentPaddleHitX[3])
    this->samePaddleHitStreak++;
  else
    this->samePaddleHitStreak = 0;
}

bool BreakoutPlugin::hasAlternatingPaddlePattern() const
{
  if (this->paddleHitHistoryCount < 4)
    return false;

  const int a = this->recentPaddleHitX[0];
  const int b = this->recentPaddleHitX[1];
  const int c = this->recentPaddleHitX[2];
  const int d = this->recentPaddleHitX[3];

  return (a == c && b == d && a != b);
}

void BreakoutPlugin::evaluateRallyPathOnPaddleHit()
{
  const bool currentEmpty = rallyPathIsEmpty(this->currentRallyPath);
  const bool previousEmpty = rallyPathIsEmpty(this->previousRallyPath);

  if (!currentEmpty && !previousEmpty && rallyPathMatchesPrevious())
  {
    this->repeatedPathOverlayStreak++;
  }
  else
  {
    this->repeatedPathOverlayStreak = 0;
  }

  if (this->repeatedPathOverlayStreak >= 2)
  {
    // Trigger diversification on the following paddle impact.
    this->randomizeNextPaddleBounce = true;
    this->repeatedPathOverlayStreak = 0;
  }

  memcpy(
      this->previousRallyPath,
      this->currentRallyPath,
      sizeof(uint64_t) * this->PATH_WORDS);
  clearRallyPath(this->currentRallyPath);
}

void BreakoutPlugin::resetLoopDetector()
{
  this->zeroDxStreak = 0;
  this->repeatedStateStreak = 0;
  this->lastStateX = -1;
  this->lastStateY = -1;
  this->lastStateDx = 0;
  this->lastStateDy = 0;
}

void BreakoutPlugin::detectAndRecoverFromLoop(bool destroyedBrickThisFrame)
{
  if (destroyedBrickThisFrame)
  {
    // Brick hits naturally change trajectories, so reset loop heuristics.
    resetLoopDetector();
    return;
  }

  if (this->ballMovement[0] == 0)
    this->zeroDxStreak++;
  else
    this->zeroDxStreak = 0;

  if (this->ball.x == this->lastStateX &&
      this->ball.y == this->lastStateY &&
      this->ballMovement[0] == this->lastStateDx &&
      this->ballMovement[1] == this->lastStateDy)
  {
    this->repeatedStateStreak++;
  }
  else
  {
    this->repeatedStateStreak = 0;
  }

  this->lastStateX = this->ball.x;
  this->lastStateY = this->ball.y;
  this->lastStateDx = this->ballMovement[0];
  this->lastStateDy = this->ballMovement[1];

  // If the ball keeps moving straight up/down for too long (or repeatedly
  // re-enters the same exact state), inject a small horizontal nudge.
  if (this->zeroDxStreak > 22 || this->repeatedStateStreak > 10)
  {
    this->ballMovement[0] = (this->ball.x < (this->X_MAX / 2)) ? 1 : -1;
    if (this->ballMovement[1] == 0)
      this->ballMovement[1] = -1;

    this->zeroDxStreak = 0;
    this->repeatedStateStreak = 0;
  }
}

void BreakoutPlugin::newLevel()
{
  Screen.clear();
  this->initBricks();

  for (byte i = 0; i < this->PADDLE_WIDTH; i++)
  {
    this->paddle[i].x = (this->X_MAX / 2) - (this->PADDLE_WIDTH / 2) + i;
    this->paddle[i].y = this->Y_MAX - 1;
  }
  renderPaddle();

  this->ball.x = this->paddle[1].x;
  this->ball.y = this->paddle[1].y - 1;
  this->clearTrail();
  this->resetLoopDetector();

  renderBall();
  this->ballMovement[0] = 1;
  this->ballMovement[1] = -1;
  this->speedRampCounter = 0;
  this->lastPaddleMoveDirection = 0;
  this->curveDirection = 0;
  this->curveFramesRemaining = 0;
  this->randomizeNextPaddleBounce = false;
  this->repeatedPathOverlayStreak = 0;
  this->paddleHitHistoryCount = 0;
  this->samePaddleHitStreak = 0;
  for (int i = 0; i < 4; i++)
    this->recentPaddleHitX[i] = -1;
  clearRallyPath(this->currentRallyPath);
  clearRallyPath(this->previousRallyPath);
  markRallyPathCell(this->ball.x, this->ball.y);
  this->lastBallUpdate = 0;

  this->level++;
  this->gameState = this->GAME_STATE_RUNNING;
}
