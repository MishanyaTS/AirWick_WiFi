void User_setings() {
  
HTTP.on("/light", handle_lightTreshold);    // Порог освещения
HTTP.on("/lightm", handle_lightTresholdm);  // Пошаговый порог освещения минус
HTTP.on("/lightp", handle_lightTresholdp);  // Пошаговый порог освещения плюс
HTTP.on("/s_IP", handle_use_static_ip);     // Включение статического IP адреса
HTTP.on("/set_ip", handle_set_static_ip);   // Установки статичного IP адреса
}
// Порог освещения
void handle_lightTreshold() {
  int requestedTreshold = HTTP.arg("light").toInt();
  if (requestedTreshold < 10 || requestedTreshold > 1023) {
    HTTP.send(400, "application/json", "{\"ok\":false,\"error\":\"light must be 10..1023\"}");
    return;
  }
  lightTreshold = (uint16_t)requestedTreshold;
  jsonWrite(configSetup, "light", lightTreshold);
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

// Пошаговый порог освещения минус
void handle_lightTresholdm() {
  int nextTreshold = (int)lightTreshold - 10;
  if (nextTreshold < 10) nextTreshold = 10;
  lightTreshold = (uint16_t)nextTreshold;
  jsonWrite(configSetup, "light", lightTreshold);
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

// Пошаговый порог освещения плюс
void handle_lightTresholdp() {
  int nextTreshold = (int)lightTreshold + 10;
  if (nextTreshold > 1023) nextTreshold = 1023;
  lightTreshold = (uint16_t)nextTreshold;
  jsonWrite(configSetup, "light", lightTreshold);
  saveConfig();
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

bool isNonZeroIp(const IPAddress& address) {
  return address[0] || address[1] || address[2] || address[3];
}

bool isValidSubnetMask(const IPAddress& address) {
  uint32_t mask = ((uint32_t)address[0] << 24) |
                  ((uint32_t)address[1] << 16) |
                  ((uint32_t)address[2] << 8) |
                  (uint32_t)address[3];
  if (mask == 0) return false;
  uint32_t inverse = ~mask;
  return (inverse & (inverse + 1U)) == 0;
}

void init_ip(){
  String configIP = readFile("config_ip.json", 512);
  if (configIP == "Failed" || configIP == "Large") configIP = "{}";
  use_static_ip = jsonReadtoInt(configSetup, "s_IP") ? 1 : 0;
  bool ipOk = Static_IP.fromString(jsonRead(configIP, "ip"));
  bool gatewayOk = Gateway.fromString(jsonRead(configIP, "gateway"));
  bool subnetOk = Subnet.fromString(jsonRead(configIP, "subnet"));
  bool dnsOk = DNS1.fromString(jsonRead(configIP, "dns"));
  staticIpConfigValid = ipOk && gatewayOk && subnetOk && dnsOk &&
                        isNonZeroIp(Static_IP) && isNonZeroIp(Gateway) &&
                        isNonZeroIp(DNS1) && isValidSubnetMask(Subnet);
}

void handle_use_static_ip() {
  uint8_t requestedMode = HTTP.arg("s_IP").toInt() ? 1 : 0;
  init_ip();
  if (requestedMode && !staticIpConfigValid) {
    HTTP.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid static IP configuration\"}");
    return;
  }
  use_static_ip = requestedMode;
  jsonWrite(configSetup, "s_IP", use_static_ip);
  saveConfig();
  HTTP.send(200, "application/json", "{\"ok\":true,\"should_refresh\":true}");
}

void handle_set_static_ip ()   {
    IPAddress requestedIp;
    IPAddress requestedGateway;
    IPAddress requestedSubnet;
    IPAddress requestedDns;
    bool valid = requestedIp.fromString(HTTP.arg("ip1")) &&
                 requestedGateway.fromString(HTTP.arg("gateway")) &&
                 requestedSubnet.fromString(HTTP.arg("subnet")) &&
                 requestedDns.fromString(HTTP.arg("dns")) &&
                 isNonZeroIp(requestedIp) && isNonZeroIp(requestedGateway) &&
                 isNonZeroIp(requestedDns) && isValidSubnetMask(requestedSubnet);
    if (!valid) {
      HTTP.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid IPv4 settings\"}");
      return;
    }

    String configIP = readFile("config_ip.json", 2048);
    if (configIP == "Failed" || configIP == "Large") configIP = "{}";
    // Совместимость со старой страницей AirWick.
    if (HTTP.hasArg("ip_on")) {
      use_static_ip = HTTP.arg("ip_on").toInt() ? 1 : 0;
      jsonWrite(configSetup, "s_IP", use_static_ip);
      saveConfig();
    }
    jsonWrite(configIP, "ip", requestedIp.toString());
    jsonWrite(configIP, "gateway", requestedGateway.toString());
    jsonWrite(configIP, "subnet", requestedSubnet.toString());
    jsonWrite(configIP, "dns", requestedDns.toString());
    writeFile("config_ip.json", configIP );
    init_ip();
    HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

bool FileCopy (const String& SourceFile, const String& TargetFile) {
    File S_File = LittleFS.open(SourceFile, "r");
    File T_File = LittleFS.open(TargetFile, "w");
    if (!S_File || !T_File)
      return false;
    size_t size = S_File.size();
    for (unsigned int i = 0; i < size; i++) {
        T_File.write(S_File.read());
        ESP.wdtFeed();
        yield();
    }
    S_File.close();
    T_File.close();
    return true;
}

//https://arduino.esp8266.com/stable/package_esp8266com_index.json
