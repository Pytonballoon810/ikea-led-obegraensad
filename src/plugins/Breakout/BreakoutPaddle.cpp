#include "plugins/BreakoutPlugin.h"

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
