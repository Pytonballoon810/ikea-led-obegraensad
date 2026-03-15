#include "PluginManager.h"
#include "scheduler.h"

#ifdef ENABLE_SERVER

AsyncWebSocket ws("/ws");

/**
 * Serialise the current system state to JSON and broadcast it to all
 * connected WebSocket clients.  Called on connection and after any
 * state-changing command.
 */
void sendInfo()
{
  DynamicJsonDocument jsonDocument(6144);
  if (currentStatus == NONE)
  {
    for (int j = 0; j < ROWS * COLS; j++)
    {
      jsonDocument["data"][j] = Screen.getRenderBuffer()[j];
    }
  }

  Plugin *active = pluginManager.getActivePlugin();
  if (!active)
    return;

  jsonDocument["status"] = currentStatus;
  jsonDocument["plugin"] = active->getId();
  jsonDocument["persist-plugin"] = pluginManager.getPersistedPluginId();
  jsonDocument["event"] = "info";
  jsonDocument["rotation"] = Screen.currentRotation;
  jsonDocument["brightness"] = Screen.getCurrentBrightness();
  jsonDocument["scheduleActive"] = Scheduler.isActive;

  JsonArray scheduleArray = jsonDocument.createNestedArray("schedule");
  for (const auto &item : Scheduler.schedule)
  {
    JsonObject scheduleItem = scheduleArray.createNestedObject();
    scheduleItem["pluginId"] = item.pluginId;
    scheduleItem["duration"] = item.duration / 1000; // Convert milliseconds to seconds
  }

  JsonArray plugins = jsonDocument.createNestedArray("plugins");

  std::vector<Plugin *> &allPlugins = pluginManager.getAllPlugins();
  for (Plugin *plugin : allPlugins)
  {
    JsonObject object = plugins.createNestedObject();

    object["id"] = plugin->getId();
    object["name"] = plugin->getName();
  }
  String output;
  serializeJson(jsonDocument, output);
  ws.textAll(output);
  jsonDocument.clear();
}

/**
 * ESPAsyncWebServer WebSocket event handler.
 *
 * IMPORTANT: this runs on the async TCP task (Core 0 on ESP32).  Any
 * access to shared state (renderBuffer_, plugin list, …) must be done
 * with the appropriate locking already in place on those objects.
 *
 * Frame reassembly is not supported – only single-frame messages are
 * processed.  Multi-frame messages are silently dropped.
 */
void onWsEvent(
    AsyncWebSocket *server,
    AsyncWebSocketClient *client,
    AwsEventType type,
    void *arg,
    uint8_t *data,
    size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    sendInfo();
  }

  if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len)
    {
      if (info->opcode == WS_BINARY && currentStatus == WSBINARY && info->len == 256)
      {
        Screen.setRenderBuffer(data, true);
      }
      else if (info->opcode == WS_TEXT)
      {
        // ESPAsyncWebServer does NOT guarantee a trailing NUL byte on the
        // payload.  Write into a stack buffer that has room for one so we
        // can safely pass it to deserializeJson / strcmp without a
        // heap allocation and without mutating the DMA-backed data buffer.
        if (len == 0 || len > 4096)
          return; // reject empty or oversized frames

        char textBuf[len + 1];
        memcpy(textBuf, data, len);
        textBuf[len] = '\0';

        DynamicJsonDocument wsRequest(6144);
        DeserializationError error = deserializeJson(wsRequest, textBuf);

        if (error)
        {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          return;
        }

        // Let the active plugin handle plugin-specific events first.
        Plugin *active = pluginManager.getActivePlugin();
        if (active)
          active->websocketHook(wsRequest);

        const char *event = wsRequest["event"];
        if (!event)
          return; // malformed request – no event field

        if (!strcmp(event, "plugin"))
        {
          int pluginId = wsRequest["plugin"];

          Scheduler.clearSchedule();
          pluginManager.setActivePluginById(pluginId);
          sendInfo();
        }
        else if (!strcmp(event, "persist-plugin"))
        {
          pluginManager.persistActivePlugin();
          sendInfo();
        }
        else if (!strcmp(event, "rotate"))
        {
          bool isRight = (bool)!strcmp(wsRequest["direction"], "right");
          Screen.setCurrentRotation((Screen.currentRotation + (isRight ? 1 : 3)) % 4, true);
          sendInfo();
        }
        else if (!strcmp(event, "info"))
        {
          sendInfo();
        }
        else if (!strcmp(event, "brightness"))
        {
          uint8_t brightness = wsRequest["brightness"].as<uint8_t>();
          Screen.setBrightness(brightness, true);
          sendInfo();
        }
      }
    }
  }
}

void initWebsocketServer(AsyncWebServer &server)
{
  server.addHandler(&ws);
  ws.onEvent(onWsEvent);
}

void cleanUpClients()
{
  ws.cleanupClients();
}

#endif