
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
  // Запускаем HTTP сервер
  HTTP.begin();
  // Добавляем обработчик для кнопки
  HTTP.on("/motor", []() {
    // Включаем мотор на 1 секунду
    Serial.println("Кнопка нажата в веб интерфейсе");
    digitalWrite(motorPin, HIGH);
    delay(50);
    digitalWrite(motorPin, LOW);
    // Возвращаем ответ на страницу
    HTTP.send(200, "text/plain", "Motor started for 1 second");
  });
}
