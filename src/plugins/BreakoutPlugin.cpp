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

void BreakoutPlugin::updateBall()
{
  if ((millis() - this->lastBallUpdate) < this->ballDelay)
  {
    return;
  }

  this->lastBallUpdate = millis();
  const int prevBallX = this->ball.x;
  const int prevBallY = this->ball.y;

  // Clear prior trail footprint before drawing new frame's trail.
  for (int i = 0; i < this->TRAIL_LENGTH; i++)
  {
    const int tx = this->ballTrail[i].x;
    const int ty = this->ballTrail[i].y;
    if (tx < 0 || ty < 0)
      continue;
    if (!isForegroundCell(tx, ty))
    {
      Screen.setPixelAtIndex(ty * this->X_MAX + tx, this->LED_TYPE_OFF);
    }
  }

  // Erase old ball pixel before recomputing position.
  if (!isForegroundCell(this->ball.x, this->ball.y))
  {
    Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_OFF);
  }

  int dx = this->ballMovement[0];
  int dy = this->ballMovement[1];
  const uint8_t destroyedBefore = this->destroyedBricks;
  bool verticalBounceOccurred = false;

  // Keep a short-lived spin impulse after paddle contact for a curved feel.
  if (this->curveFramesRemaining > 0 && this->curveDirection != 0)
  {
    dx += this->curveDirection;
    if (dx > 1)
      dx = 1;
    if (dx < -1)
      dx = -1;
    this->curveFramesRemaining--;
  }

  int nx = this->ball.x + dx;
  int ny = this->ball.y + dy;

  // Wall collisions.
  if (nx < 0 || nx > (this->X_MAX - 1))
  {
    dx *= -1;
    nx = this->ball.x + dx;
  }
  if (ny < 0)
  {
    dy *= -1;
    ny = this->ball.y + dy;
    verticalBounceOccurred = true;
  }

  // Paddle collision at bottom row.
  if (dy > 0 && ny == (this->Y_MAX - 1))
  {
    const int paddleLeft = this->paddle[0].x;
    const int paddleRight = this->paddle[this->PADDLE_WIDTH - 1].x;
    if (nx >= paddleLeft && nx <= paddleRight)
    {
      // Physics-style reflection on a horizontal paddle: keep horizontal
      // component, invert vertical component.
      dy = -abs(dy == 0 ? -1 : dy);
      verticalBounceOccurred = true;

      // Explicit corner rebounds make edge hits reliably kick outward.
      if (nx == paddleLeft)
      {
        dx = -1;
      }
      else if (nx == paddleRight)
      {
        dx = 1;
      }

      // Add spin from paddle motion so moving hits curve the trajectory.
      if (this->lastPaddleMoveDirection != 0)
      {
        dx += this->lastPaddleMoveDirection;
        if (dx > 1)
          dx = 1;
        if (dx < -1)
          dx = -1;
        if (dx == 0)
          dx = this->lastPaddleMoveDirection;

        this->curveDirection = this->lastPaddleMoveDirection;
        this->curveFramesRemaining = 3;
      }
      else
      {
        this->curveDirection = 0;
        this->curveFramesRemaining = 0;

        // Add slight controlled variation for center/neutral hits so
        // trajectories don't repeat the same lane every rally.
        if (dx == 0 || (random(100) < 25))
        {
          dx = (random(2) == 0) ? -1 : 1;
        }
      }

      if (this->randomizeNextPaddleBounce)
      {
        // Break repeated loop patterns by forcing a fresh outgoing angle.
        dx = (dx >= 0) ? -1 : 1;
        this->curveDirection = dx;
        this->curveFramesRemaining = 4;
        this->randomizeNextPaddleBounce = false;
      }

      nx = this->ball.x + dx;
      ny = this->ball.y + dy;

      // Track paddle-contact sequence and detect common A-B-A-B / same-spot
      // ping-pong patterns that path overlays may miss.
      recordPaddleHitX(nx);
      if (this->samePaddleHitStreak >= 2 || hasAlternatingPaddlePattern())
      {
        this->randomizeNextPaddleBounce = true;
      }

      // End of one rally segment (between paddle contacts).
      evaluateRallyPathOnPaddleHit();
    }
  }

  // Brick collision: handle side-axis contacts even when the diagonal cell is
  // clear. This prevents diagonal travel lanes from skipping reachable bricks.
  const int hitX = brickIndexAt(this->ball.x + dx, this->ball.y);
  const int hitY = brickIndexAt(this->ball.x, this->ball.y + dy);
  const int hitDiag = brickIndexAt(nx, ny);
  if (hitX >= 0 || hitY >= 0 || hitDiag >= 0)
  {
    bool bounceX = false;
    bool bounceY = false;
    int brickToRemove = -1;

    if (hitX >= 0 && hitY < 0)
    {
      // Side-wall style impact into a brick column.
      bounceX = true;
      brickToRemove = hitX;
    }
    else if (hitY >= 0 && hitX < 0)
    {
      // Vertical impact into a brick row.
      bounceY = true;
      brickToRemove = hitY;
    }
    else if (hitX >= 0 && hitY >= 0)
    {
      // Corner squeeze between bricks: reflect both axes.
      bounceX = true;
      bounceY = true;
      brickToRemove = (hitDiag >= 0) ? hitDiag : hitY;
    }
    else
    {
      // Pure diagonal overlap with no axis-only hit.
      brickToRemove = hitDiag;
      // Alternate axis selection to avoid getting stuck in mirrored lanes.
      if (((this->ball.x + this->ball.y) & 0x1) == 0)
        bounceX = true;
      else
        bounceY = true;
    }

    if (brickToRemove >= 0)
      removeBrick(brickToRemove);

    if (bounceX)
      dx *= -1;
    if (bounceY)
    {
      dy *= -1;
      verticalBounceOccurred = true;
    }

    // Minor randomization on brick contacts keeps animation less repetitive
    // without destroying player-readability.
    if (random(100) < 18)
    {
      if (dx == 0)
        dx = (random(2) == 0) ? -1 : 1;
      else if (random(2) == 0)
        dx *= -1;
    }

    nx = this->ball.x + dx;
    ny = this->ball.y + dy;
  }

  // Lose condition: ball passed paddle area.
  if (ny >= this->Y_MAX)
  {
    this->end();
    return;
  }

  this->ballMovement[0] = dx;
  this->ballMovement[1] = dy;

  // Progressive speed-up: increase speed every N vertical bounces, capped to
  // keep gameplay winnable.
  if (verticalBounceOccurred)
  {
    this->speedRampCounter++;
    if (this->speedRampCounter >= this->SPEED_RAMP_BOUNCES)
    {
      this->speedRampCounter = 0;
      if (this->ballDelay > this->BALL_DELAY_MIN)
      {
        this->ballDelay--;
      }
    }
  }

  this->ball.x = nx;
  this->ball.y = ny;
  markRallyPathCell(this->ball.x, this->ball.y);

  const bool destroyedBrickThisFrame = (this->destroyedBricks != destroyedBefore);
  detectAndRecoverFromLoop(destroyedBrickThisFrame);

  shiftTrail(prevBallX, prevBallY);
  renderTrail();
  renderBall();

  if (this->destroyedBricks >= this->BRICK_AMOUNT)
  {
    this->gameState = this->GAME_STATE_WIN;
    this->winAnimStart = millis();
  }
}

void BreakoutPlugin::hitBrick(byte i)
{
  removeBrick(i);
}

void BreakoutPlugin::checkPaddleCollision()
{
  if ((this->paddle[0].y - 1) != this->ball.y)
  {
    return;
  }

  // reverse movement direction on the edges
  if (this->ballMovement[0] == 1 && (this->paddle[0].x - 1) == this->ball.x ||
      this->ballMovement[0] == -1 && (this->paddle[this->PADDLE_WIDTH - 1].x + 1) == this->ball.x)
  {
    this->ballMovement[0] *= -1;
    this->ballMovement[1] *= -1;

    return;
  }
  if (paddle[this->PADDLE_WIDTH / 2].x == this->ball.x)
  {
    this->ballMovement[0] = 0;
    this->ballMovement[1] *= -1;

    return;
  }

  for (byte i = 0; i < this->PADDLE_WIDTH; i++)
  {
    if (this->paddle[i].x == this->ball.x)
    {
      this->ballMovement[1] *= -1;
      if (random(2) == 0)
      {
        this->ballMovement[0] = 1;
      }
      else
      {
        this->ballMovement[0] = -1;
      }

      break;
    }
  }
}

void BreakoutPlugin::updatePaddle()
{
  int targetX = predictBallLandingX();
  const int centerX = this->paddle[this->PADDLE_WIDTH / 2].x;
  const int brickCenterX = getActiveBrickCenterX();

  int desiredDx = 0;
  if (brickCenterX > this->ball.x)
    desiredDx = 1;
  else if (brickCenterX < this->ball.x)
    desiredDx = -1;

  // If centered on brick centroid already, prefer sending toward the denser
  // opposite side of current drift to avoid repeating same-side channels.
  if (desiredDx == 0)
  {
    if (this->ballMovement[0] > 0)
      desiredDx = -1;
    else if (this->ballMovement[0] < 0)
      desiredDx = 1;
    else
      desiredDx = (this->ball.x < (this->X_MAX / 2)) ? 1 : -1;
  }

  // During descending approaches near the paddle, nudge alignment so contact
  // favors sending the ball toward remaining brick clusters.
  if (this->ballMovement[1] > 0 && this->ball.y >= (this->Y_MAX - 6))
  {
    // Shift paddle center slightly opposite the desired travel direction so
    // the impact point and spin bias cooperate.
    targetX -= desiredDx;
    targetX = max(0, min((int)this->X_MAX - 1, targetX));
  }

  int moveDirection = 0;
  if (targetX > centerX)
    moveDirection = 1;
  else if (targetX < centerX)
    moveDirection = -1;

  // In the impact window, keep paddle movement aligned to desiredDx to apply
  // intentional spin and break right-side lockups.
  if (moveDirection == 0 && this->ballMovement[1] > 0 && this->ball.y >= (this->Y_MAX - 3))
  {
    moveDirection = desiredDx;
  }

  if (moveDirection == 0)
  {
    this->lastPaddleMoveDirection = 0;
    return;
  }

  int newPaddlePosition = this->paddle[0].x + moveDirection;
  if (newPaddlePosition < 0 || newPaddlePosition + this->PADDLE_WIDTH > this->X_MAX)
  {
    this->lastPaddleMoveDirection = 0;
    return;
  }

  this->lastPaddleMoveDirection = moveDirection;

  for (byte i = 0; i < this->PADDLE_WIDTH; i++)
  {
    Screen.setPixelAtIndex(this->paddle[i].y * this->X_MAX + this->paddle[i].x, this->LED_TYPE_OFF);
  }
  for (byte i = 0; i < this->PADDLE_WIDTH; i++)
  {
    this->paddle[i].x += moveDirection;
  }
  renderPaddle();
}

void BreakoutPlugin::end()
{
  this->gameState = this->GAME_STATE_END;
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_ON);
}

void BreakoutPlugin::setup()
{
  this->gameState = this->GAME_STATE_END;
}

void BreakoutPlugin::loop()
{
  switch (this->gameState)
  {
  case this->GAME_STATE_LEVEL:
    this->newLevel();
    break;
  case this->GAME_STATE_RUNNING:
    this->updateBall();
    this->updatePaddle();
    delay(12);
    break;
  case this->GAME_STATE_WIN:
    this->playWinAnimation();
    delay(35);
    break;
  case this->GAME_STATE_END:
    this->initGame();
    break;
  }
}

const char *BreakoutPlugin::getName() const
{
  return "Breakout";
}
