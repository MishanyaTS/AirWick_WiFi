void Timer_init() {
  HTTP.on("/setPreTimers", handle_Pretimers);   // Установка значений предварительного таймера
  HTTP.on("/setpretimer", handle_pretimer);     // Установка значений предварительного таймера  
  HTTP.on("/preTimerm", handle_preTimerm);      // Обработчик для уменьшения предтаймера
  HTTP.on("/preTimerp", handle_preTimerp);      // Обработчик для увеличения предтаймера 
   
  HTTP.on("/setTimers", handle_timers);         // Установка значений таймера срабатывания      
  HTTP.on("/setmaintimer", handle_maintimer);   // Установка значений таймера срабатывания
  HTTP.on("/Timerm", handle_Timerm);            // Обработчик для уменьшения таймера
  HTTP.on("/Timerp", handle_Timerp);            // Обработчик для увеличения таймера

}

// Установка значений предварительного таймера
void handle_Pretimers() {
  int temppre = HTTP.arg("pretimer").toInt();
  savePreTimers(temppre);
  preTimer = temppre * 60000;
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Функция для сохранения таймеров в JSON
void savePreTimers(int preTimerValue) {
  jsonWrite(configSetup, "preTimer", preTimerValue);
  saveConfig();
}
// Установка значений предварительного таймера
void handle_pretimer() {
  preTimer = HTTP.arg("val").toInt() * 60000;
  jsonWrite(configSetup, "preTimer", preTimer / 60000); // сохраняя в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Обработчик уменьшения предтаймера
void handle_preTimerm() {
  preTimer = constrain(preTimer - 1 * 60000, 1 * 60000, 10 * 60000);
  jsonWrite(configSetup, "preTimer", preTimer / 60000); // Сохраняем в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Обработчик увеличения предтаймера
void handle_preTimerp() {
  preTimer = constrain(preTimer + 1 * 60000, 1 * 60000, 10 * 60000);
  jsonWrite(configSetup, "preTimer", preTimer / 60000); // Сохраняем в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}

// Установка значений таймера срабатывания
void handle_timers() {
  int tempint = HTTP.arg("interval").toInt();
  saveTimers(tempint);
  timerDuration = tempint * 60000;
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Функция для сохранения таймеров в JSON
void saveTimers(int timerDurationValue) {
  jsonWrite(configSetup, "Interval", timerDurationValue);
  saveConfig();
}
// Установка значений таймера срабатывания
void handle_maintimer() {
  timerDuration = HTTP.arg("val").toInt() * 60000;
  jsonWrite(configSetup, "Interval", timerDuration / 60000); // сохраняя в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Обработчик уменьшения предтаймера
void handle_Timerm() {
  timerDuration = constrain(timerDuration - 1 * 60000, 1 * 60000, 60 * 60000);
  jsonWrite(configSetup, "Interval", timerDuration / 60000); // Сохраняем в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
// Обработчик увеличения предтаймера
void handle_Timerp() {
  timerDuration = constrain(timerDuration + 1 * 60000, 1 * 60000, 60 * 60000);
  jsonWrite(configSetup, "Interval", timerDuration / 60000); // Сохраняем в минутах
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\": \"true\"}");
}
