void User_setings () {
  
HTTP.on("/light", handle_lightTreshold);    // Порог освещения
HTTP.on("/lightm", handle_lightTresholdm);  // Пошаговый порог освещения минус
HTTP.on("/lightp", handle_lightTresholdp);  // Порог освещения освещения плюс
HTTP.on("/lowpwr", handle_lowpower);        // Установка значения энергосберегающего режима
HTTP.on("/tm", handle_tm);                  // Смена темы страници (0 - светлая / 1 - тёмная)

}
// Порог освещения  http://192.168.0.101/light?light=1
void handle_lightTreshold() {
  jsonWrite(configSetup, "light", HTTP.arg("light").toInt()); // Получаем значение порога освещения из запроса конвертируем в int сохраняем
  lightTreshold = jsonReadtoInt(configSetup, "light");
  saveConfig();
  HTTP.send(200, ("application/json"), ("{\"should_refresh\": \"true\"}"));
}

// Пошаговый порог освещения минус
void handle_lightTresholdm() {
  lightTreshold = constrain(lightTreshold - 10, 10, 1024);
  jsonWrite(configSetup, "light", lightTreshold);
  HTTP.send(200, ("application/json"), ("{\"should_refresh\": \"true\"}"));
}

// Порог освещения освещения плюс
void handle_lightTresholdp() {
  lightTreshold = constrain(lightTreshold + 10, 10, 1024);
  jsonWrite(configSetup, "light", lightTreshold);
  HTTP.send(200, ("application/json"), ("{\"should_refresh\": \"true\"}"));
}

// Установка значения энергосберегающего режима  
void handle_lowpower() {
  lowPower=HTTP.arg("onoff").toInt();// включение и отключение режима
  jsonWrite(configSetup, "lowPWR", lowPower); // сохраняем в json
  saveConfig();
  if (client.connected()){
  }
  HTTP.send(200, "text/plain", "OK");
} 

// Смена темы страницы
void handle_tm ()   {
  bool flg = false;
  jsonWrite(configSetup, "tm", HTTP.arg("tm").toInt());
  if (jsonReadtoInt(configSetup, "tm")) flg = FileCopy (("/css/dark/build.css.gz") , ("/css/build.css.gz"));
  else flg = FileCopy (("/css/light/build.css.gz") , ("/css/build.css.gz"));
  if (flg) {
       HTTP.send(200, ("text/plain"), ("OK"));
     saveConfig();
    }
  else HTTP.send(404, ("text/plain"), "File not found");  
}

bool FileCopy (const String& SourceFile , const String& TargetFile)   {
    File S_File = SPIFFS.open( SourceFile, "r");
    File T_File = SPIFFS.open( TargetFile, "w");
    if (!S_File || !T_File) 
  return false;
    size_t size = S_File.size();
    for (unsigned int i=0; i<size; i++)  {
        T_File.write(S_File.read ());
        ESP.wdtFeed();
        yield();
    }
    S_File.close();
    T_File.close();
    return true;
}
