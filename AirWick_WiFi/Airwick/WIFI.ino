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

bool keepWiFiRadioAwake() {
  // Освежитель постоянно питается от сети: радиомодуль Wi-Fi не должен
  // переходить ни в один из режимов энергосбережения.
  return WiFi.setSleepMode(WIFI_NONE_SLEEP);
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
  // Статический адрес выключен — обязательно запускаем DHCP.
  if (!use_static_ip) {
    LOG.println(F("Статический IP выключен. Включается DHCP."));

    if (!WiFi.config(0U, 0U, 0U)) {
      LOG.println(F("Не удалось включить DHCP."));
      return false;
    }

    return true;
  }

  // Если настройки статического IP повреждены — используем DHCP.
  if (!staticIpConfigValid) {
    LOG.println(F("Неверные настройки статического IP. Включается DHCP."));
    WiFi.config(0U, 0U, 0U);
    return false;
  }

  // Применяем статический IP.
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

bool stationHasValidConnection() {
  return WiFi.status() == WL_CONNECTED && stationIpIsValid(WiFi.localIP());
}

bool sameIpAddress(const IPAddress& first, const IPAddress& second) {
  for (uint8_t i = 0; i < 4; i++) {
    if (first[i] != second[i]) return false;
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
    LOG.print(F("Wi-Fi получил IP-адрес: "));
    LOG.println(WiFi.localIP());
  }

  // На некоторых версиях ядра ESP8266 WiFi.status() некоторое время может
  // оставаться WL_CONNECTED после фактического обрыва. Событие отключения
  // считаем приоритетным, если после него ещё не было события получения IP.
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
  keepWiFiRadioAwake();
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
  int configuredTimeout = jsonReadtoInt(configSetup, "TimeOut");
  ESP_CONN_TIMEOUT = configuredTimeout > 0 ? configuredTimeout : 60;
  if (ESP_CONN_TIMEOUT > 300) ESP_CONN_TIMEOUT = 300;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  LOG.print(F("Режим Wi-Fi: "));
  LOG.println(espMode == 0 ? F("точка доступа") : F("подключение к роутеру"));

  if (espMode == 0) {
    StartAPMode();
    routerConnected = false;
    lastStationIP = IPAddress();
    return;
  }

  WiFi.mode(WIFI_STA);
  if (keepWiFiRadioAwake()) {
    LOG.println(F("Энергосбережение Wi-Fi полностью выключено: радиомодуль всегда активен"));
  } else {
    LOG.println(F("Не удалось выключить энергосбережение Wi-Fi"));
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
  uint32_t timeout = (uint32_t)ESP_CONN_TIMEOUT * 1000UL;
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

  keepWiFiRadioAwake();
  routerConnected = true;
  apFallbackActive = false;
  apFallbackStartMs = 0;
  lastRouterRetryMs = millis();
  lastStationIP = WiFi.localIP();
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
  keepWiFiRadioAwake();
  HTTP.stop();
  delay(1);
  HTTP.begin();
  restartDiscoveryUdp();
  restartSSDP();
  LOG.println(F("HTTP, UDP-поиск и SSDP перезапущены"));
}

void checkWiFiFallback() {
  if (espMode == 0) {
    apFallbackActive = false;
    return;
  }

  if (stationHasValidConnection()) {
    if (apFallbackActive) {
      if (WiFi.softAPdisconnect(true)) {
        keepWiFiRadioAwake();
        LOG.println(F("Wi-Fi восстановлен, временная точка доступа выключена."));
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
      LOG.println(ipChanged ? F("IP-адрес Wi-Fi изменился") : F("Wi-Fi восстановлен!"));
      LOG.print(F("SSID: "));
      LOG.println(WiFi.SSID());
      LOG.print(F("IP: "));
      LOG.println(currentIP);
      LOG.print(F("RSSI: "));
      LOG.print(WiFi.RSSI());
      LOG.println(F(" dBm"));
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
    keepWiFiRadioAwake();
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
