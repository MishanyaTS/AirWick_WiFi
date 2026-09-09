
void HTTP_init(void) {

  HTTP.on("/network_health", HTTP_GET, []() {
    HTTP.send(204, "text/plain", "");
  });

  // --------------------Выдаем данные configJson
  HTTP.on("/config.live.json", HTTP_GET, []() {
    notePowerSavingWebActivity();
    outData();
    HTTP.send(200, "application/json", configJson);
  });
  
  // -------------------Выдаем данные configSetup
  HTTP.on("/config.setup.json", HTTP_GET, []() {
    notePowerSavingWebActivity();
    HTTP.send(200, "application/json", configSetup);
  });

  // IP-адрес для пункта меню «Статусы устройств».
  HTTP.on("/wifi_ip", HTTP_GET, []() {
    notePowerSavingWebActivity();
    DynamicJsonDocument doc(512);
    WiFiMode_t mode = WiFi.getMode();
    String ip;

    if (stationHasValidConnection()) {
      ip = WiFi.localIP().toString();
    } else if (mode == WIFI_AP || mode == WIFI_AP_STA) {
      ip = WiFi.softAPIP().toString();
    }
    if (!ip.length() || ip == "0.0.0.0") ip = "Не получен IP";

    doc["ip"] = ip;
    doc["sta_connected"] = stationHasValidConnection();
    doc["ap_active"] = (mode == WIFI_AP || mode == WIFI_AP_STA);
    doc["wifi_mode"] = mode == WIFI_AP ? "AP" :
                       mode == WIFI_STA ? "Station" : "AP+Station";
    doc["power_mode"] = powerSavingMode == POWER_SAVE_LIGHT ?
                        "compatible" : "off";
    doc["wifi_sleep"] = "none";
    doc["light_sleep_seconds"] = lightSleepSeconds;
    doc["light_awake_seconds"] = lightAwakeSeconds;
    doc["light_sleep_allowed"] = compatiblePowerSleepAllowed();
    doc["light_sleep_in"] = compatiblePowerSecondsUntilSleep();
    doc["light_sleep_compatible"] = compatiblePowerSavingActive();

    String response;
    serializeJson(doc, response);
    HTTP.send(200, "application/json; charset=utf-8", response);
  });

  HTTP.on("/heap", HTTP_GET, []() {
    notePowerSavingWebActivity();
    DynamicJsonDocument doc(384);
    doc["free"] = ESP.getFreeHeap();
    doc["max_block"] = ESP.getMaxFreeBlockSize();
    doc["fragmentation"] = ESP.getHeapFragmentation();
    doc["uptime"] = millis() / 1000UL;
    doc["power_mode"] = powerSavingMode;
    doc["light_sleep_in"] = compatiblePowerSecondsUntilSleep();

    String response;
    serializeJson(doc, response);
    HTTP.send(200, "application/json; charset=utf-8", response);
  });

  HTTP.on("/web_closed", []() {
    releasePowerSavingWebActivity();
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  // -------------------Обработка Restart
  HTTP.on("/restart", HTTP_GET, []() {
    String restart = HTTP.arg("device");          // Получаем значение device из запроса
    if (restart == "ok") {                         // Если значение равно Ок
      LOG.println(F("Получена команда перезагрузки через WEB"));
      HTTP.send(200, "text / plain", "Reset OK"); // Oтправляем ответ Reset OK
      delay(1000);
      ESP.restart();                                // перезагружаем модуль
    }
    else {                                        // иначе
      HTTP.send(200, "text / plain", "No Reset"); // Oтправляем ответ No Reset
    }
  });


  // Добавляем функцию Update для перезаписи прошивки по WiFi при 1М(256K LittleFS) и выше
  httpUpdater.setup(&HTTP);
  // Добавляем обработчик для кнопки
  HTTP.on("/motor", []() {
    notePowerSavingWebActivity();
    bool sprayed = activateSprayer(F("Команда из веб-интерфейса"), "web");
    if (sprayed) {
      HTTP.send(200, "application/json", "{\"ok\":true,\"sprayed\":true}");
    } else {
      String response = String("{\"ok\":false,\"sprayed\":false,\"reason\":\"cooldown\",\"cooldown_ms\":") +
                        sprayCooldownRemainingMs() + "}";
      HTTP.send(200, "application/json", response);
    }
  });
  // Запускаем HTTP сервер после регистрации всех обработчиков.
  HTTP.begin();
  LOG.println(F("HTTP-сервер запущен, порт 80"));
}
