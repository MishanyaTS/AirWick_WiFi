void SSDP_init(void) {
  String chipID = String( ESP.getChipId() ) + "-" + String( ESP.getFlashChipId() );
  // SSDP дескриптор
  HTTP.on("/description.xml", HTTP_GET, []() {
    SSDP.schema(HTTP.client());
  });
   // --------------------Получаем SSDP со страницы
  HTTP.on("/ssdp", HTTP_GET, []() {
    String ssdp = HTTP.arg("ssdp");
  configJson=jsonWrite(configJson, "SSDP", ssdp);
  configJson=jsonWrite(configSetup, "SSDP", ssdp);
  SSDP.setName(jsonRead(configSetup, "SSDP"));
  saveConfig();                 // Функция сохранения данных во Flash
  HTTP.send(200, "text/plain", "OK"); // отправляем ответ о выполнении
  });
  //Если версия  2.0.0 закаментируйте следующую строчку
  SSDP.setDeviceType("upnp:rootdevice");
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName(jsonRead(configSetup, "SSDP"));
  SSDP.setSerialNumber(chipID);
  SSDP.setURL("/");
  SSDP.setModelName("AirWick");
  SSDP.setModelNumber(jsonRead(configSetup, "SSDP") + FLL_VERSION);
  
  
  SSDP.setModelURL("https://github.com/MishanyaTS/");
  SSDP.setManufacturer("MishanyaTS");
  SSDP.setManufacturerURL("https://github.com/MishanyaTS");
  SSDP.begin();
}
