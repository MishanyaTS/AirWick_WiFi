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

// ------------- Проверка наличия ключа json (в том числе с пустым значением)
bool jsonHasKey(String &json, const char* name) {
  DynamicJsonDocument jsonDoc(1024);
  DeserializationError error = deserializeJson(jsonDoc, json);
  if (error) return false;
  return jsonDoc.containsKey(name);
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
  if (writeFile("config.json", configSetup) != "Write sucsses") {
    LOG.println(F("Ошибка сохранения config.json"));
  }
}

// ------------- Чтение файла в строку
String readFile(String fileName, size_t len ) {
  File configFile = LittleFS.open("/" + fileName, "r");
  if (!configFile) {
    LOG.print(F("Не удалось открыть файл: /"));
    LOG.println(fileName);
    return "Failed";
  }
  size_t size = configFile.size();
  if (size > len) {
    configFile.close();
    LOG.print(F("Файл превышает допустимый размер: /"));
    LOG.println(fileName);
    return "Large";
  }
  String temp = configFile.readString();
  configFile.close();
  return temp;
}

// ------------- Запись строки в файл
String writeFile(String fileName, String strings ) {
  File configFile = LittleFS.open("/" + fileName, "w");
  if (!configFile) {
    LOG.print(F("Не удалось открыть файл для записи: /"));
    LOG.println(fileName);
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
