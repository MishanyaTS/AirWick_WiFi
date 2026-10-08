void migrateNetworkConfig() {
  bool changed = false;

  if (!jsonHasKey(configSetup, "ESP_mode")) {
    jsonWrite(configSetup, "ESP_mode", jsonRead(configSetup, "ssid").length() ? 1 : 0);
    changed = true;
  }
  if (!jsonHasKey(configSetup, "wifi_multi")) {
    jsonWrite(configSetup, "wifi_multi", 1);
    changed = true;
  }
  if (!jsonHasKey(configSetup, "TimeOut")) {
    jsonWrite(configSetup, "TimeOut", 60);
    changed = true;
  }
  int configuredPowerMode;
  if (!jsonHasKey(configSetup, "power_mode")) {
    bool legacyEnabled =
        (jsonHasKey(configSetup, "lowPWR") &&
         jsonReadtoInt(configSetup, "lowPWR") != 0) ||
        (jsonHasKey(configSetup, "battery_mode") &&
         jsonReadtoInt(configSetup, "battery_mode") != 0);
    configuredPowerMode = legacyEnabled ? POWER_SAVE_LIGHT : POWER_SAVE_OFF;
    jsonWrite(configSetup, "power_mode", configuredPowerMode);
    changed = true;
  } else {
    int storedPowerMode = jsonReadtoInt(configSetup, "power_mode");
    configuredPowerMode = storedPowerMode == POWER_SAVE_OFF ?
                          POWER_SAVE_OFF : POWER_SAVE_LIGHT;
    if (storedPowerMode != configuredPowerMode) {
      jsonWrite(configSetup, "power_mode", configuredPowerMode);
      changed = true;
    }
  }

  int legacyPowerFlag = configuredPowerMode == POWER_SAVE_LIGHT ? 1 : 0;
  if (!jsonHasKey(configSetup, "battery_mode") ||
      jsonReadtoInt(configSetup, "battery_mode") != legacyPowerFlag) {
    jsonWrite(configSetup, "battery_mode", legacyPowerFlag);
    changed = true;
  }
  if (!jsonHasKey(configSetup, "lowPWR") ||
      jsonReadtoInt(configSetup, "lowPWR") != legacyPowerFlag) {
    jsonWrite(configSetup, "lowPWR", legacyPowerFlag);
    changed = true;
  }

  int powerSavingSchema = jsonReadtoInt(configSetup, "power_saving_schema");
  if (!jsonHasKey(configSetup, "power_saving_schema") ||
      powerSavingSchema != POWER_SAVING_SCHEMA_VERSION) {
    int configuredSleepSeconds = jsonHasKey(configSetup, "light_sleep_seconds") ?
        jsonReadtoInt(configSetup, "light_sleep_seconds") :
        jsonReadtoInt(configSetup, "deep_sleep_seconds");
    if (configuredSleepSeconds < LIGHT_SLEEP_MIN_SECONDS ||
        configuredSleepSeconds > LIGHT_SLEEP_MAX_SECONDS) {
      configuredSleepSeconds = 20;
    }

    int configuredAwakeSeconds = jsonHasKey(configSetup, "light_awake_seconds") ?
        jsonReadtoInt(configSetup, "light_awake_seconds") :
        jsonReadtoInt(configSetup, "deep_awake_seconds");
    if (configuredAwakeSeconds < LIGHT_AWAKE_MIN_SECONDS ||
        configuredAwakeSeconds > LIGHT_AWAKE_MAX_SECONDS) {
      configuredAwakeSeconds = 5;
    }

    jsonWrite(configSetup, "power_saving_schema", POWER_SAVING_SCHEMA_VERSION);
    jsonWrite(configSetup, "light_sleep_seconds", configuredSleepSeconds);
    jsonWrite(configSetup, "light_awake_seconds", configuredAwakeSeconds);
    changed = true;
  } else {
    int configuredSleepSeconds = jsonReadtoInt(configSetup, "light_sleep_seconds");
    if (!jsonHasKey(configSetup, "light_sleep_seconds") ||
        configuredSleepSeconds < LIGHT_SLEEP_MIN_SECONDS ||
        configuredSleepSeconds > LIGHT_SLEEP_MAX_SECONDS) {
      jsonWrite(configSetup, "light_sleep_seconds", 20);
      changed = true;
    }

    int configuredAwakeSeconds = jsonReadtoInt(configSetup, "light_awake_seconds");
    if (!jsonHasKey(configSetup, "light_awake_seconds") ||
        configuredAwakeSeconds < LIGHT_AWAKE_MIN_SECONDS ||
        configuredAwakeSeconds > LIGHT_AWAKE_MAX_SECONDS) {
      jsonWrite(configSetup, "light_awake_seconds", 5);
      changed = true;
    }
  }

  const char* optionalKeys[] = {"ssid2", "password2", "ssid3", "password3"};
  for (uint8_t i = 0; i < 4; i++) {
    if (!jsonHasKey(configSetup, optionalKeys[i])) {
      jsonWrite(configSetup, optionalKeys[i], "");
      changed = true;
    }
  }

  if (!jsonHasKey(configSetup, "s_IP")) {
    String oldIpConfig = readFile("config_ip.json", 512);
    int legacyStaticIp = 0;
    if (oldIpConfig != "Failed" && oldIpConfig != "Large") {
      legacyStaticIp = jsonReadtoInt(oldIpConfig, "ip_on");
    }
    jsonWrite(configSetup, "s_IP", legacyStaticIp ? 1 : 0);
    changed = true;
  }

  if (changed) {
    saveConfig();
    LOG.println(F("Настройки сети обновлены до текущего формата"));
  }
}

bool isValidWifiSsid(const String& ssid) {
  return ssid.length() <= 32;
}

bool isValidWifiPassword(const String& password) {
  return password.length() == 0 || (password.length() >= 8 && password.length() <= 63);
}

bool parsePowerNumber(const String& value, int minimumValue,
                      int maximumValue, int& result) {
  if (!value.length()) return false;
  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }
  result = value.toInt();
  return result >= minimumValue && result <= maximumValue;
}

const __FlashStringHelper* powerSavingModeText(uint8_t mode) {
  return mode == POWER_SAVE_LIGHT ? F("включено") :
                                    F("отключено");
}

bool keepWiFiRadioAwake() {
  return WiFi.setSleepMode(WIFI_NONE_SLEEP);
}

bool applyWiFiPowerMode() {
  return keepWiFiRadioAwake();
}

bool setPowerSavingMode(uint8_t requestedMode, bool persist) {
  if (requestedMode > POWER_SAVE_LIGHT) return false;
  powerSavingMode = requestedMode;
  clearPowerSavingRuntimeState();

  if (persist) {
    jsonWrite(configSetup, "power_mode", powerSavingMode);
    jsonWrite(configSetup, "battery_mode",
              powerSavingMode == POWER_SAVE_LIGHT ? 1 : 0);
    jsonWrite(configSetup, "lowPWR",
              powerSavingMode == POWER_SAVE_LIGHT ? 1 : 0);
    saveConfig();
  }

  bool applied = applyWiFiPowerMode();
  LOG.print(F("Энергосбережение сохранено: "));
  LOG.println(powerSavingModeText(powerSavingMode));
  return applied;
}

void savePowerSavingModeFromWeb(uint8_t requestedMode) {
  bool applied = setPowerSavingMode(requestedMode, true);
  if (compatiblePowerSavingActive()) notePowerSavingWebActivity();

  DynamicJsonDocument doc(384);
  doc["ok"] = applied;
  doc["power_mode"] = powerSavingMode;
  doc["wifi_sleep"] = "none";
  doc["light_sleep_enabled"] = powerSavingMode == POWER_SAVE_LIGHT;
  doc["light_sleep_scheduled"] = compatiblePowerSleepAllowed();
  doc["light_sleep_in"] = compatiblePowerSecondsUntilSleep();
  doc["restart_required"] = false;
  String response;
  serializeJson(doc, response);
  HTTP.send(200, "application/json; charset=utf-8", response);
}

void handleCompatibleSleepSettings() {
  int sleepSeconds;
  int awakeSeconds;
  bool valid = HTTP.hasArg("sleep_seconds") &&
               HTTP.hasArg("awake_seconds") &&
               parsePowerNumber(HTTP.arg("sleep_seconds"),
                                LIGHT_SLEEP_MIN_SECONDS,
                                LIGHT_SLEEP_MAX_SECONDS, sleepSeconds) &&
               parsePowerNumber(HTTP.arg("awake_seconds"),
                                LIGHT_AWAKE_MIN_SECONDS,
                                LIGHT_AWAKE_MAX_SECONDS, awakeSeconds);
  if (!valid) {
    HTTP.send(400, "application/json",
              "{\"ok\":false,\"error\":\"invalid light sleep settings\"}");
    return;
  }

  lightSleepSeconds = (uint16_t)sleepSeconds;
  lightAwakeSeconds = (uint16_t)awakeSeconds;
  jsonWrite(configSetup, "light_sleep_seconds", lightSleepSeconds);
  jsonWrite(configSetup, "light_awake_seconds", lightAwakeSeconds);
  saveConfig();
  notePowerSavingWebActivity();
  LOG.print(F("Параметры совместимого режима сохранены: сон "));
  LOG.print(lightSleepSeconds);
  LOG.print(F(" с, окно сети "));
  LOG.print(lightAwakeSeconds);
  LOG.println(F(" с"));
  HTTP.send(200, "application/json",
            "{\"ok\":true,\"should_refresh\":true}");
}

void registerWiFiHandlers() {
  HTTP.on("/ESP_mode", HTTP_GET, []() {
    espMode = HTTP.arg("ESP_mode").toInt() ? 1 : 0;
    jsonWrite(configSetup, "ESP_mode", espMode);
    saveConfig();
    LOG.print(F("Режим Wi-Fi сохранён: "));
    LOG.println(espMode == 0 ? F("точка доступа") : F("подключение к роутеру"));
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  HTTP.on("/wifi_multi", HTTP_GET, []() {
    bool enabled = HTTP.arg("wifi_multi").toInt() != 0;
    jsonWrite(configSetup, "wifi_multi", enabled ? 1 : 0);
    saveConfig();
    LOG.print(F("Дополнительные сети Wi-Fi: "));
    LOG.println(enabled ? F("включены") : F("выключены"));
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  HTTP.on("/power_mode", HTTP_GET, []() {
    int requestedMode;
    if (!HTTP.hasArg("power_mode") ||
        !parsePowerNumber(HTTP.arg("power_mode"), POWER_SAVE_OFF,
                          POWER_SAVE_LIGHT, requestedMode)) {
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"power_mode must be 0 or 1\"}");
      return;
    }
    savePowerSavingModeFromWeb((uint8_t)requestedMode);
  });

  HTTP.on("/battery_mode", HTTP_GET, []() {
    int requestedMode;
    if (!HTTP.hasArg("battery_mode") ||
        !parsePowerNumber(HTTP.arg("battery_mode"), 0, 1, requestedMode)) {
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"battery_mode must be 0 or 1\"}");
      return;
    }
    savePowerSavingModeFromWeb(requestedMode ? POWER_SAVE_LIGHT : POWER_SAVE_OFF);
  });

  HTTP.on("/lowpwr", HTTP_GET, []() {
    int requestedMode;
    String value = HTTP.hasArg("onoff") ? HTTP.arg("onoff") :
                   HTTP.hasArg("val") ? HTTP.arg("val") : "";
    if (!parsePowerNumber(value, 0, 1, requestedMode)) {
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"onoff/val must be 0 or 1\"}");
      return;
    }
    savePowerSavingModeFromWeb(requestedMode ? POWER_SAVE_LIGHT : POWER_SAVE_OFF);
  });

  HTTP.on("/light_sleep_settings", HTTP_GET, handleCompatibleSleepSettings);
  HTTP.on("/deep_sleep_settings", HTTP_GET, handleCompatibleSleepSettings);
  HTTP.on("/power_awake", HTTP_GET, []() {
    if (!compatiblePowerSavingActive()) {
      HTTP.send(200, "application/json",
                "{\"ok\":true,\"active\":false}");
      return;
    }
    holdCompatiblePowerForMaintenance();
    LOG.println(F("Light Sleep отложен на 10 минут через WEB"));
    HTTP.send(200, "application/json",
              "{\"ok\":true,\"active\":true,\"seconds\":600}");
  });

  HTTP.on("/power_status", HTTP_GET, []() {
    notePowerSavingWebActivity();
    DynamicJsonDocument doc(512);
    doc["power_mode"] = powerSavingMode;
    doc["power_mode_text"] = powerSavingMode == POWER_SAVE_LIGHT ?
                             "compatible" : "off";
    doc["sleep_seconds"] = lightSleepSeconds;
    doc["awake_seconds"] = lightAwakeSeconds;
    doc["dark"] = lightLevel <= lightTreshold;
    doc["timers_idle"] = compatiblePowerTimersIdle();
    doc["sleep_allowed"] = compatiblePowerSleepAllowed();
    doc["sleep_in"] = compatiblePowerSecondsUntilSleep();
    doc["maintenance_hold"] = lightSleepHoldActive;
    String response;
    serializeJson(doc, response);
    HTTP.send(200, "application/json; charset=utf-8", response);
  });

  HTTP.on("/ssid", HTTP_GET, []() {
    String ssid = HTTP.hasArg("ssid") ? HTTP.arg("ssid") : jsonRead(configSetup, "ssid");
    String password = HTTP.hasArg("password") ? HTTP.arg("password") : jsonRead(configSetup, "password");
    String ssid2 = HTTP.hasArg("ssid2") ? HTTP.arg("ssid2") : jsonRead(configSetup, "ssid2");
    String password2 = HTTP.hasArg("password2") ? HTTP.arg("password2") : jsonRead(configSetup, "password2");
    String ssid3 = HTTP.hasArg("ssid3") ? HTTP.arg("ssid3") : jsonRead(configSetup, "ssid3");
    String password3 = HTTP.hasArg("password3") ? HTTP.arg("password3") : jsonRead(configSetup, "password3");
    int timeoutSeconds = HTTP.hasArg("TimeOut") ?
                         HTTP.arg("TimeOut").toInt() :
                         jsonReadtoInt(configSetup, "TimeOut");

    bool validNetworks = isValidWifiSsid(ssid) && isValidWifiSsid(ssid2) &&
                         isValidWifiSsid(ssid3) &&
                         isValidWifiPassword(password) &&
                         isValidWifiPassword(password2) &&
                         isValidWifiPassword(password3) &&
                         (!ssid2.length() ? !password2.length() : true) &&
                         (!ssid3.length() ? !password3.length() : true);
    if (!validNetworks || timeoutSeconds < 1 || timeoutSeconds > 300) {
      LOG.println(F("Настройки Wi-Fi отклонены: неверный SSID, пароль или тайм-аут"));
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"invalid Wi-Fi settings\"}");
      return;
    }

    jsonWrite(configSetup, "ssid", ssid);
    jsonWrite(configSetup, "password", password);
    jsonWrite(configSetup, "ssid2", ssid2);
    jsonWrite(configSetup, "password2", password2);
    jsonWrite(configSetup, "ssid3", ssid3);
    jsonWrite(configSetup, "password3", password3);
    ESP_CONN_TIMEOUT = (uint16_t)timeoutSeconds;
    jsonWrite(configSetup, "TimeOut", ESP_CONN_TIMEOUT);
    saveConfig();
    LOG.print(F("Настройки Wi-Fi сохранены, основная сеть: "));
    LOG.print(ssid);
    LOG.print(F(", сетей: "));
    LOG.print((ssid.length() ? 1 : 0) + (ssid2.length() ? 1 : 0) + (ssid3.length() ? 1 : 0));
    LOG.print(F(", тайм-аут: "));
    LOG.print(ESP_CONN_TIMEOUT);
    LOG.println(F(" с"));
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  HTTP.on("/ssidap", HTTP_GET, []() {
    String ssidAP = HTTP.hasArg("ssidAP") ? HTTP.arg("ssidAP") : jsonRead(configSetup, "ssidAP");
    String passwordAP = HTTP.hasArg("passwordAP") ? HTTP.arg("passwordAP") : jsonRead(configSetup, "passwordAP");
    if (!ssidAP.length() || ssidAP.length() > 32 ||
        passwordAP.length() < 8 || passwordAP.length() > 63) {
      LOG.println(F("Настройки точки доступа отклонены"));
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"invalid access point settings\"}");
      return;
    }
    jsonWrite(configSetup, "ssidAP", ssidAP);
    jsonWrite(configSetup, "passwordAP", passwordAP);
    saveConfig();
    LOG.print(F("Настройки точки доступа сохранены, SSID: "));
    LOG.println(ssidAP);
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });
}

bool startAccessPoint(bool keepStation) {
  if (!keepStation) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
  } else {
    WiFi.mode(WIFI_AP_STA);
  }
  keepWiFiRadioAwake();

  IPAddress apIp(AP_STATIC_IP[0], AP_STATIC_IP[1], AP_STATIC_IP[2], AP_STATIC_IP[3]);
  IPAddress apGateway(AP_STATIC_IP[0], AP_STATIC_IP[1], AP_STATIC_IP[2], 1);
  WiFi.softAPConfig(apIp, apGateway, IPAddress(255, 255, 255, 0));

  String ssidAP = jsonRead(configSetup, "ssidAP");
  String passwordAP = jsonRead(configSetup, "passwordAP");
  if (!ssidAP.length()) ssidAP = "AirWick";

  bool started;
  if (passwordAP.length() >= 8) {
    started = WiFi.softAP(ssidAP.c_str(), passwordAP.c_str());
  } else {
    started = WiFi.softAP(ssidAP.c_str());
  }

  delay(100);
  LOG.print(keepStation ? F("Временная точка доступа запущена: ") : F("Точка доступа запущена: "));
  LOG.println(WiFi.softAPIP());
  if (!started) LOG.println(F("Ошибка запуска точки доступа AirWick"));
  return started;
}

bool StartAPMode() {
  return startAccessPoint(false);
}

bool applyStaticIpConfig() {
  if (!use_static_ip) {
    LOG.println(F("Статический IP выключен. Включается DHCP."));

    if (!WiFi.config(0U, 0U, 0U)) {
      LOG.println(F("Не удалось включить DHCP."));
      return false;
    }

    return true;
  }

  if (!staticIpConfigValid) {
    LOG.println(F("Неверные настройки статического IP. Включается DHCP."));
    WiFi.config(0U, 0U, 0U);
    return false;
  }

  if (!WiFi.config(Static_IP, Gateway, Subnet, DNS1, DNS2)) {
    LOG.println(F("Не удалось применить статический IP. Включается DHCP."));
    WiFi.config(0U, 0U, 0U);
    return false;
  }

  LOG.print(F("Используется статический IP: "));
  LOG.println(Static_IP);
  return true;
}

uint8_t addConfiguredNetworks() {
  uint8_t count = 0;
  String ssid = jsonRead(configSetup, "ssid");
  String password = jsonRead(configSetup, "password");
  if (ssid.length()) {
    wifiMulti.addAP(ssid.c_str(), password.c_str());
    count++;
  }

  if (jsonReadtoInt(configSetup, "wifi_multi")) {
    String ssid2 = jsonRead(configSetup, "ssid2");
    String password2 = jsonRead(configSetup, "password2");
    String ssid3 = jsonRead(configSetup, "ssid3");
    String password3 = jsonRead(configSetup, "password3");
    if (ssid2.length()) {
      wifiMulti.addAP(ssid2.c_str(), password2.c_str());
      count++;
      LOG.print(F("Добавлена сеть 2: "));
      LOG.println(ssid2);
    }
    if (ssid3.length()) {
      wifiMulti.addAP(ssid3.c_str(), password3.c_str());
      count++;
      LOG.print(F("Добавлена сеть 3: "));
      LOG.println(ssid3);
    }
  }
  return count;
}

bool stationIpIsValid(const IPAddress& address) {
  return address[0] || address[1] || address[2] || address[3];
}

const uint32_t WIFI_HEALTH_CHECK_INTERVAL = 60000UL;
const uint32_t WIFI_HEALTH_PASSIVE_INTERVAL = 300000UL;
const uint32_t WIFI_HEALTH_PING_GUARD_MS = 4000UL;
const uint32_t WIFI_WEB_SELF_TEST_INTERVAL = 45000UL;
const uint32_t WIFI_WEB_SELF_TEST_TIMEOUT = 8000UL;
const uint32_t WIFI_STACK_RECOVERY_COOLDOWN = 120000UL;
const uint32_t WIFI_AP_HEALTH_CHECK_INTERVAL = 30000UL;
const uint8_t WIFI_HEALTH_FAILURE_LIMIT = 3;
const uint8_t WIFI_HEALTH_INITIAL_PROBE_LIMIT = 3;

static struct ping_option wifiGatewayPingOption;
volatile bool wifiGatewayPingInProgress = false;
volatile bool wifiGatewayPingFinished = false;
volatile bool wifiGatewayPingReceived = false;
volatile bool wifiGatewayPingIgnoreResult = false;
uint32_t wifiGatewayPingStartedMs = 0;
uint32_t lastWifiGatewayPingMs = 0;
uint32_t lastWiFiStackRecoveryMs = 0;
uint32_t lastAccessPointHealthCheckMs = 0;
uint8_t wifiGatewayPingFailureCount = 0;
uint8_t wifiGatewayInitialProbeFailures = 0;
bool wifiGatewayPingSupported = false;
bool wifiRecoveryValidationPending = false;
IPAddress wifiGatewayPingTarget;
IPAddress wifiKnownGateway;

WiFiClient wifiWebHealthClient;
bool wifiWebHealthTestInProgress = false;
bool wifiWebHealthTestSupported = false;
uint32_t wifiWebHealthTestStartedMs = 0;
uint32_t lastWifiWebHealthTestMs = 0;
uint8_t wifiWebHealthFailureCount = 0;
uint8_t wifiWebHealthInitialProbeFailures = 0;
uint8_t wifiWebHealthResponseLength = 0;
char wifiWebHealthResponse[48];
IPAddress wifiWebHealthTarget;

static void wifiGatewayPingReceiveCallback(void*, void*) {
  if (!wifiGatewayPingIgnoreResult) wifiGatewayPingReceived = true;
}

static void wifiGatewayPingFinishedCallback(void*, void* responseData) {
  if (wifiGatewayPingIgnoreResult) {
    wifiGatewayPingIgnoreResult = false;
    wifiGatewayPingInProgress = false;
    wifiGatewayPingFinished = false;
    wifiGatewayPingReceived = false;
    return;
  }
  struct ping_resp* response = (struct ping_resp*)responseData;
  if (response != nullptr && response->total_bytes > 0) {
    wifiGatewayPingReceived = true;
  }
  wifiGatewayPingInProgress = false;
  wifiGatewayPingFinished = true;
}

bool stationHasValidConnection() {
  return WiFi.status() == WL_CONNECTED && stationIpIsValid(WiFi.localIP());
}

bool sameIpAddress(const IPAddress& first, const IPAddress& second) {
  for (uint8_t i = 0; i < 4; i++) {
    if (first[i] != second[i]) return false;
  }
  return true;
}

void resetGatewayHealthForNewNetwork(const IPAddress& gateway) {
  wifiKnownGateway = gateway;
  wifiGatewayPingFailureCount = 0;
  wifiGatewayInitialProbeFailures = 0;
  wifiGatewayPingSupported = false;
  lastWifiGatewayPingMs = millis();
}

bool startGatewayHealthPing() {
  IPAddress gateway = WiFi.gatewayIP();
  if (!stationIpIsValid(gateway) || wifiGatewayPingInProgress) return false;

  if (stationIpIsValid(wifiKnownGateway) &&
      !sameIpAddress(gateway, wifiKnownGateway)) {
    LOG.print(F("Изменился шлюз Wi-Fi: "));
    LOG.println(gateway);
    resetGatewayHealthForNewNetwork(gateway);
  } else if (!stationIpIsValid(wifiKnownGateway)) {
    wifiKnownGateway = gateway;
  }

  memset(&wifiGatewayPingOption, 0, sizeof(wifiGatewayPingOption));
  wifiGatewayPingOption.ip = gateway;
  wifiGatewayPingOption.count = 1;
  wifiGatewayPingOption.coarse_time = 1;
  wifiGatewayPingOption.recv_function = wifiGatewayPingReceiveCallback;
  wifiGatewayPingOption.sent_function = wifiGatewayPingFinishedCallback;

  wifiGatewayPingTarget = gateway;
  wifiGatewayPingReceived = false;
  wifiGatewayPingFinished = false;
  wifiGatewayPingInProgress = true;
  wifiGatewayPingStartedMs = millis();
  lastWifiGatewayPingMs = wifiGatewayPingStartedMs;

  if (!ping_start(&wifiGatewayPingOption)) {
    wifiGatewayPingInProgress = false;
    wifiGatewayPingFinished = false;
    return false;
  }
  return true;
}

const __FlashStringHelper* wifiDisconnectReasonText(uint8_t reason) {
  switch (reason) {
    case 1:   return F("неизвестная причина");
    case 2:   return F("истекла авторизация");
    case 3:   return F("роутер завершил авторизацию");
    case 4:   return F("истекло подключение к точке доступа");
    case 8:   return F("точка доступа завершила соединение");
    case 15:  return F("тайм-аут четырёхстороннего рукопожатия");
    case 200: return F("потеряны маяки роутера");
    case 201: return F("сеть не найдена");
    case 202: return F("ошибка авторизации");
    case 203: return F("ошибка подключения к точке доступа");
    case 204: return F("тайм-аут рукопожатия");
    default:  return F("другая причина");
  }
}

void registerWiFiEventHandlers() {
  wifiGotIpEventHandler = WiFi.onStationModeGotIP(
      [](const WiFiEventStationModeGotIP&) {
        wifiGotIpEventPending = true;
      });

  wifiDisconnectedEventHandler = WiFi.onStationModeDisconnected(
      [](const WiFiEventStationModeDisconnected& event) {
        lastWiFiDisconnectReason = (uint8_t)event.reason;
        wifiDisconnectEventPending = true;
      });
}

bool processPendingWiFiEvents() {
  bool hadDisconnectEvent = wifiDisconnectEventPending;
  bool hadGotIpEvent = wifiGotIpEventPending;

  if (hadDisconnectEvent) {
    uint8_t reason = lastWiFiDisconnectReason;
    wifiDisconnectEventPending = false;
    LOG.print(F("Событие отключения Wi-Fi, код "));
    LOG.print(reason);
    LOG.print(F(": "));
    LOG.println(wifiDisconnectReasonText(reason));
  }

  if (hadGotIpEvent) {
    wifiGotIpEventPending = false;
    lastNetworkSuccessMs = millis();
    lastWifiGatewayPingMs = millis();
    wifiGatewayPingFailureCount = 0;
    wifiGatewayInitialProbeFailures = 0;
    LOG.print(F("Wi-Fi получил IP-адрес: "));
    LOG.println(WiFi.localIP());
  }

  return hadDisconnectEvent && !hadGotIpEvent;
}

bool getConfiguredNetwork(uint8_t requestedIndex, String& ssid, String& password) {
  uint8_t foundIndex = 0;
  bool useAdditionalNetworks = jsonReadtoInt(configSetup, "wifi_multi") != 0;

  for (uint8_t slot = 0; slot < 3; slot++) {
    if (slot > 0 && !useAdditionalNetworks) break;

    String ssidKey = "ssid";
    String passwordKey = "password";
    if (slot > 0) {
      ssidKey += String(slot + 1);
      passwordKey += String(slot + 1);
    }
    String candidateSsid = jsonRead(configSetup, ssidKey);
    if (!candidateSsid.length()) continue;

    if (foundIndex == requestedIndex) {
      ssid = candidateSsid;
      password = jsonRead(configSetup, passwordKey);
      return true;
    }
    foundIndex++;
  }

  return false;
}

bool beginNextConfiguredNetwork() {
  if (configuredWiFiNetworks == 0) return false;

  uint8_t requestedIndex = nextWiFiNetworkIndex % configuredWiFiNetworks;
  String ssid;
  String password;
  if (!getConfiguredNetwork(requestedIndex, ssid, password)) {
    nextWiFiNetworkIndex = 0;
    requestedIndex = 0;
    if (!getConfiguredNetwork(0, ssid, password)) return false;
  }

  WiFi.mode(apFallbackActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.setAutoReconnect(false);
  applyWiFiPowerMode();
  applyStaticIpConfig();

  LOG.print(F("Попытка подключения к Wi-Fi: "));
  LOG.println(ssid);
  WiFi.begin(ssid.c_str(), password.c_str());

  nextWiFiNetworkIndex = (requestedIndex + 1) % configuredWiFiNetworks;
  lastRouterRetryMs = millis();
  return true;
}

void WIFIinit() {
  registerWiFiHandlers();
  registerWiFiEventHandlers();

  espMode = jsonReadtoInt(configSetup, "ESP_mode") ? 1 : 0;
  powerSavingMode = (uint8_t)jsonReadtoInt(configSetup, "power_mode");
  lightSleepSeconds = (uint16_t)jsonReadtoInt(configSetup, "light_sleep_seconds");
  lightAwakeSeconds = (uint16_t)jsonReadtoInt(configSetup, "light_awake_seconds");
  int configuredTimeout = jsonReadtoInt(configSetup, "TimeOut");
  ESP_CONN_TIMEOUT = configuredTimeout > 0 ? configuredTimeout : 60;
  if (ESP_CONN_TIMEOUT > 300) ESP_CONN_TIMEOUT = 300;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  lastWiFiPreventiveRecoveryMs = millis();
  lastWiFiHeapGuardMs = millis();

  LOG.print(F("Режим Wi-Fi: "));
  LOG.println(espMode == 0 ? F("точка доступа") : F("подключение к роутеру"));
  LOG.print(F("Энергосбережение: "));
  LOG.println(powerSavingModeText(powerSavingMode));

  if (espMode == 0) {
    StartAPMode();
    routerConnected = false;
    lastStationIP = IPAddress();
    return;
  }

  WiFi.mode(WIFI_STA);
  if (applyWiFiPowerMode()) {
    if (powerSavingMode == POWER_SAVE_OFF) {
      LOG.println(F("Wi-Fi без сна: радиомодуль всегда активен"));
    } else {
      LOG.println(F("Совместимый режим: в активном окне Wi-Fi работает без сна"));
    }
  } else {
    LOG.println(F("Не удалось применить выбранный режим питания Wi-Fi"));
  }
  applyStaticIpConfig();
  configuredWiFiNetworks = addConfiguredNetworks();
  if (configuredWiFiNetworks == 0) {
    LOG.println(F("SSID не задан. Запускается точка доступа."));
    apFallbackActive = StartAPMode();
    apFallbackStartMs = millis();
    lastRouterRetryMs = millis();
    lastStationIP = IPAddress();
    return;
  }

  LOG.print(F("Подключение к Wi-Fi"));
  uint32_t startTime = millis();
  uint16_t connectionTimeoutSeconds = ESP_CONN_TIMEOUT;
  if (compatiblePowerSavingActive() &&
      lightLevel <= lightTreshold &&
      connectionTimeoutSeconds > LIGHT_SLEEP_WIFI_TIMEOUT_SECONDS) {
    connectionTimeoutSeconds = LIGHT_SLEEP_WIFI_TIMEOUT_SECONDS;
    LOG.print(F(" (темно: тайм-аут "));
    LOG.print(connectionTimeoutSeconds);
    LOG.print(F(" с)"));
  }
  uint32_t timeout = (uint32_t)connectionTimeoutSeconds * 1000UL;
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    yield();
    LOG.print('.');
    if (millis() - startTime >= timeout) {
      LOG.println(F("\nНе удалось подключиться. Запускается временная точка доступа."));
      bool accessPointStarted = startAccessPoint(true);
      routerConnected = false;
      apFallbackActive = accessPointStarted;
      apFallbackStartMs = millis();
      lastRouterRetryMs = millis();
      lastStationIP = IPAddress();
      return;
    }
  }

  applyWiFiPowerMode();
  routerConnected = true;
  apFallbackActive = false;
  apFallbackStartMs = 0;
  lastRouterRetryMs = millis();
  lastStationIP = WiFi.localIP();
  wifiKnownGateway = WiFi.gatewayIP();
  lastWifiGatewayPingMs = millis();
  LOG.println(F("\nWi-Fi подключён!"));
  LOG.print(F("SSID: "));
  LOG.println(WiFi.SSID());
  LOG.print(F("IP: "));
  LOG.println(WiFi.localIP());
  LOG.print(F("RSSI: "));
  LOG.print(WiFi.RSSI());
  LOG.println(F(" dBm"));
}

void stopSSDP() {
  if (ssdpInitialized) SSDP.end();
  ssdpInitialized = false;
}

void restartSSDP() {
  if (WiFi.status() != WL_CONNECTED || WiFi.getMode() != WIFI_STA) {
    stopSSDP();
    return;
  }
  if (ssdpInitialized) SSDP.end();
  LOG.println(F("Инициализация SSDP..."));
  SSDP_init();
  ssdpInitialized = true;
}

void restartNetworkServices() {
  applyWiFiPowerMode();
  HTTP.stop();
  delay(1);
  HTTP.begin();
  restartDiscoveryUdp();
  restartSSDP();
  LOG.println(F("HTTP, UDP-поиск и SSDP перезапущены"));
}

void forceWiFiStackRecovery() {
  uint32_t now = millis();
  if (lastWiFiStackRecoveryMs != 0 &&
      now - lastWiFiStackRecoveryMs < WIFI_STACK_RECOVERY_COOLDOWN) {
    return;
  }
  lastWiFiStackRecoveryMs = now;
  lastWiFiPreventiveRecoveryMs = now;
  if (wifiRecoveryCount < 65535) wifiRecoveryCount++;
  wifiRecoveryValidationPending = true;
  wifiGatewayPingFailureCount = 0;

  LOG.print(F("Сетевой стек Wi-Fi: полное восстановление #"));
  LOG.println(wifiRecoveryCount);
  if (wifiGatewayPingInProgress) wifiGatewayPingIgnoreResult = true;
  stopMqttForNetworkRecovery();
  HTTP.stop();
  stopDiscoveryUdp();
  stopSSDP();
  WiFiClient::stopAll();

  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false);
  delay(20);
  WiFi.mode(WIFI_OFF);
  delay(100);

  wifiDisconnectEventPending = false;
  wifiGotIpEventPending = false;
  routerConnected = false;
  lastStationIP = IPAddress();
  apFallbackStartMs = millis();
  lastRouterRetryMs = millis();
  wifiGatewayPingInProgress = false;
  wifiGatewayPingFinished = false;
  wifiGatewayPingReceived = false;
  wifiWebHealthClient.stop(0);
  wifiWebHealthTestInProgress = false;
  wifiWebHealthResponseLength = 0;
  lastWifiGatewayPingMs = millis();
  lastWifiWebHealthTestMs = millis();

  bool accessPointStarted = startAccessPoint(true);
  apFallbackActive = accessPointStarted;
  apFallbackStartMs = millis();
  if (!accessPointStarted) {
    LOG.println(F("Радиомодуль не создал точку доступа. Выполняется перезапуск ESP8266."));
    Serial.flush();
    delay(250);
    ESP.restart();
    return;
  }

  restartNetworkServices();
  wifiDisconnectEventPending = false;
  wifiGotIpEventPending = false;
  if (configuredWiFiNetworks > 0 && beginNextConfiguredNetwork()) {
    LOG.println(F("Подключение к роутеру повторно запущено; сервисная AP пока оставлена"));
  } else {
    LOG.println(F("Сохранённых сетей нет; устройство остаётся в сервисной AP"));
  }
}

void handleGatewayHealthResult(bool success) {
  if (success) {
    lastNetworkSuccessMs = millis();
    bool firstSuccess = !wifiGatewayPingSupported;
    wifiGatewayPingSupported = true;
    wifiGatewayInitialProbeFailures = 0;
    wifiGatewayPingFailureCount = 0;
    if (firstSuccess) {
      LOG.println(F("Активный контроль Wi-Fi включён: шлюз отвечает"));
    }
    if (wifiRecoveryValidationPending) {
      wifiRecoveryValidationPending = false;
      LOG.println(F("Wi-Fi после восстановления проверен, шлюз доступен"));
    }
    return;
  }

  if (!wifiGatewayPingSupported) {
    if (wifiGatewayInitialProbeFailures < 255) {
      wifiGatewayInitialProbeFailures++;
    }
    if (wifiGatewayInitialProbeFailures == WIFI_HEALTH_INITIAL_PROBE_LIMIT) {
      LOG.println(F("Шлюз не отвечает на Ping; проверка останется пассивной"));
    }
    return;
  }

  if (wifiGatewayPingFailureCount < 255) wifiGatewayPingFailureCount++;
  LOG.print(F("Нет ответа от шлюза Wi-Fi: "));
  LOG.print(wifiGatewayPingFailureCount);
  LOG.print('/');
  LOG.println(WIFI_HEALTH_FAILURE_LIMIT);

  if (wifiGatewayPingFailureCount < WIFI_HEALTH_FAILURE_LIMIT) return;

  if (wifiRecoveryValidationPending && apFallbackActive) {
    wifiGatewayPingFailureCount = 0;
    LOG.println(F("Роутер пока недоступен; сервисная точка доступа оставлена включённой"));
    return;
  }

  forceWiFiStackRecovery();
}

IPAddress getWebHealthTarget() {
  if (apFallbackActive) {
    WiFiMode_t mode = WiFi.getMode();
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
      IPAddress apAddress = WiFi.softAPIP();
      if (stationIpIsValid(apAddress)) return apAddress;
    }
  }
  if (stationHasValidConnection()) return WiFi.localIP();
  return IPAddress();
}

void finishWebHealthTest(bool success) {
  wifiWebHealthClient.stop(0);
  wifiWebHealthTestInProgress = false;
  wifiWebHealthResponseLength = 0;

  if (success) {
    if (!wifiWebHealthTestSupported) {
      LOG.println(F("Активный контроль TCP/Web включён"));
    }
    wifiWebHealthTestSupported = true;
    wifiWebHealthInitialProbeFailures = 0;
    wifiWebHealthFailureCount = 0;
    return;
  }

  if (!wifiWebHealthTestSupported) {
    if (wifiWebHealthInitialProbeFailures < 255) {
      wifiWebHealthInitialProbeFailures++;
    }
    if (wifiWebHealthInitialProbeFailures == WIFI_HEALTH_INITIAL_PROBE_LIMIT) {
      LOG.println(F("Внутренняя TCP-проверка недоступна; оставлена пассивной"));
    }
    return;
  }

  if (wifiWebHealthFailureCount < 255) wifiWebHealthFailureCount++;
  LOG.print(F("Внутренняя проверка TCP/Web не прошла: "));
  LOG.print(wifiWebHealthFailureCount);
  LOG.print('/');
  LOG.println(WIFI_HEALTH_FAILURE_LIMIT);
  if (wifiWebHealthFailureCount < WIFI_HEALTH_FAILURE_LIMIT) return;

  if (wifiRecoveryValidationPending && apFallbackActive) {
    LOG.println(F("TCP/Web не восстановились через сервисную AP. Перезапуск ESP8266."));
    Serial.flush();
    delay(250);
    ESP.restart();
    return;
  }
  forceWiFiStackRecovery();
}

bool startWebHealthTest() {
  if (wifiWebHealthTestInProgress) return false;
  IPAddress target = getWebHealthTarget();
  if (!stationIpIsValid(target)) return false;

  wifiWebHealthClient.stop(0);
  wifiWebHealthClient.setTimeout(250);
  wifiWebHealthTarget = target;
  lastWifiWebHealthTestMs = millis();
  if (!wifiWebHealthClient.connect(target, 80)) {
    return false;
  }

  size_t sent = wifiWebHealthClient.print(
      F("GET /network_health HTTP/1.1\r\nHost: AirWick\r\nConnection: close\r\n\r\n"));
  if (sent == 0) {
    wifiWebHealthClient.stop(0);
    return false;
  }

  wifiWebHealthResponseLength = 0;
  wifiWebHealthResponse[0] = '\0';
  wifiWebHealthTestStartedMs = millis();
  wifiWebHealthTestInProgress = true;
  return true;
}

void webHealthLoop() {
  uint32_t now = millis();
  if (wifiWebHealthTestInProgress) {
    IPAddress currentTarget = getWebHealthTarget();
    if (!stationIpIsValid(currentTarget) ||
        !sameIpAddress(currentTarget, wifiWebHealthTarget)) {
      wifiWebHealthClient.stop(0);
      wifiWebHealthTestInProgress = false;
      wifiWebHealthResponseLength = 0;
      lastWifiWebHealthTestMs = now;
      return;
    }

    while (wifiWebHealthClient.available() &&
           wifiWebHealthResponseLength < sizeof(wifiWebHealthResponse) - 1) {
      char c = (char)wifiWebHealthClient.read();
      wifiWebHealthResponse[wifiWebHealthResponseLength++] = c;
      wifiWebHealthResponse[wifiWebHealthResponseLength] = '\0';
      if (c == '\n') {
        bool success = strstr(wifiWebHealthResponse, " 204 ") != nullptr;
        finishWebHealthTest(success);
        return;
      }
    }

    if (wifiWebHealthResponseLength >= sizeof(wifiWebHealthResponse) - 1 ||
        now - wifiWebHealthTestStartedMs >= WIFI_WEB_SELF_TEST_TIMEOUT ||
        (!wifiWebHealthClient.connected() && !wifiWebHealthClient.available())) {
      finishWebHealthTest(false);
    }
    return;
  }

  uint32_t checkInterval = (wifiWebHealthTestSupported ||
                           wifiWebHealthInitialProbeFailures < WIFI_HEALTH_INITIAL_PROBE_LIMIT) ?
                           WIFI_WEB_SELF_TEST_INTERVAL : WIFI_HEALTH_PASSIVE_INTERVAL;
  if (lastWifiWebHealthTestMs != 0 &&
      now - lastWifiWebHealthTestMs < checkInterval) {
    return;
  }

  IPAddress target = getWebHealthTarget();
  if (!stationIpIsValid(target)) return;
  if (!startWebHealthTest()) {
    lastWifiWebHealthTestMs = now;
    finishWebHealthTest(false);
  }
}

void wifiHealthLoop() {
  if (espMode != 1 || compatiblePowerSavingActive()) {
    if (wifiGatewayPingInProgress) wifiGatewayPingIgnoreResult = true;
    wifiGatewayPingInProgress = false;
    wifiGatewayPingFinished = false;
    if (wifiWebHealthTestInProgress) wifiWebHealthClient.stop(0);
    wifiWebHealthTestInProgress = false;
    wifiWebHealthResponseLength = 0;
    return;
  }

  uint32_t now = millis();

  if (lastWiFiHeapGuardMs == 0 ||
      now - lastWiFiHeapGuardMs >= WIFI_HEAP_GUARD_INTERVAL) {
    lastWiFiHeapGuardMs = now;
    uint32_t freeHeap = ESP.getFreeHeap();
    if (freeHeap < WIFI_CRITICAL_FREE_HEAP) {
      LOG.print(F("Критически мало памяти для сети: "));
      LOG.print(freeHeap);
      LOG.println(F(" байт. Перезапуск ESP8266."));
      Serial.flush();
      delay(100);
      ESP.restart();
      return;
    }
  }

  if (lastWiFiPreventiveRecoveryMs == 0) lastWiFiPreventiveRecoveryMs = now;
  uint32_t lastNetworkProofMs = lastNetworkSuccessMs != 0 ?
                                lastNetworkSuccessMs :
                                lastWiFiPreventiveRecoveryMs;
  if (stationHasValidConnection() &&
      now - lastNetworkProofMs >= WIFI_PREVENTIVE_RECOVERY_INTERVAL) {
    lastWiFiPreventiveRecoveryMs = now;
    LOG.println(F("Сеть давно не подтверждала обмен данными. Восстановление Wi-Fi стека"));
    forceWiFiStackRecovery();
    return;
  }
  if (wifiGatewayPingInProgress) {
    if (now - wifiGatewayPingStartedMs >= WIFI_HEALTH_PING_GUARD_MS) {
      LOG.println(F("Контрольный Ping Wi-Fi завис"));
      wifiGatewayPingIgnoreResult = true;
      wifiGatewayPingInProgress = false;
      wifiGatewayPingFinished = false;
      if (wifiGatewayPingSupported) {
        forceWiFiStackRecovery();
      }
    }
    return;
  }

  if (wifiGatewayPingFinished) {
    bool success = wifiGatewayPingReceived;
    IPAddress currentGateway = WiFi.gatewayIP();
    wifiGatewayPingFinished = false;
    wifiGatewayPingReceived = false;
    if (stationHasValidConnection() &&
        sameIpAddress(currentGateway, wifiGatewayPingTarget)) {
      handleGatewayHealthResult(success);
    }
    return;
  }

  if (!stationHasValidConnection()) {
    wifiGatewayPingFailureCount = 0;
    return;
  }

  uint32_t checkInterval = (wifiGatewayPingSupported ||
                           wifiGatewayInitialProbeFailures < WIFI_HEALTH_INITIAL_PROBE_LIMIT) ?
                           WIFI_HEALTH_CHECK_INTERVAL : WIFI_HEALTH_PASSIVE_INTERVAL;
  if (lastWifiGatewayPingMs != 0 &&
      now - lastWifiGatewayPingMs < checkInterval) {
    return;
  }

  if (!startGatewayHealthPing()) {
    lastWifiGatewayPingMs = now;
    handleGatewayHealthResult(false);
  }
}

void checkWiFiFallback() {
  if (espMode == 0) {
    apFallbackActive = false;
    uint32_t now = millis();
    if (lastAccessPointHealthCheckMs != 0 &&
        now - lastAccessPointHealthCheckMs < WIFI_AP_HEALTH_CHECK_INTERVAL) {
      return;
    }
    lastAccessPointHealthCheckMs = now;
    WiFiMode_t mode = WiFi.getMode();
    bool accessPointHealthy = (mode == WIFI_AP || mode == WIFI_AP_STA) &&
                              stationIpIsValid(WiFi.softAPIP());
    if (!accessPointHealthy) {
      LOG.println(F("Точка доступа Wi-Fi остановлена. Выполняется восстановление."));
      HTTP.stop();
      stopDiscoveryUdp();
      WiFiClient::stopAll();
      if (!StartAPMode()) {
        LOG.println(F("Не удалось восстановить AP. Выполняется перезапуск ESP8266."));
        Serial.flush();
        delay(250);
        ESP.restart();
        return;
      }
      restartNetworkServices();
    }
    return;
  }

  if (apFallbackActive) {
    WiFiMode_t mode = WiFi.getMode();
    bool accessPointHealthy = (mode == WIFI_AP || mode == WIFI_AP_STA) &&
                              stationIpIsValid(WiFi.softAPIP());
    if (!accessPointHealthy) {
      apFallbackActive = false;
      apFallbackStartMs = millis();
      LOG.println(F("Сервисная точка доступа остановилась; будет создана повторно"));
    }
  }

  if (stationHasValidConnection() && !wifiRecoveryValidationPending) {
    if (apFallbackActive) {
      if (WiFi.softAPdisconnect(true)) {
        applyWiFiPowerMode();
        LOG.println(F("Wi-Fi восстановлен, временная точка доступа выключена."));
        restartNetworkServices();
      } else {
        LOG.println(F("Не удалось выключить временную точку доступа"));
        return;
      }
    }
    apFallbackActive = false;
    apFallbackStartMs = 0;
    return;
  }

  if (!apFallbackActive) {
    if (apFallbackStartMs == 0) apFallbackStartMs = millis();
    if (millis() - apFallbackStartMs >= WIFI_FALLBACK_DELAY) {
      LOG.println(F("Wi-Fi потерян. Запускается временная точка доступа."));
      if (startAccessPoint(true)) {
        apFallbackActive = true;
        lastRouterRetryMs = millis();
        restartNetworkServices();
      } else {
        LOG.println(F("Не удалось запустить временную точку доступа"));
        apFallbackStartMs = millis();
      }
    }
  }
}

void wifiReconnect() {
  bool disconnectEventRequiresRecovery = processPendingWiFiEvents();

  uint32_t now = millis();
  bool currentlyConnected = stationHasValidConnection() &&
                            !disconnectEventRequiresRecovery;

  if (currentlyConnected) {
    IPAddress currentIP = WiFi.localIP();
    bool ipChanged = routerConnected && !sameIpAddress(currentIP, lastStationIP);

    if (!routerConnected || ipChanged) {
      routerConnected = true;
      apFallbackStartMs = 0;
      lastRouterRetryMs = now;
      lastStationIP = currentIP;
      lastNetworkSuccessMs = now;
      LOG.println(ipChanged ? F("IP-адрес Wi-Fi изменился") : F("Wi-Fi восстановлен!"));
      LOG.print(F("SSID: "));
      LOG.println(WiFi.SSID());
      LOG.print(F("IP: "));
      LOG.println(currentIP);
      LOG.print(F("RSSI: "));
      LOG.print(WiFi.RSSI());
      LOG.println(F(" dBm"));
      if (wifiRecoveryValidationPending) {
        wifiRecoveryValidationPending = false;
        LOG.println(F("Wi-Fi после полного восстановления снова получил IP"));
      }
      restartNetworkServices();
    }

    if (lastWiFiDiagnosticMs == 0 ||
        now - lastWiFiDiagnosticMs >= WIFI_DIAGNOSTIC_INTERVAL) {
      lastWiFiDiagnosticMs = now;
      LOG.print(F("Wi-Fi работает: IP="));
      LOG.print(currentIP);
      LOG.print(F(", RSSI="));
      LOG.print(WiFi.RSSI());
      LOG.print(F(" dBm, свободная память="));
      LOG.print(ESP.getFreeHeap());
      LOG.println(F(" байт"));
    }
    return;
  }

  if (routerConnected && espMode == 1) {
    routerConnected = false;
    lastStationIP = IPAddress();
    stopSSDP();
    apFallbackStartMs = now;
    LOG.print(F("Соединение с роутером потеряно, status="));
    LOG.print((int)WiFi.status());
    LOG.print(F(", свободная память="));
    LOG.print(ESP.getFreeHeap());
    LOG.println(F(" байт"));

    lastRouterRetryMs = now;
    applyWiFiPowerMode();
    if (WiFi.reconnect()) {
      LOG.println(F("Запущено немедленное переподключение к последней сети"));
    } else {
      LOG.println(F("Последняя сеть недоступна, используется сохранённый список"));
      beginNextConfiguredNetwork();
    }
    return;
  }

  if (espMode == 1 && configuredWiFiNetworks > 0 &&
      now - lastRouterRetryMs >= WIFI_ROUTER_RETRY_INTERVAL) {
    beginNextConfiguredNetwork();
  }
}
