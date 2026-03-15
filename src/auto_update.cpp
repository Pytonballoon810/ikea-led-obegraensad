#include "auto_update.h"

#if defined(ENABLE_SERVER) && defined(ESP32)

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <mbedtls/sha256.h>

#include "websocket.h"

namespace
{
const char *AUTO_UPDATE_NAMESPACE = "auto-update";
const char *AUTO_UPDATE_LAST_VERSION_KEY = "last-version";

Preferences autoUpdatePreferences;
String lastAppliedVersion;
unsigned long lastCheckMillis = 0;
bool firstCheckDone = false;

String normalizeHex(String input)
{
  input.trim();
  input.toLowerCase();
  input.replace(" ", "");
  input.replace("\n", "");
  input.replace("\r", "");
  return input;
}

bool isTimeToCheck()
{
  const unsigned long now = millis();

  if (!firstCheckDone)
  {
    return now >= AUTO_UPDATE_INITIAL_DELAY_MS;
  }

  return (now - lastCheckMillis) >= AUTO_UPDATE_CHECK_INTERVAL_MS;
}

bool fetchManifest(String &version, String &firmwareUrl, String &sha256)
{
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  if (!http.begin(client, AUTO_UPDATE_MANIFEST_URL))
  {
    Serial.println("[auto-update] Failed to initialize manifest request");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    Serial.printf("[auto-update] Manifest request failed: HTTP %d\n", code);
    http.end();
    return false;
  }

  DynamicJsonDocument doc(2048);
  const DeserializationError error = deserializeJson(doc, http.getString());
  http.end();

  if (error)
  {
    Serial.printf("[auto-update] Manifest parse error: %s\n", error.c_str());
    return false;
  }

  version = doc["version"].as<String>();
  firmwareUrl = doc["firmware_url"].as<String>();
  sha256 = normalizeHex(doc["sha256"].as<String>());

  if (version.isEmpty() || firmwareUrl.isEmpty() || sha256.length() != 64)
  {
    Serial.println("[auto-update] Manifest is missing required fields");
    return false;
  }

  return true;
}

bool applyFirmwareUpdate(const String &firmwareUrl, const String &expectedSha256)
{
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(20000);

  if (!http.begin(client, firmwareUrl))
  {
    Serial.println("[auto-update] Failed to initialize firmware request");
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK)
  {
    Serial.printf("[auto-update] Firmware request failed: HTTP %d\n", code);
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (!Update.begin(contentLength > 0 ? contentLength : UPDATE_SIZE_UNKNOWN))
  {
    Serial.printf("[auto-update] Update.begin failed: %s\n", Update.errorString());
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[1024];

  size_t writtenTotal = 0;
  int remaining = contentLength;

  mbedtls_sha256_context shaCtx;
  mbedtls_sha256_init(&shaCtx);
  mbedtls_sha256_starts(&shaCtx, 0);

  while (http.connected() && (remaining > 0 || remaining == -1))
  {
    const size_t available = stream->available();
    if (available == 0)
    {
      delay(1);
      continue;
    }

    const size_t toRead = min(available, sizeof(buffer));
    const int readBytes = stream->readBytes(buffer, toRead);
    if (readBytes <= 0)
    {
      continue;
    }

    if (Update.write(buffer, readBytes) != (size_t)readBytes)
    {
      Serial.printf("[auto-update] Update.write failed: %s\n", Update.errorString());
      Update.abort();
      mbedtls_sha256_free(&shaCtx);
      http.end();
      return false;
    }

    mbedtls_sha256_update(&shaCtx, buffer, readBytes);
    writtenTotal += (size_t)readBytes;

    if (remaining > 0)
    {
      remaining -= readBytes;
    }
  }

  uint8_t digest[32];
  mbedtls_sha256_finish(&shaCtx, digest);
  mbedtls_sha256_free(&shaCtx);

  String calculatedSha256;
  calculatedSha256.reserve(64);
  char hexBuffer[3] = {0, 0, 0};
  for (size_t i = 0; i < sizeof(digest); i++)
  {
    snprintf(hexBuffer, sizeof(hexBuffer), "%02x", digest[i]);
    calculatedSha256 += hexBuffer;
  }

  if (normalizeHex(calculatedSha256) != normalizeHex(expectedSha256))
  {
    Serial.println("[auto-update] SHA256 mismatch. Aborting update.");
    Update.abort();
    http.end();
    return false;
  }

  if (contentLength > 0 && writtenTotal != (size_t)contentLength)
  {
    Serial.println("[auto-update] Incomplete firmware download. Aborting update.");
    Update.abort();
    http.end();
    return false;
  }

  const bool success = Update.end();
  http.end();

  if (!success)
  {
    Serial.printf("[auto-update] Update.end failed: %s\n", Update.errorString());
    return false;
  }

  if (!Update.isFinished())
  {
    Serial.println("[auto-update] Update not finished");
    return false;
  }

  return true;
}
} // namespace

void initAutoUpdate()
{
  if (!AUTO_UPDATE_ENABLED)
  {
    return;
  }

  if (!autoUpdatePreferences.begin(AUTO_UPDATE_NAMESPACE, false))
  {
    Serial.println("[auto-update] Failed to initialize preferences");
    return;
  }

  lastAppliedVersion = autoUpdatePreferences.getString(AUTO_UPDATE_LAST_VERSION_KEY, "");
  Serial.printf("[auto-update] Current firmware version: %s\n", FIRMWARE_VERSION);
  if (lastAppliedVersion.length() > 0)
  {
    Serial.printf("[auto-update] Last applied auto-update: %s\n", lastAppliedVersion.c_str());
  }
}

void loopAutoUpdate()
{
  if (!AUTO_UPDATE_ENABLED)
  {
    return;
  }

  if (currentStatus != NONE)
  {
    return;
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  if (!isTimeToCheck())
  {
    return;
  }

  firstCheckDone = true;
  lastCheckMillis = millis();

  String version;
  String firmwareUrl;
  String sha256;

  if (!fetchManifest(version, firmwareUrl, sha256))
  {
    return;
  }

  if (version == lastAppliedVersion)
  {
    return;
  }

  Serial.printf("[auto-update] New version detected: %s\n", version.c_str());
  currentStatus = UPDATE;
  sendInfo();

  if (!applyFirmwareUpdate(firmwareUrl, sha256))
  {
    currentStatus = NONE;
    sendInfo();
    return;
  }

  autoUpdatePreferences.putString(AUTO_UPDATE_LAST_VERSION_KEY, version);
  lastAppliedVersion = version;

  Serial.println("[auto-update] Update successful. Rebooting...");
  delay(200);
  ESP.restart();
}

#else

void initAutoUpdate() {}
void loopAutoUpdate() {}

#endif
