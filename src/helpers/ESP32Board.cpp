#ifdef ESP_PLATFORM

#include "ESP32Board.h"

#if !defined(DISABLE_WIFI_OTA) && (defined(ADMIN_PASSWORD) || defined(WIFI_OTA_PERSISTENT_STA) || defined(WIFI_OTA_USE_WIFI_MANAGER))   // Repeater or Room Server only
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <Preferences.h>
#ifdef WIFI_OTA_USE_WIFI_MANAGER
#include <WiFiManager.h>
#endif

#include <SPIFFS.h>

static AsyncWebServer* wifi_ota_server = nullptr;
static bool wifi_ota_started = false;
static bool wifi_manager_started = false;
static bool wifi_sta_requested = false;
static bool wifi_ota_has_deadline = false;
static bool wifi_ota_stop_pending = false;
static bool wifi_legacy_ota_ap = false;
static uint32_t wifi_ota_until = 0;
static uint32_t wifi_ota_stop_at = 0;
static bool diag_radio_ok = false;
static int diag_radio_status = 0;
static char diag_boot_status[48] = "booting";
static ESP32Board::WifiOTACommandHandler wifi_ota_command_handler = nullptr;
static char wifi_ota_id_buf[60];
static char wifi_ota_home_buf[90];
static char wifi_manager_ap_name[64];
static char web_password[64];
static bool web_enabled = false;
static bool web_config_loaded = false;

#ifdef WIFI_OTA_USE_WIFI_MANAGER
static WiFiManager wifi_manager;
#endif

static bool load_wifi_sta_enabled() {
  Preferences prefs;
  bool enabled = false;
  if (prefs.begin("meshcore", true)) {
    enabled = prefs.getBool("wifi_sta", false);
    prefs.end();
  }
  return enabled;
}

static void load_web_config() {
  if (web_config_loaded) return;
  web_password[0] = 0;
  Preferences prefs;
  if (prefs.begin("meshcore", true)) {
    prefs.getString("web_pass", web_password, sizeof(web_password));
    web_enabled = prefs.getBool("web_on", false);
    prefs.end();
  }
  web_config_loaded = true;
}

static bool web_password_is_set() {
  load_web_config();
  return strlen(web_password) >= 8;
}

static bool require_web_auth(AsyncWebServerRequest *request) {
  if (!web_password_is_set()) {
    request->send(403, "text/plain", "web password not configured");
    return false;
  }
  if (!request->authenticate("admin", web_password)) {
    request->requestAuthentication();
    return false;
  }
  return true;
}

static void save_wifi_sta_enabled(bool enabled) {
  Preferences prefs;
  if (prefs.begin("meshcore", false)) {
    prefs.putBool("wifi_sta", enabled);
    prefs.end();
  }
}

static void start_wifi_ota_server() {
  if (wifi_ota_started) return;
  load_web_config();
  if (!wifi_legacy_ota_ap && (!web_enabled || !web_password_is_set())) return;

  wifi_ota_server = new AsyncWebServer(80);

  wifi_ota_server->on("/", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!wifi_legacy_ota_ap && !require_web_auth(request)) return;
    request->send(200, "text/html", wifi_ota_home_buf);
  });
  wifi_ota_server->on("/log", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!require_web_auth(request)) return;
    request->send(SPIFFS, "/packet_log", "text/plain");
  });
  wifi_ota_server->on("/diag", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!require_web_auth(request)) return;
    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"id\":\"%s\",\"boot\":\"%s\",\"radio_ok\":%s,\"radio_status\":%d,"
             "\"uptime_ms\":%lu,\"heap\":%lu,\"min_heap\":%lu,"
             "\"wifi_mode\":%d,\"wifi_status\":%d,\"sta_ip\":\"%s\",\"ap_ip\":\"%s\","
             "\"ota_started\":%s,\"ota_left_s\":%lu,\"reset_reason\":%d}",
             wifi_ota_id_buf, diag_boot_status, diag_radio_ok ? "true" : "false",
             diag_radio_status, (unsigned long)millis(), (unsigned long)ESP.getFreeHeap(),
             (unsigned long)ESP.getMinFreeHeap(), (int)WiFi.getMode(), (int)WiFi.status(),
             WiFi.localIP().toString().c_str(), WiFi.softAPIP().toString().c_str(),
             wifi_ota_started ? "true" : "false", (unsigned long)0, (int)esp_reset_reason());
    request->send(200, "application/json", buf);
  });
  wifi_ota_server->on("/radio", AsyncWebRequestMethod::HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!require_web_auth(request)) return;
    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"ok\":%s,\"status\":%d,\"pins\":{\"dio1\":%d,\"reset\":%d,\"busy\":%d,"
             "\"nss\":%d,\"sclk\":%d,\"miso\":%d,\"mosi\":%d},"
             "\"settings\":{\"freq\":%.3f,\"bw\":%.1f,\"sf\":%d,\"tx_power\":%d,"
             "\"current_limit\":%d,\"dio2_rf_switch\":%s,\"dio3_tcxo\":%.1f,"
             "\"sx126x_register_patch\":%s}}",
             diag_radio_ok ? "true" : "false", diag_radio_status,
             P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, P_LORA_NSS,
             P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI,
             (double)LORA_FREQ, (double)LORA_BW, LORA_SF, LORA_TX_POWER,
#ifdef SX126X_CURRENT_LIMIT
             SX126X_CURRENT_LIMIT,
#else
             0,
#endif
#ifdef SX126X_DIO2_AS_RF_SWITCH
             SX126X_DIO2_AS_RF_SWITCH ? "true" : "false",
#else
             "false",
#endif
#ifdef SX126X_DIO3_TCXO_VOLTAGE
             (double)SX126X_DIO3_TCXO_VOLTAGE,
#else
             0.0,
#endif
#ifdef SX126X_REGISTER_PATCH
             "true"
#else
             "false"
#endif
             );
    request->send(200, "application/json", buf);
  });
#ifdef WIFI_OTA_HTTP_CLI
  wifi_ota_server->on("/cmd", AsyncWebRequestMethod::HTTP_POST,
    [](AsyncWebServerRequest *request) {
      String* body = static_cast<String*>(request->_tempObject);
      request->_tempObject = nullptr;

      if (!require_web_auth(request)) {
        if (body) delete body;
        return;
      }

      if (!wifi_ota_command_handler) {
        if (body) delete body;
        request->send(503, "text/plain", "command handler unavailable");
        return;
      }

      if (!body && request->hasParam("cmd", true)) {
        body = new String(request->getParam("cmd", true)->value());
      }
      if (!body) {
        request->send(400, "text/plain", "missing command body");
        return;
      }

      body->trim();
      if (body->length() == 0) {
        delete body;
        request->send(400, "text/plain", "empty command");
        return;
      }
      if (body->length() >= 220) {
        delete body;
        request->send(413, "text/plain", "command too long");
        return;
      }

      char cmd[220];
      char reply[220];
      body->toCharArray(cmd, sizeof(cmd));
      delete body;
      reply[0] = 0;
      wifi_ota_command_handler(cmd, reply);
      request->send(200, "text/plain", reply[0] ? reply : "OK");
    },
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        if (request->_tempObject) delete static_cast<String*>(request->_tempObject);
        String* body = new String();
        body->reserve(total + 1);
        request->_tempObject = body;
      }

      String* body = static_cast<String*>(request->_tempObject);
      if (body && body->length() + len < 512) {
        body->concat(reinterpret_cast<const char*>(data), len);
      }
    });
#endif

  AsyncElegantOTA.setID(wifi_ota_id_buf);
  if (web_password_is_set()) {
    AsyncElegantOTA.begin(wifi_ota_server, "admin", web_password);
  } else {
    AsyncElegantOTA.begin(wifi_ota_server);
  }
  wifi_ota_server->begin();
  wifi_ota_started = true;

  MESH_DEBUG_PRINTLN("WiFi OTA: http://%s/update", WiFi.localIP().toString().c_str());
}

static void stop_wifi_ota_server() {
  if (!wifi_ota_server) {
    wifi_ota_started = false;
    return;
  }

  wifi_ota_server->end();
  delete wifi_ota_server;
  wifi_ota_server = nullptr;
  wifi_ota_started = false;
}

static bool wifi_ota_deadline_expired() {
  return wifi_ota_has_deadline && (int32_t)(millis() - wifi_ota_until) >= 0;
}

static void set_wifi_ota_deadline(uint32_t seconds) {
  if (seconds == 0) {
    wifi_ota_has_deadline = false;
    wifi_ota_until = 0;
  } else {
    wifi_ota_has_deadline = true;
    wifi_ota_until = millis() + (seconds * 1000UL);
  }
}

static void suspend_wifi_radio_only() {
#ifdef WIFI_OTA_USE_WIFI_MANAGER
  wifi_manager_started = false;
#endif

  wifi_sta_requested = false;
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
}

void ESP32Board::beginWifiOTA(const char* id) {
  wifi_ota_stop_pending = false;
  wifi_legacy_ota_ap = false;
  wifi_sta_requested = true;
  inhibit_sleep = true;   // keep WiFi and OTA responsive; MeshCore loop still runs normally

  snprintf(wifi_ota_id_buf, sizeof(wifi_ota_id_buf), "%s (%s)", id, getManufacturerName());
  snprintf(wifi_ota_home_buf, sizeof(wifi_ota_home_buf), "<H2>Hi! I am a MeshCore Repeater. ID: %s</H2>", id);

  set_wifi_ota_deadline(0);

#ifdef WIFI_OTA_USE_WIFI_MANAGER
  snprintf(wifi_manager_ap_name, sizeof(wifi_manager_ap_name), "MeshCore-WiFi-%s", id);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.setHostname(id);

  wifi_manager.setConfigPortalBlocking(false);
#ifdef WIFI_OTA_CONNECT_TIMEOUT
  wifi_manager.setConnectTimeout(WIFI_OTA_CONNECT_TIMEOUT);
#else
  wifi_manager.setConnectTimeout(10);
#endif

  if (web_password_is_set()) {
    wifi_manager.autoConnect(wifi_manager_ap_name, web_password);
  } else {
    wifi_manager.autoConnect(wifi_manager_ap_name);
  }
  wifi_manager_started = true;
  if (WiFi.status() == WL_CONNECTED && web_enabled && web_password_is_set()) {
    start_wifi_ota_server();
  }
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MeshCore-OTA", NULL);
  start_wifi_ota_server();
#endif
}

void ESP32Board::loopWifiOTA() {
  if (wifi_ota_stop_pending && (int32_t)(millis() - wifi_ota_stop_at) >= 0) {
    wifi_ota_stop_pending = false;
    suspend_wifi_radio_only();
    inhibit_sleep = false;
    return;
  }

  if (wifi_ota_deadline_expired()) {
    bool resume_sta = load_wifi_sta_enabled();
    char saved_id[sizeof(wifi_ota_id_buf)];
    snprintf(saved_id, sizeof(saved_id), "%s", wifi_ota_id_buf);
    char *board_suffix = strstr(saved_id, " (");
    if (board_suffix) *board_suffix = 0;
    stopWifiOTA();
    if (resume_sta) beginWifiOTA(saved_id);
    return;
  }

#ifdef WIFI_OTA_USE_WIFI_MANAGER
  if (wifi_manager_started && WiFi.status() != WL_CONNECTED) {
    wifi_manager.process();
  }
#endif

  if (WiFi.status() == WL_CONNECTED && web_enabled && web_password_is_set()) {
    start_wifi_ota_server();
  }
}

bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  wifi_ota_stop_pending = false;
  stopWifiOTA();
  wifi_legacy_ota_ap = true;
  inhibit_sleep = true;
  snprintf(wifi_ota_id_buf, sizeof(wifi_ota_id_buf), "%s (%s)", id, getManufacturerName());
  snprintf(wifi_ota_home_buf, sizeof(wifi_ota_home_buf), "<H2>Hi! I am a MeshCore Repeater. ID: %s</H2>", id);
#ifdef WIFI_OTA_AP_WINDOW_SECS
  set_wifi_ota_deadline(WIFI_OTA_AP_WINDOW_SECS);
#else
  set_wifi_ota_deadline(600);
#endif
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MeshCore-OTA", NULL);
  start_wifi_ota_server();
  sprintf(reply, "Started: http://%s/update", WiFi.softAPIP().toString().c_str());
  return true;
}

void ESP32Board::stopWifiOTAAP() {
  wifi_ota_has_deadline = false;
  wifi_ota_until = 0;
}

void ESP32Board::stopWifiOTA() {
  wifi_ota_stop_pending = false;

#ifdef WIFI_OTA_USE_WIFI_MANAGER
  if (wifi_manager_started) {
    wifi_manager.stopConfigPortal();
    wifi_manager_started = false;
  }
#endif

  if (wifi_ota_server) {
    stop_wifi_ota_server();
  }

  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
  wifi_sta_requested = false;
  wifi_legacy_ota_ap = false;
  wifi_ota_has_deadline = false;
  wifi_ota_until = 0;
  inhibit_sleep = false;
}

void ESP32Board::requestWifiOTAAPStop(uint32_t delay_ms) {
  wifi_ota_has_deadline = false;
  wifi_ota_until = 0;
}

void ESP32Board::requestWifiOTAStop(uint32_t delay_ms) {
  wifi_ota_stop_pending = true;
  wifi_ota_stop_at = millis() + delay_ms;
}

bool ESP32Board::isWifiOTAActive() const {
  return wifi_ota_started || wifi_manager_started || WiFi.status() == WL_CONNECTED;
}

uint32_t ESP32Board::getWifiOTASecondsRemaining() const {
  if (!wifi_ota_has_deadline) return 0;
  int32_t remaining = (int32_t)(wifi_ota_until - millis());
  if (remaining <= 0) return 0;
  return ((uint32_t)remaining + 999) / 1000;
}

bool ESP32Board::isWifiSTAEnabled() const {
  return load_wifi_sta_enabled();
}

void ESP32Board::setWifiSTAEnabled(bool enabled) {
  save_wifi_sta_enabled(enabled);
}

bool ESP32Board::isWebEnabled() const {
  load_web_config();
  return web_enabled;
}

bool ESP32Board::hasWebPassword() const {
  return web_password_is_set();
}

bool ESP32Board::setWebPassword(const char* password) {
  size_t len = password ? strlen(password) : 0;
  if (len < 8 || len >= sizeof(web_password)) return false;
  Preferences prefs;
  if (!prefs.begin("meshcore", false)) return false;
  bool ok = prefs.putString("web_pass", password) == len;
  prefs.end();
  if (!ok) return false;
  snprintf(web_password, sizeof(web_password), "%s", password);
  web_config_loaded = true;
  return true;
}

void ESP32Board::setWebEnabled(bool enabled) {
  load_web_config();
  web_enabled = enabled;
  Preferences prefs;
  if (prefs.begin("meshcore", false)) {
    prefs.putBool("web_on", enabled);
    prefs.end();
  }
  if (!enabled && wifi_ota_server && !wifi_legacy_ota_ap) {
    stop_wifi_ota_server();
  } else if (enabled && WiFi.status() == WL_CONNECTED && web_password_is_set()) {
    stop_wifi_ota_server();
    start_wifi_ota_server();
  }
}

void ESP32Board::formatWebStatus(char* reply, size_t reply_len) const {
  load_web_config();
  snprintf(reply, reply_len, "web=%s password=%s user=admin",
           web_enabled ? "on" : "off", web_password_is_set() ? "set" : "missing");
}

void ESP32Board::setBootStatus(const char* status) {
  if (!status) return;
  snprintf(diag_boot_status, sizeof(diag_boot_status), "%s", status);
}

void ESP32Board::setRadioStatus(bool ok, int status) {
  diag_radio_ok = ok;
  diag_radio_status = status;
}

void ESP32Board::setWifiOTACommandHandler(WifiOTACommandHandler handler) {
  wifi_ota_command_handler = handler;
}

void ESP32Board::formatWifiOTAStatus(char* reply, size_t reply_len) const {
  const char* mode = WiFi.getMode() == WIFI_OFF ? "off" : (WiFi.getMode() == WIFI_AP ? "ap" : (WiFi.getMode() == WIFI_STA ? "sta" : "apsta"));
  uint32_t remaining = getWifiOTASecondsRemaining();
  bool sta_enabled = load_wifi_sta_enabled();

  if (WiFi.getMode() == WIFI_OFF) {
    snprintf(reply, reply_len, "sta=%s ota=off web=%s mode=off left=%lus",
             sta_enabled ? "on" : "off", isWebEnabled() ? "on" : "off", (unsigned long)remaining);
  } else if (WiFi.status() == WL_CONNECTED) {
    snprintf(reply, reply_len, "sta=%s ota=%s web=%s mode=%s left=%lus ip=%s",
             sta_enabled ? "on" : "off", wifi_ota_started ? "lan" : "off",
             isWebEnabled() ? "on" : "off", mode, (unsigned long)remaining,
             WiFi.localIP().toString().c_str());
  } else if (wifi_legacy_ota_ap) {
    snprintf(reply, reply_len, "sta=%s ota=ap web=%s mode=ap left=%lus ip=%s",
             sta_enabled ? "on" : "off", isWebEnabled() ? "on" : "off",
             (unsigned long)remaining, WiFi.softAPIP().toString().c_str());
  } else {
    snprintf(reply, reply_len, "sta=%s ota=waiting web=%s mode=%s left=%lus",
             sta_enabled ? "on" : "off", isWebEnabled() ? "on" : "off",
             mode, (unsigned long)remaining);
  }
}

const char* ESP32Board::getResetReasonString(uint32_t reason) {
  switch ((esp_reset_reason_t)reason) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    default: return "unknown";
  }
}

#else
void ESP32Board::beginWifiOTA(const char* id) {
}

void ESP32Board::loopWifiOTA() {
}
void ESP32Board::stopWifiOTAAP() {
}
void ESP32Board::requestWifiOTAAPStop(uint32_t delay_ms) {
}

bool ESP32Board::startOTAUpdate(const char* id, char reply[]) {
  return false; // not supported
}

void ESP32Board::stopWifiOTA() {
}

bool ESP32Board::isWifiOTAActive() const {
  return false;
}
bool ESP32Board::isWifiSTAEnabled() const {
  return false;
}
void ESP32Board::setWifiSTAEnabled(bool enabled) {
}
bool ESP32Board::isWebEnabled() const {
  return false;
}
bool ESP32Board::hasWebPassword() const {
  return false;
}
bool ESP32Board::setWebPassword(const char* password) {
  return false;
}
void ESP32Board::setWebEnabled(bool enabled) {
}
void ESP32Board::formatWebStatus(char* reply, size_t reply_len) const {
  snprintf(reply, reply_len, "web=unsupported");
}

uint32_t ESP32Board::getWifiOTASecondsRemaining() const {
  return 0;
}

void ESP32Board::formatWifiOTAStatus(char* reply, size_t reply_len) const {
  snprintf(reply, reply_len, "ota=unsupported");
}

void ESP32Board::setBootStatus(const char* status) {
}

void ESP32Board::setRadioStatus(bool ok, int status) {
}

void ESP32Board::setWifiOTACommandHandler(WifiOTACommandHandler handler) {
}

const char* ESP32Board::getResetReasonString(uint32_t reason) {
  return "Not available";
}
#endif

#endif
