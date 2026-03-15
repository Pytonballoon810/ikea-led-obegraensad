// Module: Autoplay breakout game plugin with collision, AI paddle, and win/loss flow.
// copyright https://elektro.turanis.de/html/prj104/index.html
#include "plugins/BreakoutPlugin.h"

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
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_ON, 100);
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

  renderBall();
  this->ballMovement[0] = 1;
  this->ballMovement[1] = -1;
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
  // Erase old ball pixel before recomputing position.
  Screen.setPixelAtIndex(this->ball.y * this->X_MAX + this->ball.x, this->LED_TYPE_OFF);

  int dx = this->ballMovement[0];
  int dy = this->ballMovement[1];
  int nx = this->ball.x + dx;
  int ny = this->ball.y + dy;

  // Wall collisions.
  if (nx <= 0 || nx >= (this->X_MAX - 1))
  {
    dx *= -1;
    nx = this->ball.x + dx;
  }
  if (ny <= 0)
  {
    dy *= -1;
    ny = this->ball.y + dy;
  }

  // Paddle collision at bottom row.
  if (dy > 0 && ny == (this->Y_MAX - 1))
  {
    const int paddleLeft = this->paddle[0].x;
    const int paddleRight = this->paddle[this->PADDLE_WIDTH - 1].x;
    if (nx >= paddleLeft && nx <= paddleRight)
    {
      // Reflect upward and steer angle by hit position on paddle.
      dy = -1;
      const int center = this->paddle[this->PADDLE_WIDTH / 2].x;
      if (nx < center)
        dx = -1;
      else if (nx > center)
        dx = 1;
      else
        dx = 0;

      nx = this->ball.x + dx;
      ny = this->ball.y + dy;
    }
  }

  // Brick collision resolution checks X- and Y-axes independently so side
  // impacts reflect correctly instead of always using a simple pass-through.
  int hitDiag = brickIndexAt(nx, ny);
  int hitX = brickIndexAt(this->ball.x + dx, this->ball.y);
  int hitY = brickIndexAt(this->ball.x, this->ball.y + dy);

  if (hitDiag >= 0 || hitX >= 0 || hitY >= 0)
  {
    if (hitX >= 0)
    {
      removeBrick(hitX);
      dx *= -1;
    }
    if (hitY >= 0)
    {
      removeBrick(hitY);
      dy *= -1;
    }
    if (hitDiag >= 0 && hitDiag != hitX && hitDiag != hitY)
    {
      removeBrick(hitDiag);
      // Corner-only impact: reflect both axes for natural bounce.
      dx *= -1;
      dy *= -1;
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
  this->ball.x = nx;
  this->ball.y = ny;
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
  const int targetX = predictBallLandingX();
  const int centerX = this->paddle[this->PADDLE_WIDTH / 2].x;

  int moveDirection = 0;
  if (targetX > centerX)
    moveDirection = 1;
  else if (targetX < centerX)
    moveDirection = -1;

  if (moveDirection == 0)
    return;

  int newPaddlePosition = this->paddle[0].x + moveDirection;
  if (newPaddlePosition < 0 || newPaddlePosition + this->PADDLE_WIDTH > this->X_MAX)
    return;

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
    delay(25);
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
