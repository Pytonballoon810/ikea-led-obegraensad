#include <Arduino.h>
#include <unity.h>

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>

#include "constants.h"
#include "PluginManager.h"
#include "scheduler.h"

SYSTEM_STATUS currentStatus = NONE;
PluginManager pluginManager;

namespace {
AsyncWebServer recoveryServer(80);
bool recoveryStarted = false;

void startOtaRecoveryModeOnce() {
  if (recoveryStarted) {
    return;
  }

  // Keep deterministic AP recovery mode, but also try to reconnect STA using
  // saved credentials so OTA via the normal device URL can still work.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("IKEA-Test-OTA", "adminadmin");
  WiFi.begin();

  const uint32_t staDeadline = millis() + 8000;
  while (WiFi.status() != WL_CONNECTED && millis() < staDeadline) {
    delay(100);
  }

  recoveryServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Test image OTA mode. Open /update to flash real firmware.");
  });

  ElegantOTA.begin(&recoveryServer);
  ElegantOTA.setAuth("admin", "admin");
  recoveryServer.begin();
  recoveryStarted = true;

  Serial.println("[TEST] OTA recovery AP: IKEA-Test-OTA");
  Serial.println("[TEST] OTA credentials: admin / admin");
  Serial.println("[TEST] OTA URL: http://192.168.4.1/update");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[TEST] OTA URL (STA): http://");
    Serial.print(WiFi.localIP());
    Serial.println("/update");
  } else {
    Serial.println("[TEST] STA reconnect unavailable; AP recovery remains active.");
  }
}
}  // namespace

class DummyPlugin : public Plugin {
public:
  explicit DummyPlugin(const char *name) : name_(name) {}

  void setup() override { setupCount++; }
  void teardown() override { teardownCount++; }
  void loop() override { loopCount++; }
  const char *getName() const override { return name_; }

  int setupCount = 0;
  int teardownCount = 0;
  int loopCount = 0;

private:
  const char *name_;
};

void resetScheduler() {
  Scheduler.clearSchedule(true);
  Scheduler.stop();
}

void test_scheduler_add_item_converts_seconds_to_milliseconds() {
  resetScheduler();

  Scheduler.addItem(7, 12);

  TEST_ASSERT_EQUAL_UINT32(1, Scheduler.schedule.size());
  TEST_ASSERT_EQUAL_INT(7, Scheduler.schedule[0].pluginId);
  TEST_ASSERT_EQUAL_UINT32(12000, Scheduler.schedule[0].duration);
}

void test_scheduler_set_schedule_json_populates_items() {
  resetScheduler();

  bool ok = Scheduler.setScheduleByJSONString(
      "[{\"pluginId\":2,\"duration\":3},{\"pluginId\":4,\"duration\":10}]");

  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_UINT32(2, Scheduler.schedule.size());
  TEST_ASSERT_EQUAL_INT(2, Scheduler.schedule[0].pluginId);
  TEST_ASSERT_EQUAL_UINT32(3000, Scheduler.schedule[0].duration);
  TEST_ASSERT_EQUAL_INT(4, Scheduler.schedule[1].pluginId);
  TEST_ASSERT_EQUAL_UINT32(10000, Scheduler.schedule[1].duration);
}

void test_scheduler_invalid_json_is_rejected() {
  resetScheduler();

  bool ok = Scheduler.setScheduleByJSONString("{broken json]");

  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_UINT32(0, Scheduler.schedule.size());
}

void test_scheduler_start_and_stop_toggle_active_state() {
  resetScheduler();
  Scheduler.addItem(1, 5);

  Scheduler.start();
  TEST_ASSERT_TRUE(Scheduler.isActive);

  Scheduler.stop();
  TEST_ASSERT_FALSE(Scheduler.isActive);
}

void test_plugin_manager_add_set_and_cycle_plugins() {
  // Reset to a fresh manager instance for deterministic IDs and order.
  pluginManager = PluginManager();

  DummyPlugin p1("P1");
  DummyPlugin p2("P2");

  int id1 = pluginManager.addPlugin(&p1);
  int id2 = pluginManager.addPlugin(&p2);

  TEST_ASSERT_EQUAL_INT(1, id1);
  TEST_ASSERT_EQUAL_INT(2, id2);

  pluginManager.setActivePluginById(id1);
  TEST_ASSERT_EQUAL_PTR(&p1, pluginManager.getActivePlugin());
  TEST_ASSERT_EQUAL_INT(1, p1.setupCount);

  pluginManager.activateNextPlugin();
  TEST_ASSERT_EQUAL_PTR(&p2, pluginManager.getActivePlugin());
  TEST_ASSERT_EQUAL_INT(1, p1.teardownCount);
  TEST_ASSERT_EQUAL_INT(1, p2.setupCount);

  pluginManager.activateNextPlugin();
  TEST_ASSERT_EQUAL_PTR(&p1, pluginManager.getActivePlugin());
  TEST_ASSERT_EQUAL_INT(1, p2.teardownCount);
  TEST_ASSERT_EQUAL_INT(2, p1.setupCount);
}

void setup() {
  delay(2000);
}

void loop() {
  static bool ran = false;
  if (ran) {
    delay(1000);
    return;
  }
  ran = true;

  UNITY_BEGIN();

  RUN_TEST(test_scheduler_add_item_converts_seconds_to_milliseconds);
  RUN_TEST(test_scheduler_set_schedule_json_populates_items);
  RUN_TEST(test_scheduler_invalid_json_is_rejected);
  RUN_TEST(test_scheduler_start_and_stop_toggle_active_state);
  RUN_TEST(test_plugin_manager_add_set_and_cycle_plugins);

  UNITY_END();

  startOtaRecoveryModeOnce();

  // Keep test image alive in OTA recovery mode so a real firmware can be
  // flashed without USB after test execution.
  for (;;) {
    ElegantOTA.loop();
    delay(20);
  }
}
