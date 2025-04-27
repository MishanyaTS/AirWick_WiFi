void WIFIinit() {
  // Получаем параметры WiFi со страницы
  HTTP.on("/ssid", HTTP_GET, []() {
    jsonWrite(configSetup, "ssid", HTTP.arg("ssid"));
    jsonWrite(configSetup, "password", HTTP.arg("password"));
    saveConfig(); // Сохраняем данные во Flash
    HTTP.send(200, "text/plain", "OK"); // Отправляем ответ о выполнении
  });

  // Получаем параметры точки доступа со страницы
  HTTP.on("/ssidap", HTTP_GET, []() {
    jsonWrite(configSetup, "ssidAP", HTTP.arg("ssidAP"));
    jsonWrite(configSetup, "passwordAP", HTTP.arg("passwordAP"));
    saveConfig(); // Сохраняем данные во Flash
    HTTP.send(200, "text/plain", "OK"); // Отправляем ответ о выполнении
  });

  // Попытка подключения к роутеру
  WiFi.mode(WIFI_STA);
  byte tries = 10;
  String _ssid = jsonRead(configSetup, "ssid");
  String _password = jsonRead(configSetup, "password");

  if (_ssid.isEmpty() || _password.isEmpty()) {
    WiFi.begin(); // Если нет SSID и пароля, подключаемся к открытой сети
  } else {
    if (use_static_ip) {
      // Конфигурация статического IP
      if (IPAddress(Static_IP) && IPAddress(Gateway) && IPAddress(Subnet) && IPAddress(DNS1)) {
        WiFi.config(Static_IP, Gateway, Subnet, DNS1); 
      } else {
        Serial.println("Ошибка: некорректные параметры статического IP.");
        return; // Не продолжаем, если параметры неверные
      }
    }
    
    WiFi.begin(_ssid.c_str(), _password.c_str());
  }

  // Проверяем подключение
  while (--tries && WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Если не удалось подключиться, запускаем в режиме AP
    Serial.println();
    Serial.println("Не удалось подключиться к Wi-Fi");
    Serial.println("Создание точки доступа");
    StartAPMode();
  } else {
    // Подключение успешно
    Serial.println();
    Serial.println("Подключено к Wi-Fi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

bool StartAPMode() {
  // Отключаем WiFi
  WiFi.disconnect();
  // Переключаем режим на точку доступа
  WiFi.mode(WIFI_AP);
  
  // Настройки AP
  String _ssidAP = jsonRead(configSetup, "ssidAP");
  String _passwordAP = jsonRead(configSetup, "passwordAP");
  WiFi.softAP(_ssidAP.c_str(), _passwordAP.c_str());
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.println("Точка доступа создана");
  Serial.print("IP address: ");
  Serial.println(myIP);
  return true;
}
