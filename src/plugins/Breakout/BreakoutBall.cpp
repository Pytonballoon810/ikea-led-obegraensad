#include "plugins/BreakoutPlugin.h"

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

    // Always clear previous trail footprint first, then redraw foreground.
    Screen.setPixelAtIndex(ty * this->X_MAX + tx, this->LED_TYPE_OFF);
  }

  // Erase old ball pixel before recomputing position.
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_OFF);

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

  const int remainingBricks = this->BRICK_AMOUNT - this->destroyedBricks;
  if (remainingBricks > 0 && remainingBricks <= this->MAGNET_BRICK_THRESHOLD)
  {
    const int targetX = getMagnetTargetX();

    // Endgame magnet: bias upward travel toward surviving bricks.
    if (dy < 0)
    {
      if (targetX > this->ball.x && dx < 1 && random(100) < this->MAGNET_STEER_CHANCE)
      {
        dx = 1;
      }
      else if (targetX < this->ball.x && dx > -1 && random(100) < this->MAGNET_STEER_CHANCE)
      {
        dx = -1;
      }
      else if (targetX == this->ball.x && random(100) < (this->MAGNET_STEER_CHANCE / 2))
      {
        dx = 0;
      }
    }
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

  // Safety: never leave the ball in a live brick cell after stacked contacts.
  const int overlapBrick = brickIndexAt(nx, ny);
  if (overlapBrick >= 0)
  {
    removeBrick(overlapBrick);
    dx *= -1;
    dy *= -1;
    verticalBounceOccurred = true;
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

  // Redraw static foreground every frame so dynamic clears cannot leave ghosts.
  renderBricks();
  renderPaddle();

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
