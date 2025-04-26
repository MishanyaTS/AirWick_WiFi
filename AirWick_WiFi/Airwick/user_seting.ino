void User_setings() {
  
HTTP.on("/light", handle_lightTreshold);    // Порог освещения
HTTP.on("/lightm", handle_lightTresholdm);  // Пошаговый порог освещения минус
HTTP.on("/lightp", handle_lightTresholdp);  // Пошаговый порог освещения плюс
HTTP.on("/set_ip", handle_set_static_ip);   // Установки статичного IP адреса
}
// Порог освещения
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

// Пошаговый порог освещения плюс
void handle_lightTresholdp() {
  lightTreshold = constrain(lightTreshold + 10, 10, 1024);
  jsonWrite(configSetup, "light", lightTreshold);
  HTTP.send(200, ("application/json"), ("{\"should_refresh\": \"true\"}"));
}

void init_ip(){
  String configIP = readFile("config_ip.json", 512);
  Serial.println(configIP);
  use_static_ip = jsonReadtoInt(configIP, "ip_on");
  Static_IP.fromString(jsonRead(configIP, "ip"));
  Gateway.fromString(jsonRead(configIP, "gateway"));
  Subnet.fromString(jsonRead(configIP, "subnet"));
  DNS1.fromString(jsonRead(configIP, "dns"));
}

void handle_set_static_ip ()   {
    String configIP = readFile("config_ip.json", 2048);
    use_static_ip = HTTP.arg("ip_on").toInt();
    jsonWrite(configIP, "ip_on", use_static_ip);
    jsonWrite(configIP, "ip", HTTP.arg("ip1"));
    jsonWrite(configIP, "gateway", HTTP.arg("gateway"));
    jsonWrite(configIP, "subnet", HTTP.arg("subnet"));
    jsonWrite(configIP, "dns", HTTP.arg("dns"));
    writeFile("config_ip.json", configIP );
    HTTP.send(200, ("application/json"), ("{\"should_refresh\": \"true\"}"));
}

bool FileCopy (const String& SourceFile , const String& TargetFile)   {
    File S_File = LittleFS.open( SourceFile, "r");
    File T_File = LittleFS.open( TargetFile, "w");
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
//https://arduino.esp8266.com/stable/package_esp8266com_index.json
