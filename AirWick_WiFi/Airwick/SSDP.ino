void registerSSDPHandlers() {
  static bool schemaRouteRegistered = false;
  if (schemaRouteRegistered) return;

  HTTP.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(HTTP.client());
  });
  // --------------------Получаем SSDP со страницы
  HTTP.on("/ssdp", HTTP_GET, []() {
    String ssdp = HTTP.arg("ssdp");
    ssdp.trim();
    bool validName = ssdp.length() > 0 && ssdp.length() <= 63;
    for (size_t i = 0; validName && i < ssdp.length(); i++) {
      uint8_t c = (uint8_t)ssdp[i];
      if (c < 0x20 || c == '<' || c == '>' || c == '&' || c == '/' || c == '\\') {
        validName = false;
      }
    }
    if (!validName) {
      LOG.println(F("Имя AirWick отклонено: недопустимые символы"));
      HTTP.send(400, "application/json",
                "{\"ok\":false,\"error\":\"invalid device name\"}");
      return;
    }
    configJson = jsonWrite(configJson, "SSDP", ssdp);
    configSetup = jsonWrite(configSetup, "SSDP", ssdp);
    SSDP.setName(jsonRead(configSetup, "SSDP"));
    saveConfig();                       // Функция сохранения данных во Flash
    LOG.print(F("Имя AirWick сохранено: "));
    LOG.println(ssdp);
    HTTP.send(200, "application/json", "{\"ok\":true}");
  });

  schemaRouteRegistered = true;
}

void SSDP_init(void) {
  registerSSDPHandlers();
  String chipID = String( ESP.getChipId() ) + "-" + String( ESP.getFlashChipId() );
  // SSDP дескриптор
  //Если версия  2.0.0 закаментируйте следующую строчку
  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(jsonRead(configSetup, "SSDP"));
  SSDP.setSerialNumber(chipID);
  SSDP.setURL("/");
  SSDP.setModelName("AirWick");
  SSDP.setModelNumber(jsonRead(configSetup, "SSDP") + " Ver." + AIRWICK_VERSION);
  
  
  SSDP.setModelURL("https://github.com/MishanyaTS/AirWick_WiFi");
  SSDP.setManufacturer("MishanyaTS");
  SSDP.setManufacturerURL("https://github.com/MishanyaTS");
  SSDP.begin();
  LOG.println(F("SSDP запущен"));
}
