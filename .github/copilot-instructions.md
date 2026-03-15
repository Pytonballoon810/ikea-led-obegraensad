# Coding Agent Instructions — ikea-led-obegraensad Firmware

This file provides conventions and instructions for coding agents (GitHub Copilot, Claude, etc.) working in this repository or in the companion **Home Assistant addon** that controls this device.

---

## Plugin System

### How plugins work

Every visual mode on the 16×16 LED matrix is a **plugin** — a C++ class that inherits from `Plugin` (declared in `include/PluginManager.h`).

The device exposes the full plugin list at runtime via HTTP:

```
GET http://<device-ip>/api/info
```

Response (relevant excerpt):
```json
{
  "plugin": 3,
  "plugins": [
    { "id": 1, "name": "Draw" },
    { "id": 2, "name": "Breakout" },
    ...
  ]
}
```

The `name` field is the exact string returned by `Plugin::getName()`.  
The `id` is assigned by registration order — **IDs start at 1 and increment by 1 per `addPlugin()` call in `src/main.cpp`**.

### Canonical plugin list

`plugins.json` at the repo root is the machine-readable source of truth. It contains:
- Every plugin's numeric `id`, `name` string, header file, class name, and whether it requires WiFi/server (`requires_server`).
- Raw URL for the HA addon to reference:  
  `https://raw.githubusercontent.com/Pytonballoon810/ikea-led-obegraensad/main/plugins.json`

**Always keep `plugins.json` in sync when adding or removing plugins.**

### ID stability rule

> **Always append new plugins at the END of their registration block** in `src/main.cpp`.  
> Never insert a plugin between existing registrations. Inserting shifts all subsequent IDs and breaks any integration (Home Assistant addon, stored preferences, schedules) that references numeric IDs.

There are two registration blocks:
1. Always-registered (no server required) — lines after `pluginManager.addPlugin(new DrawPlugin())`.
2. `#ifdef ENABLE_SERVER` block — for plugins that need WiFi/NTP/internet.

New non-server plugins go at the end of block 1. New server-dependent plugins go at the end of block 2 (before `#endif`).

---

## Adding a New Plugin (Firmware Side)

Follow these 5 steps. Do not skip any.

### Step 1 — Create the header `include/plugins/MyPlugin.h`

```cpp
#pragma once
#include "PluginManager.h"

class MyPlugin : public Plugin
{
public:
  void setup() override;
  void loop() override;
  void teardown() override;        // optional but recommended — clear the screen
  const char *getName() const override;
  // void websocketHook(JsonDocument &request) override;  // optional
};
```

### Step 2 — Create the source `src/plugins/MyPlugin.cpp`

```cpp
#include "plugins/MyPlugin.h"

void MyPlugin::setup()
{
  Screen.clear();
}

void MyPlugin::loop()
{
  // called every frame — draw to Screen
}

void MyPlugin::teardown()
{
  Screen.clear();
}

const char *MyPlugin::getName() const
{
  return "My Plugin";   // ← this string is what the HA addon and /api/info return
}
```

Key screen API:
- `Screen.setPixel(x, y, 1, brightness)` — set pixel (0–15, 0–15, layer 1, brightness 0–255)
- `Screen.clear()` — blank the display
- `Screen.getRenderBuffer()` — read current 256-byte frame

### Step 3 — Register in `src/main.cpp`

Add `#include` with the other plugin headers:
```cpp
#include "plugins/MyPlugin.h"
```

Append `addPlugin` at the END of the correct block:
```cpp
// If it does NOT need WiFi:
pluginManager.addPlugin(new WaveBarsPlugin());   // existing last entry
pluginManager.addPlugin(new MyPlugin());         // ← append here

// If it DOES need WiFi (inside #ifdef ENABLE_SERVER):
pluginManager.addPlugin(new ArtNetPlugin());     // existing last entry
pluginManager.addPlugin(new MyPlugin());         // ← append here
```

### Step 4 — Update `plugins.json`

Add an entry at the end of the `plugins` array. The `id` is the next sequential number:
```json
{ "id": 31, "name": "My Plugin", "header": "plugins/MyPlugin.h", "class": "MyPlugin", "requires_server": false, "description": "Short description" }
```

### Step 5 — Verify build

```bash
pio run -e esp32dev
```

---

## Adding a New Plugin to the Home Assistant Addon

The HA addon needs to know the plugin `id` and `name`. There are two approaches:

### Approach A — Dynamic (preferred)

Have the HA integration call `GET /api/info` on the device to get the live plugin list. This is always up to date and never requires manual synchronisation.

### Approach B — Static option list

Reference `plugins.json` in this firmware repo to build the static option list. The `name` field must match exactly (it is case-sensitive). The `id` field is the number to send to the device.

To change the active plugin from the HA addon:
- **HTTP**: `PATCH http://<device-ip>/api/plugin?id=<id>`
- **WebSocket**: send `{ "event": "plugin", "plugin": <id> }`

---

## WebSocket Protocol (for HA addon agents)

Connect to `ws://<device-ip>/ws`.

On connect the device immediately sends the full state JSON:
```json
{
  "event": "info",
  "plugin": <active-plugin-id>,
  "plugins": [ { "id": 1, "name": "Draw" }, ... ],
  "brightness": 128,
  "rotation": 0,
  "scheduleActive": false,
  "schedule": [],
  "persist-plugin": 1,
  "data": [0, 0, ...]
}
```

Useful commands to send:
```json
{ "event": "plugin",         "plugin": 5 }           // switch to plugin id 5
{ "event": "brightness",     "brightness": 200 }      // set brightness 0-255
{ "event": "rotate",         "direction": "right" }   // rotate display
{ "event": "persist-plugin"                       }   // save active plugin across reboots
{ "event": "info"                                 }   // request fresh state
```

---

## HTTP API Summary

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/info` | Full state incl. plugin list |
| PATCH | `/api/plugin?id=<n>` | Switch active plugin |
| PATCH | `/api/brightness?value=<n>` | Set brightness (0–255) |
| GET | `/api/data` | Raw 256-byte pixel buffer |
| POST | `/api/schedule` | Set scheduler |
| GET | `/api/schedule/start` | Start scheduler |
| GET | `/api/schedule/stop` | Stop scheduler |
| GET | `/api/config` | Get device config (NTP, timezone, weather location) |
| POST | `/api/config` | Update device config (JSON body) |
