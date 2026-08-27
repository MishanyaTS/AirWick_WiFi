
void HTTP_init(void) {

  // --------------------Выдаем данные configJson
  HTTP.on("/config.live.json", HTTP_GET, []() {
    outData();
    HTTP.send(200, "application/json", configJson);
  });
  
  // -------------------Выдаем данные configSetup
  HTTP.on("/config.setup.json", HTTP_GET, []() {
    HTTP.send(200, "application/json", configSetup);
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
