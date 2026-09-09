const uint16_t DISCOVERY_UDP_PORT = 8888;
const uint16_t DISCOVERY_HTTP_PORT = 80;
const size_t DISCOVERY_PACKET_SIZE = 96;

WiFiUDP discoveryUdp;
bool discoveryUdpStarted = false;
uint32_t lastDiscoveryStartAttemptMs = 0;

void stopDiscoveryUdp() {
  discoveryUdp.stop();
  discoveryUdpStarted = false;
}

IPAddress getActiveAirWickIP() {
  if (stationHasValidConnection()) {
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
  notePowerSavingWebActivity();
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
  doc["sta_connected"] = stationHasValidConnection();
  doc["ap_active"] = mode == WIFI_AP || mode == WIFI_AP_STA;
  doc["wifi_mode"] = mode == WIFI_AP ? "AP" :
                     mode == WIFI_STA ? "Station" : "AP+Station";
  doc["power_mode"] = powerSavingMode;
  doc["light_sleep_in"] = compatiblePowerSecondsUntilSleep();

  String response;
  serializeJson(doc, response);
  HTTP.send(200, "application/json; charset=utf-8", response);
}

void sendDiscoveryVersion() {
  notePowerSavingWebActivity();
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

bool startDiscoveryUdp(bool restarted) {
  lastDiscoveryStartAttemptMs = millis();
  discoveryUdp.stop();
  if (discoveryUdp.begin(DISCOVERY_UDP_PORT)) {
    discoveryUdpStarted = true;
    LOG.print(restarted ? F("UDP поиск AirWick перезапущен, порт: ")
                        : F("UDP поиск AirWick запущен, порт: "));
    LOG.println(DISCOVERY_UDP_PORT);
    return true;
  } else {
    discoveryUdpStarted = false;
    LOG.println(F("Не удалось запустить UDP поиск AirWick"));
    return false;
  }
}

void restartDiscoveryUdp() {
  startDiscoveryUdp(true);
}

void initDiscovery() {
  HTTP.on("/api/v1/info", HTTP_GET, sendDiscoveryInfo);
  HTTP.on("/version", HTTP_GET, sendDiscoveryVersion);
  startDiscoveryUdp(false);
}

void discoveryLoop() {
  if (!discoveryUdpStarted) {
    WiFiMode_t mode = WiFi.getMode();
    bool networkInterfaceActive = stationHasValidConnection() ||
                                  mode == WIFI_AP || mode == WIFI_AP_STA;
    if (networkInterfaceActive &&
        millis() - lastDiscoveryStartAttemptMs >= WIFI_ROUTER_RETRY_INTERVAL) {
      startDiscoveryUdp(true);
    }
    return;
  }

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
    notePowerSavingWebActivity();

    IPAddress activeIp = getActiveAirWickIP();
    if (discoveryUdp.remoteIP() == activeIp) continue;

    String reply = String("IP ") + activeIp.toString() + ':' +
                   String(DISCOVERY_UDP_PORT) + ':' + getAirWickDiscoveryName();
    discoveryUdp.beginPacket(discoveryUdp.remoteIP(), discoveryUdp.remotePort());
    discoveryUdp.print(reply);
    discoveryUdp.endPacket();
  }
}
