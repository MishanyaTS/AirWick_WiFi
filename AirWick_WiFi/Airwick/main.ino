// ------------- Чтение значения json
String jsonRead(String &json, String name) {
  DynamicJsonDocument jsonDoc(1024); // Увеличьте размер, если требуется
  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) {
    return "";
  }
  return jsonDoc[name].as<String>();
}

// ------------- Чтение значения json
int jsonReadtoInt(String &json, String name) {
  DynamicJsonDocument jsonDoc(1024); // Увеличьте размер, если требуется
  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) {
    return 0;
  }
  return jsonDoc[name].as<int>();
}

// ------------- Запись значения json String
String jsonWrite(String &json, String name, String volume) {
  DynamicJsonDocument jsonDoc(1024); // Увеличьте размер, если требуется
  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) {
    return "";
  }
  jsonDoc[name] = volume;
  json = "";
  serializeJson(jsonDoc, json);
  return json;
}

// ------------- Запись значения json int
String jsonWrite(String &json, String name, int volume) {
  DynamicJsonDocument jsonDoc(1024); // Увеличьте размер, если требуется
  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) {
    return "";
  }
  jsonDoc[name] = volume;
  json = "";
  serializeJson(jsonDoc, json);
  return json;
}

void saveConfig (){
  writeFile("config.json", configSetup );
}

// ------------- Чтение файла в строку
String readFile(String fileName, size_t len ) {
  File configFile = SPIFFS.open("/" + fileName, "r");
  if (!configFile) {
    return "Failed";
  }
  size_t size = configFile.size();
  if (size > len) {
    configFile.close();
    return "Large";
  }
  String temp = configFile.readString();
  configFile.close();
  return temp;
}

// ------------- Запись строки в файл
String writeFile(String fileName, String strings ) {
  File configFile = SPIFFS.open("/" + fileName, "w");
  if (!configFile) {
    return "Failed to open config file";
  }
  configFile.print(strings);
  //strings.printTo(configFile);
  configFile.close();
  return "Write sucsses";
}

// Перегрузка функций
// --------------Создание данных для графика
String graf(int datas, int points, int refresh) {
  DynamicJsonDocument jsonDoc(1024); // Увеличьте размер, если требуется
  JsonObject json = jsonDoc.to<JsonObject>();

  // Заполняем поля json
  JsonArray data = json.createNestedArray("data");
  data.add(datas);
  json["points"] = points;
  json["refresh"] = refresh;

  String root;
  serializeJson(json, root);
  return root;
}
