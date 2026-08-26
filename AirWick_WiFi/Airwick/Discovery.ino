const uint16_t DISCOVERY_UDP_PORT = 8888;
const uint16_t DISCOVERY_HTTP_PORT = 80;
const size_t DISCOVERY_PACKET_SIZE = 96;

WiFiUDP discoveryUdp;

IPAddress getActiveAirWickIP() {
  if (WiFi.status() == WL_CONNECTED) {
    IPAddress stationIp = WiFi.localIP();
    if (stationIp.toString() != "0.0.0.0") return stationIp;
  }

  WiFiMode_t mode = WiFi.getMode();
  if (mode == WIFI_AP || mode == WIFI_AP_STA) {
    IPAddress accessPointIp = WiFi.softAPIP();
    if (accessPointIp.toString() != "0.0.0.0") return accessPointIp;
  }

  return IPAddress(AP_STATIC_IP[0], AP_STATIC_IP[1], AP_STATIC_IP[2], AP_STATIC_IP[3]);
}

String getAirWickDiscoveryName() {
  String name = jsonRead(configSetup, "SSDP");
  if (!name.length()) name = "AirWick";
  return name;
}

String getAirWickChipId() {
  char chipId[9];
  snprintf(chipId, sizeof(chipId), "%06lX", (unsigned long)ESP.getChipId());
  return String(chipId);
}

void sendDiscoveryInfo() {
  DynamicJsonDocument doc(512);
  WiFiMode_t mode = WiFi.getMode();
  doc["api"] = 1;
  doc["device"] = "AirWick";
  doc["name"] = getAirWickDiscoveryName();
  doc["version"] = AIRWICK_VERSION;
  doc["app_patch"] = "2026.08";
  doc["id"] = getAirWickChipId();
  doc["ip"] = getActiveAirWickIP().toString();
  doc["http_port"] = DISCOVERY_HTTP_PORT;
  doc["udp_port"] = DISCOVERY_UDP_PORT;
  doc["sta_connected"] = WiFi.status() == WL_CONNECTED;
  doc["ap_active"] = mode == WIFI_AP || mode == WIFI_AP_STA;
  doc["wifi_mode"] = mode == WIFI_AP ? "AP" :
                     mode == WIFI_STA ? "Station" : "AP+Station";

  String response;
  serializeJson(doc, response);
  HTTP.send(200, "application/json; charset=utf-8", response);
}

void sendDiscoveryVersion() {
  DynamicJsonDocument doc(384);
  doc["device"] = "AirWick";
  doc["name"] = getAirWickDiscoveryName();
  doc["version"] = AIRWICK_VERSION;
  doc["id"] = getAirWickChipId();
  doc["ip_address"] = getActiveAirWickIP().toString();
  doc["lamp_on"] = workmode;
  doc["effect_count"] = 0;

  String response;
  serializeJson(doc, response);
  HTTP.send(200, "application/json; charset=utf-8", response);
}

void initDiscovery() {
  HTTP.on("/api/v1/info", HTTP_GET, sendDiscoveryInfo);
  HTTP.on("/version", HTTP_GET, sendDiscoveryVersion);

  if (discoveryUdp.begin(DISCOVERY_UDP_PORT)) {
    Serial.print("UDP поиск запущен, порт: ");
    Serial.println(DISCOVERY_UDP_PORT);
  } else {
    Serial.println("Не удалось запустить UDP поиск.");
  }
}

void discoveryLoop() {
  // За один цикл обрабатываем несколько пакетов: приложение посылает запрос
  // повторно и может искать сразу несколько устройств.
  for (uint8_t processed = 0; processed < 3; processed++) {
    int packetSize = discoveryUdp.parsePacket();
    if (packetSize <= 0) return;

    char packet[DISCOVERY_PACKET_SIZE];
    int bytesRead = discoveryUdp.read(packet, sizeof(packet) - 1);
    if (bytesRead <= 0) continue;
    packet[bytesRead] = '\0';

    String command(packet);
    command.trim();
    if (!command.startsWith("DISCOVER")) continue;

    IPAddress activeIp = getActiveAirWickIP();
    if (discoveryUdp.remoteIP() == activeIp) continue;

    String reply = String("IP ") + activeIp.toString() + ':' +
                   String(DISCOVERY_UDP_PORT) + ':' + getAirWickDiscoveryName();
    discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
    discoveryUdp.print(reply);
    discoveryUdp.endPacket();
  }
}
