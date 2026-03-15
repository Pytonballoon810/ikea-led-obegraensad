#include "plugins/SnakePlugin.h"

int SnakePlugin::evaluateMove(uint head, uint8_t direction) const
{
  const uint next = nextPosition(head, direction);
  if (next >= SnakePlugin::BOARD_CELLS)
    return -100000;

  const bool eatsDot = (next == this->dot);
  const bool tailIsFree = !eatsDot;
  if (isOccupied(next, tailIsFree))
    return -100000;

  const int freeCells = reachableSpace(next, tailIsFree);
  if (freeCells == 0)
    return -90000;

  const int distance = manhattanDistance(next, this->dot);
  const uint8_t nx = toX(next);
  const uint8_t ny = toY(next);
  const int wallDistance = min(min(nx, (uint8_t)(SnakePlugin::BOARD_WIDTH - 1 - nx)),
                               min(ny, (uint8_t)(SnakePlugin::BOARD_HEIGHT - 1 - ny)));

  int score = 0;
  score += freeCells * 10;
  score -= distance * 14;
  score += wallDistance * 3;
  if (direction == this->lastDirection)
    score += 5;
  if (eatsDot)
    score += 240;

  // Strongly avoid moves that enter tiny pockets.
  if (freeCells <= (int)this->position.size() / 2)
    score -= 160;

  return score;
}

void SnakePlugin::findDirection()
{
  if (this->position.empty())
  {
    end();
    return;
  }

  const uint snakehead = this->position[this->position.size() - 1];

  uint8_t bestDirection = 0;
  int bestScore = -1000000;

  for (uint8_t direction = 1; direction <= 4; direction++)
  {
    const int score = evaluateMove(snakehead, direction);
    if (score > bestScore)
    {
      bestScore = score;
      bestDirection = direction;
    }
    else if (score == bestScore && this->lastDirection == direction)
    {
      bestDirection = direction;
    }
  }

  if (bestDirection == 0 || bestScore <= -90000)
  {
    // Killed yourself or no legal move.
    end();
    return;
  }

  const uint newHead = nextPosition(snakehead, bestDirection);
  if (newHead >= SnakePlugin::BOARD_CELLS)
  {
    end();
    return;
  }

  moveSnake(newHead);
  this->lastDirection = bestDirection;
}
