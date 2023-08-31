void Timer_init() {
  HTTP.on("/setTimers", handle_set_timers);    // Установка значений таймеров
  HTTP.on("/setpretimer", handle_set_pretimer);    // Установка значений предварительного таймера
  HTTP.on("/setmaintimer", handle_set_maintimer);    // Установка значений предварительного таймера
  timerDuration=jsonReadtoInt(configSetup, "Interval")*60000;//читаем значение интервала из конфига и переводим в миллисекунды
  preTimer=jsonReadtoInt(configSetup, "preTimer")*60000;//читаем значение предварительного таймера из конфига и переводим в миллисекунды
}

// Установка значений таймеров запросу вида http://192.168.0.101/setTimers?pretimer=1&interval=4
void handle_set_timers() {
  int temppre=HTTP.arg("pretimer").toInt();// Получаем значение pretimer из запроса конвертируем в int
  int tempint=HTTP.arg("interval").toInt();// Получаем значение interval из запроса конвертируем в int
  jsonWrite(configSetup, "preTimer", temppre); // сохраняем в json
  jsonWrite(configSetup, "Interval", tempint);  //сохраняем в json
  preTimer=temppre*60000;//переводим в миллисекунды и присваиваем новое значение таймеру интервала
  timerDuration=tempint*60000;//переводим в миллисекунды и присваиваем новое значение таймеру интервала
  saveConfig();
  HTTP.send(200, "text/plain", "OK");
}

// Установка значений таймеров запросу вида http://192.168.0.101/setpretimer?val=1
void handle_set_pretimer() {
  preTimer=HTTP.arg("val").toInt()*60000;// Получаем значение pretimer из запроса конвертируем, переводим в миллисекунды и присваиваем новое значение предварительному таймеру 
  HTTP.send(200, "text/plain", "OK");
}

// Установка значений таймеров запросу вида http://192.168.0.101/setmaintimer?val=1
void handle_set_maintimer() {
  timerDuration=HTTP.arg("val").toInt()*60000;// Получаем значение maintimer из запроса конвертируем, переводим в миллисекунды и присваиваем новое значение предварительному таймеру 
  HTTP.send(200, "text/plain", "OK");
}
