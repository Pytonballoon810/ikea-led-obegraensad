// Module: Autonomous snake game simulation plugin.
#include "plugins/SnakePlugin.h"

uint SnakePlugin::toIndex(uint8_t x, uint8_t y) const
{
  return (uint)y * SnakePlugin::BOARD_WIDTH + x;
}

uint8_t SnakePlugin::toX(uint index) const
{
  return index % SnakePlugin::BOARD_WIDTH;
}

uint8_t SnakePlugin::toY(uint index) const
{
  return index / SnakePlugin::BOARD_WIDTH;
}

bool SnakePlugin::isInside(uint8_t x, uint8_t y) const
{
  return x < SnakePlugin::BOARD_WIDTH && y < SnakePlugin::BOARD_HEIGHT;
}

uint SnakePlugin::nextPosition(uint head, uint8_t direction) const
{
  const uint8_t x = toX(head);
  const uint8_t y = toY(head);

  if (direction == 1)
  {
    if (y == 0)
      return SnakePlugin::BOARD_CELLS;
    return toIndex(x, y - 1);
  }
  if (direction == 2)
  {
    if (x + 1 >= SnakePlugin::BOARD_WIDTH)
      return SnakePlugin::BOARD_CELLS;
    return toIndex(x + 1, y);
  }
  if (direction == 3)
  {
    if (y + 1 >= SnakePlugin::BOARD_HEIGHT)
      return SnakePlugin::BOARD_CELLS;
    return toIndex(x, y + 1);
  }

  if (x == 0)
    return SnakePlugin::BOARD_CELLS;
  return toIndex(x - 1, y);
}

bool SnakePlugin::isOccupied(uint cell, bool tailIsFree) const
{
  if (this->position.empty())
    return false;

  const int lastBodyCell = tailIsFree ? (int)this->position.size() - 1 : (int)this->position.size();
  for (int i = 0; i < lastBodyCell; i++)
  {
    if (this->position[i] == cell)
      return true;
  }
  return false;
}

int SnakePlugin::manhattanDistance(uint a, uint b) const
{
  const int ax = toX(a);
  const int ay = toY(a);
  const int bx = toX(b);
  const int by = toY(b);
  return abs(ax - bx) + abs(ay - by);
}

int SnakePlugin::reachableSpace(uint startCell, bool tailIsFree) const
{
  if (startCell >= SnakePlugin::BOARD_CELLS)
    return 0;

  bool blocked[SnakePlugin::BOARD_CELLS] = {false};
  bool visited[SnakePlugin::BOARD_CELLS] = {false};
  uint queue[SnakePlugin::BOARD_CELLS] = {0};

  const int lastBodyCell = tailIsFree ? (int)this->position.size() - 1 : (int)this->position.size();
  for (int i = 0; i < lastBodyCell; i++)
  {
    if (this->position[i] < SnakePlugin::BOARD_CELLS)
      blocked[this->position[i]] = true;
  }

  if (blocked[startCell])
    return 0;

  int head = 0;
  int tail = 0;
  queue[tail++] = startCell;
  visited[startCell] = true;
  int count = 0;

  while (head < tail)
  {
    const uint current = queue[head++];
    count++;

    const uint8_t cx = toX(current);
    const uint8_t cy = toY(current);

    if (cy > 0)
    {
      const uint n = toIndex(cx, cy - 1);
      if (!visited[n] && !blocked[n])
      {
        visited[n] = true;
        queue[tail++] = n;
      }
    }
    if (cx + 1 < SnakePlugin::BOARD_WIDTH)
    {
      const uint n = toIndex(cx + 1, cy);
      if (!visited[n] && !blocked[n])
      {
        visited[n] = true;
        queue[tail++] = n;
      }
    }
    if (cy + 1 < SnakePlugin::BOARD_HEIGHT)
    {
      const uint n = toIndex(cx, cy + 1);
      if (!visited[n] && !blocked[n])
      {
        visited[n] = true;
        queue[tail++] = n;
      }
    }
    if (cx > 0)
    {
      const uint n = toIndex(cx - 1, cy);
      if (!visited[n] && !blocked[n])
      {
        visited[n] = true;
        queue[tail++] = n;
      }
    }
  }

  return count;
}
