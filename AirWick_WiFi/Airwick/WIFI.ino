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

  if (changed) saveConfig();
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
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  HTTP.on("/wifi_multi", HTTP_GET, []() {
    jsonWrite(configSetup, "wifi_multi", HTTP.arg("wifi_multi").toInt() ? 1 : 0);
    saveConfig();
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
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  HTTP.on("/ssidap", HTTP_GET, []() {
    String ssidAP = HTTP.hasArg("ssidAP") ? HTTP.arg("ssidAP") : jsonRead(configSetup, "ssidAP");
    String passwordAP = HTTP.hasArg("passwordAP") ? HTTP.arg("passwordAP") : jsonRead(configSetup, "passwordAP");
    if (!ssidAP.length() || ssidAP.length() > 32 ||
        passwordAP.length() < 8 || passwordAP.length() > 63) {
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"invalid access point settings\"}");
      return;
    }
    jsonWrite(configSetup, "ssidAP", ssidAP);
    jsonWrite(configSetup, "passwordAP", passwordAP);
    saveConfig();
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
  Serial.print(keepStation ? "Временная точка доступа запущена: " : "Точка доступа запущена: ");
  Serial.println(WiFi.softAPIP());
  return started;
}

bool StartAPMode() {
  return startAccessPoint(false);
}

bool applyStaticIpConfig() {
  // Статический адрес выключен — обязательно запускаем DHCP.
  if (!use_static_ip) {
    Serial.println(F("Статический IP выключен. Включается DHCP."));

    if (!WiFi.config(0U, 0U, 0U)) {
      Serial.println(F("Не удалось включить DHCP."));
      return false;
    }

    return true;
  }

  // Если настройки статического IP повреждены — используем DHCP.
  if (!staticIpConfigValid) {
    Serial.println(F("Неверные настройки статического IP. Включается DHCP."));
    WiFi.config(0U, 0U, 0U);
    return false;
  }

  // Применяем статический IP.
  if (!WiFi.config(Static_IP, Gateway, Subnet, DNS1, DNS2)) {
    Serial.println(F("Не удалось применить статический IP. Включается DHCP."));
    WiFi.config(0U, 0U, 0U);
    return false;
  }

  Serial.print(F("Используется статический IP: "));
  Serial.println(Static_IP);
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
      Serial.print("Добавлена сеть 2: ");
      Serial.println(ssid2);
    }
    if (ssid3.length()) {
      wifiMulti.addAP(ssid3.c_str(), password3.c_str());
      count++;
      Serial.print("Добавлена сеть 3: ");
      Serial.println(ssid3);
    }
  }
  return count;
}

void WIFIinit() {
  registerWiFiHandlers();

  espMode = jsonReadtoInt(configSetup, "ESP_mode") ? 1 : 0;
  int configuredTimeout = jsonReadtoInt(configSetup, "TimeOut");
  ESP_CONN_TIMEOUT = configuredTimeout > 0 ? configuredTimeout : 60;
  if (ESP_CONN_TIMEOUT > 300) ESP_CONN_TIMEOUT = 300;

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  Serial.print("Режим Wi-Fi: ");
  Serial.println(espMode == 0 ? "точка доступа" : "подключение к роутеру");

  if (espMode == 0) {
    StartAPMode();
    routerConnected = false;
    return;
  }

  WiFi.mode(WIFI_STA);
  applyStaticIpConfig();
  configuredWiFiNetworks = addConfiguredNetworks();
  if (configuredWiFiNetworks == 0) {
    Serial.println("SSID не задан. Запускается точка доступа.");
    StartAPMode();
    apFallbackActive = true;
    apFallbackStartMs = millis();
    lastRouterRetryMs = millis();
    return;
  }

  Serial.print("Подключение к Wi-Fi");
  uint32_t startTime = millis();
  uint32_t timeout = (uint32_t)ESP_CONN_TIMEOUT * 1000UL;
  while (wifiMulti.run() != WL_CONNECTED) {
    delay(500);
    yield();
    Serial.print('.');
    if (millis() - startTime >= timeout) {
      Serial.println("\nНе удалось подключиться. Запускается временная точка доступа.");
      startAccessPoint(true);
      routerConnected = false;
      apFallbackActive = true;
      apFallbackStartMs = millis();
      lastRouterRetryMs = millis();
      return;
    }
  }

  routerConnected = true;
  apFallbackActive = false;
  apFallbackStartMs = 0;
  Serial.println("\nWi-Fi подключён!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
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
  Serial.println("Инициализация SSDP...");
  SSDP_init();
  ssdpInitialized = true;
}

void checkWiFiFallback() {
  if (espMode == 0) {
    apFallbackActive = false;
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (apFallbackActive) {
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      Serial.println("Wi-Fi восстановлен, временная точка доступа выключена.");
    }
    apFallbackActive = false;
    apFallbackStartMs = 0;
    return;
  }

  if (!apFallbackActive) {
    if (apFallbackStartMs == 0) apFallbackStartMs = millis();
    if (millis() - apFallbackStartMs >= WIFI_FALLBACK_DELAY) {
      Serial.println("Wi-Fi потерян. Запускается временная точка доступа.");
      startAccessPoint(true);
      apFallbackActive = true;
      lastRouterRetryMs = millis();
      restartSSDP();
    }
  }
}

void wifiReconnect() {
  bool currentlyConnected = WiFi.status() == WL_CONNECTED;

  if (currentlyConnected && !routerConnected) {
    routerConnected = true;
    routerRetryActive = false;
    apFallbackStartMs = 0;
    Serial.println("Wi-Fi подключён!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    restartSSDP();
    return;
  }

  if (!currentlyConnected && routerConnected && espMode == 1) {
    routerConnected = false;
    routerRetryActive = false;
    stopSSDP();
    apFallbackStartMs = millis();
  }

  if (espMode == 1 && configuredWiFiNetworks > 0 && !currentlyConnected) {
    uint32_t now = millis();
    if (!routerRetryActive && now - lastRouterRetryMs >= WIFI_ROUTER_RETRY_INTERVAL) {
      routerRetryActive = true;
      routerRetryStartMs = now;
      lastRouterRetryStepMs = 0;
      lastRouterRetryMs = now;
      Serial.println("Повторная попытка подключения к роутеру...");
    }

    if (routerRetryActive) {
      if (now - routerRetryStartMs >= WIFI_ROUTER_RETRY_WINDOW) {
        routerRetryActive = false;
        Serial.println("Роутер пока недоступен. Следующая попытка через 5 минут.");
      } else if (lastRouterRetryStepMs == 0 ||
                 now - lastRouterRetryStepMs >= WIFI_ROUTER_RETRY_STEP) {
        lastRouterRetryStepMs = now;
        wifiMulti.run();
      }
    }
  }
}
