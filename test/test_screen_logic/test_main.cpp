#include <Arduino.h>
#include <unity.h>

#include "constants.h"
#include "screen.h"

SYSTEM_STATUS currentStatus = NONE;

void test_set_render_buffer_binary_maps_to_255() {
  uint8_t input[ROWS * COLS] = {0};
  input[0] = 1;
  input[15] = 1;

  Screen.setRenderBuffer(input, false);

  TEST_ASSERT_EQUAL_UINT8(255, Screen.getBufferIndex(0));
  TEST_ASSERT_EQUAL_UINT8(255, Screen.getBufferIndex(15));
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(1));
}

void test_set_pixel_with_zero_brightness_turns_pixel_off() {
  Screen.clear();
  Screen.setPixel(1, 1, 1, 0);
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(1 + COLS));
}

void test_clear_rect_clips_negative_coordinates() {
  uint8_t full[ROWS * COLS];
  memset(full, 255, sizeof(full));
  Screen.setRenderBuffer(full, true);

  Screen.clearRect(-2, -2, 4, 4);

  // Only the visible top-left 2x2 area should be cleared.
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(0));
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(1));
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(COLS));
  TEST_ASSERT_EQUAL_UINT8(0, Screen.getBufferIndex(COLS + 1));
  TEST_ASSERT_EQUAL_UINT8(255, Screen.getBufferIndex(COLS + 2));
}

void test_brightness_zero_enables_off_state_and_recovery() {
  Screen.setBrightness(64, false);
  TEST_ASSERT_FALSE(Screen.isPoweredOff());

  Screen.setBrightness(0, false);
  TEST_ASSERT_TRUE(Screen.isPoweredOff());

  Screen.setBrightness(32, false);
  TEST_ASSERT_FALSE(Screen.isPoweredOff());
}

void setup() {
  delay(2000);
  UNITY_BEGIN();

  RUN_TEST(test_set_render_buffer_binary_maps_to_255);
  RUN_TEST(test_set_pixel_with_zero_brightness_turns_pixel_off);
  RUN_TEST(test_clear_rect_clips_negative_coordinates);
  RUN_TEST(test_brightness_zero_enables_off_state_and_recovery);

  UNITY_END();
}

void loop() {}
