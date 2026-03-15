// Module: Firework particle animation plugin.
#include "plugins/FireworkPlugin.h"

void FireworkPlugin::drawExplosion(int x, int y, int maxRadius, int brightness)
{
  if (maxRadius <= 0 || brightness <= 0)
  {
    return;
  }

  const int radiusSquared = maxRadius * maxRadius;
  const int innerRadius = max(1, (maxRadius * 35) / 100);
  const int middleRadius = max(1, (maxRadius * 70) / 100);
  const int innerSquared = innerRadius * innerRadius;
  const int middleSquared = middleRadius * middleRadius;

  // Render concentric spheres: bright core, medium halo, dim outer shell.
  for (int i = 0; i < 16; i++)
  {
    for (int j = 0; j < 16; j++)
    {
      const int dx = i - x;
      const int dy = j - y;
      const int distanceSquared = (dx * dx) + (dy * dy);

      if (distanceSquared > radiusSquared)
      {
        continue;
      }

      int sphereBrightness = (brightness * 35) / 100;
      if (distanceSquared <= middleSquared)
      {
        sphereBrightness = (brightness * 65) / 100;
      }
      if (distanceSquared <= innerSquared)
      {
        sphereBrightness = brightness;
      }

      Screen.setPixel(i, j, 1, sphereBrightness);
    }
  }
}

void FireworkPlugin::explode(int x, int y)
{
  int maxRadius = random(3, 6);

  for (int radius = 1; radius <= maxRadius; radius++)
  {
    drawExplosion(x, y, radius, 255);
#ifdef ESP32
    vTaskDelay(pdMS_TO_TICKS(explosionDelay));
#else
    delay(explosionDelay);
#endif
  }

  for (int brightness = 248; brightness >= 0; brightness -= 8)
  {
    drawExplosion(x, y, maxRadius, brightness);
#ifdef ESP32
    vTaskDelay(pdMS_TO_TICKS(fadeDelay));
#else
    delay(fadeDelay);
#endif
  }
}

void FireworkPlugin::setup()
{
  Screen.clear();
}

void FireworkPlugin::loop()
{
  static int rocketY = 16;
  static int rocketX = 0;
  static bool rocketLaunched = false;

  unsigned long currentMillis = millis();

  if (!rocketLaunched)
  {
    if (currentMillis - previousMillis >= rocketDelay)
    {
      previousMillis = currentMillis;
      Screen.clear();
      Screen.setPixel(rocketX, rocketY, 1, 255);
      rocketY--;

      if (rocketY < random(8))
      {
        rocketLaunched = true;
        explode(rocketX, rocketY);
      }
    }
  }
  else
  {
    if (currentMillis - previousMillis >= explosionDuration)
    {
      rocketY = 16;
      rocketX = random(16);
      rocketLaunched = false;
      previousMillis = currentMillis;
    }
  }
}

const char *FireworkPlugin::getName() const
{
  return "Firework";
}
