#include <time.h>               //Содержится в пакете.  Видео с уроком http://esp8266-arduinoide.ru/step8-timeupdate/

bool isValidTimeZoneText(const String& value) {
  if (!value.length()) return false;
  size_t start = value[0] == '-' ? 1 : 0;
  if (start == value.length()) return false;
  for (size_t i = start; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }
  return true;
}

void Time_init() {
  HTTP.on("/Time", handle_Time);     // Синхронизировать время устройства по запросу вида /Time
  HTTP.on("/timeZone", handle_time_zone);    // Установка времянной зоны по запросу вида http://192.168.0.101/timeZone?timeZone=3
  int zone = jsonReadtoInt(configSetup, "timezone");
  if (zone < -12 || zone > 14) {
    zone = 3;
    jsonWrite(configSetup, "timezone", zone);
    saveConfig();
  }
  timeSynch(zone);
}
void timeSynch(int zone){
  if (WiFi.status() == WL_CONNECTED) {
    // Настройка соединения с NTP сервером
    configTime(zone * 3600, 0, "pool.ntp.org", "ru.pool.ntp.org");
    int i = 0;
    Serial.println("\nОжидание времени");
    while (!time(nullptr) && i < 10) {
      Serial.print(".");
      i++;
      delay(1000);
    }
    Serial.println("");
    Serial.println("Время запущено!");
    Serial.println(GetTime());
  }
}

// Установка параметров времянной зоны по запросу вида http://192.168.0.101/timeZone?timeZone=3
void handle_time_zone() {
  String zoneText = HTTP.arg("timeZone");
  int zone = zoneText.toInt();
  if (!isValidTimeZoneText(zoneText) || zone < -12 || zone > 14) {
    HTTP.send(400, "application/json",
              "{\"ok\":false,\"error\":\"timezone must be -12..14\"}");
    return;
  }
  jsonWrite(configSetup, "timezone", zone);
  saveConfig();
  HTTP.send(200, "application/json", "{\"ok\":true}");
}

void handle_Time(){
  timeSynch(jsonReadtoInt(configSetup, "timezone"));
  HTTP.send(200, "application/json", "{\"ok\":true}");
  }

// Получение текущего времени
String GetTime() {
 time_t now = time(nullptr); // получаем время с помощью библиотеки time.h
 String Time = ""; // Строка для результатов времени
 Time += ctime(&now); // Преобразуем время в строку формата Thu Jan 19 00:55:35 2017
 int i = Time.indexOf(":"); //Ишем позицию первого символа :
 Time = Time.substring(i - 2, i + 6); // Выделяем из строки 2 символа перед символом : и 6 символов после
 return Time; // Возврашаем полученное время
}

// Получение даты
String GetDate() {
 time_t now = time(nullptr); // получаем время с помощью библиотеки time.h
 String Data = ""; // Строка для результатов времени
 Data += ctime(&now); // Преобразуем время в строку формата Thu Jan 19 00:55:35 2017
 int i = Data.lastIndexOf(" "); //Ишем позицию последнего символа пробел
 String Time = Data.substring(i - 8, i+1); // Выделяем время и пробел
 Data.replace(Time, ""); // Удаляем из строки 8 символов времени и пробел
 Data.replace("\n", ""); // Удаляем символ переноса строки
 return Data; // Возврашаем полученную дату
}
