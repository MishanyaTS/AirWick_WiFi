bool isDecimalString(const String& value) {
  if (!value.length()) return false;
  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }
  return true;
}

bool isValidMqttHost(const String& host) {
  if (!host.length() || host.length() > 63) return false;
  for (size_t i = 0; i < host.length(); i++) {
    char c = host[i];
    bool alphaNumeric = (c >= '0' && c <= '9') ||
                        (c >= 'A' && c <= 'Z') ||
                        (c >= 'a' && c <= 'z');
    if (!alphaNumeric && c != '.' && c != '-' && c != '_') return false;
  }
  return true;
}

String normalizeMqttTopicBase(String topic) {
  topic.trim();
  while (topic.startsWith("/")) topic.remove(0, 1);
  while (topic.endsWith("/")) topic.remove(topic.length() - 1);
  return topic;
}

bool isValidMqttTopicBase(const String& topic) {
  if (!topic.length() || topic.length() > 48) return false;
  if (topic.indexOf('#') >= 0 || topic.indexOf('+') >= 0) return false;
  for (size_t i = 0; i < topic.length(); i++) {
    if ((uint8_t)topic[i] < 0x20) return false;
  }
  return true;
}

const char* mqttStateLabel(int8_t state) {
  switch (state) {
    case -4: return "timeout";
    case -3: return "connection lost";
    case -2: return "connect failed";
    case -1: return "disconnected";
    case 0: return "connected";
    case 1: return "bad protocol";
    case 2: return "bad client id";
    case 3: return "server unavailable";
    case 4: return "bad credentials";
    case 5: return "unauthorized";
    default: return "unknown";
  }
}

void buildMqttTopics() {
  mqttTopicBase = normalizeMqttTopicBase(mqttTopicBase);
  if (!mqttTopicBase.length()) mqttTopicBase = "AirWick";

  char chipId[9];
  snprintf(chipId, sizeof(chipId), "%06lX", (unsigned long)ESP.getChipId());
  mqttClientID = String("AirWick_") + chipId;
  mqttCommandTopic = mqttTopicBase + '/' + mqttClientID + "/cmnd";
  mqttStateTopic = mqttTopicBase + '/' + mqttClientID + "/snd";
  mqttEventTopic = mqttTopicBase + '/' + mqttClientID + "/event";
  mqttAvailabilityTopic = mqttTopicBase + '/' + mqttClientID + "/status";
}

void loadMqttConfig() {
  mqttconfigJson = readFile("config_mqtt.json", 2048);
  if (mqttconfigJson == "Failed" || mqttconfigJson == "Large") mqttconfigJson = "{}";
  bool changed = false;

  mqttServer = jsonRead(mqttconfigJson, "mq_ip");
  mqttServer.trim();
  if (jsonRead(mqttconfigJson, "mq_ip") != mqttServer) {
    jsonWrite(mqttconfigJson, "mq_ip", mqttServer);
    changed = true;
  }

  String portValue = jsonRead(mqttconfigJson, "mq_port");
  int configuredPort = portValue.toInt();
  if (!isDecimalString(portValue) || configuredPort < 1 || configuredPort > 65535) {
    configuredPort = 1883;
    jsonWrite(mqttconfigJson, "mq_port", configuredPort);
    changed = true;
  }
  mqttPort = (uint16_t)configuredPort;

  bool mqttUserKeyExists = jsonHasKey(mqttconfigJson, "mq_user");
  mqttUser = jsonRead(mqttconfigJson, "mq_user");
  if (!mqttUserKeyExists) {
    mqttUser = jsonRead(mqttconfigJson, "mq_ssid");
    jsonWrite(mqttconfigJson, "mq_user", mqttUser);
    changed = true;
  }
  mqttPassword = jsonRead(mqttconfigJson, "mq_pass");
  useMQTT = jsonReadtoInt(mqttconfigJson, "mq_on") != 0;

  mqttTopicBase = normalizeMqttTopicBase(jsonRead(mqttconfigJson, "topic"));
  if (!mqttTopicBase.length()) {
    mqttTopicBase = "AirWick";
    changed = true;
  }

  String periodValue = jsonRead(mqttconfigJson, "mq_prd");
  int configuredPeriod = periodValue.toInt();
  if (!isDecimalString(periodValue) || configuredPeriod < 0 || configuredPeriod > 60) {
    if (configuredPeriod < 0) configuredPeriod = 0;
    if (configuredPeriod > 60) configuredPeriod = 60;
    if (!isDecimalString(periodValue)) configuredPeriod = 0;
    jsonWrite(mqttconfigJson, "mq_prd", configuredPeriod);
    changed = true;
  }
  mqttPeriod = (uint8_t)configuredPeriod;

  buildMqttTopics();
  if (jsonRead(mqttconfigJson, "topic") != mqttTopicBase) {
    jsonWrite(mqttconfigJson, "topic", mqttTopicBase);
    changed = true;
  }

  String topicS = mqttClientID + "/cmnd";
  String topicP = mqttClientID + "/snd";
  if (jsonRead(mqttconfigJson, "TopicS") != topicS) {
    jsonWrite(mqttconfigJson, "TopicS", topicS);
    changed = true;
  }
  if (jsonRead(mqttconfigJson, "TopicP") != topicP) {
    jsonWrite(mqttconfigJson, "TopicP", topicP);
    changed = true;
  }
  mqttConfigValid = isValidMqttHost(mqttServer) &&
                    mqttUser.length() <= 63 &&
                    mqttPassword.length() <= 63 &&
                    isValidMqttTopicBase(mqttTopicBase);

  if (changed) writeFile("config_mqtt.json", mqttconfigJson);

  client.setServer(mqttServer.c_str(), mqttPort);
  client.setCallback(mqttCallback);
  client.setSocketTimeout(2);
}

void resetMqttConnection() {
  if (client.connected()) {
    client.publish(mqttAvailabilityTopic.c_str(), "offline", true);
    client.disconnect();
    LOG.println(F("MQTT отключён"));
  }
  mqttLastConnectingAttempt = 0;
  mqttNeedToPublish = true;
}

void init_mqtt() {
  HTTP.on("/mqtt_set", handle_mqtt_set);
  HTTP.on("/mqtt_on", handle_mqtt_on);
  HTTP.on("/mqtt_prd", handle_mqtt_period);
  HTTP.on("/mqtt_status", HTTP_GET, handle_mqtt_status);
  HTTP.on("/set_mqtt", handle_set_mqtt); // Совместимость со старой страницей.
  loadMqttConfig();
  resetMqttConnection();
  LOG.print(F("MQTT: "));
  if (!useMQTT) {
    LOG.println(F("выключен"));
  } else if (!mqttConfigValid) {
    LOG.println(F("включён, но настройки неверны"));
  } else {
    LOG.print(F("включён, брокер "));
    LOG.print(mqttServer);
    LOG.print(':');
    LOG.print(mqttPort);
    LOG.print(F(", топик команд "));
    LOG.println(mqttCommandTopic);
  }
}

bool saveMqttConnectionSettings(String& errorMessage) {
  String config = readFile("config_mqtt.json", 2048);
  if (config == "Failed" || config == "Large") config = "{}";

  String server = HTTP.hasArg("mq_ip") ? HTTP.arg("mq_ip") : jsonRead(config, "mq_ip");
  server.trim();
  String portText = HTTP.hasArg("mq_port") ? HTTP.arg("mq_port") : jsonRead(config, "mq_port");
  String user = HTTP.hasArg("mq_user") ? HTTP.arg("mq_user") : jsonRead(config, "mq_user");
  if (!HTTP.hasArg("mq_user") && HTTP.hasArg("mq_ssid")) user = HTTP.arg("mq_ssid");
  String password = HTTP.hasArg("mq_pass") ? HTTP.arg("mq_pass") : jsonRead(config, "mq_pass");
  String topic = HTTP.hasArg("topic") ?
                 normalizeMqttTopicBase(HTTP.arg("topic")) :
                 normalizeMqttTopicBase(jsonRead(config, "topic"));
  int port = portText.toInt();

  if (!isValidMqttHost(server)) {
    errorMessage = "invalid MQTT host";
    return false;
  }
  if (!isDecimalString(portText) || port < 1 || port > 65535) {
    errorMessage = "invalid MQTT port";
    return false;
  }
  if (user.length() > 63 || password.length() > 63) {
    errorMessage = "MQTT login is too long";
    return false;
  }
  if (!isValidMqttTopicBase(topic)) {
    errorMessage = "invalid MQTT topic";
    return false;
  }

  jsonWrite(config, "mq_ip", server);
  jsonWrite(config, "mq_port", port);
  jsonWrite(config, "mq_user", user);
  jsonWrite(config, "mq_pass", password);
  jsonWrite(config, "topic", topic);
  if (HTTP.hasArg("mq_on")) jsonWrite(config, "mq_on", HTTP.arg("mq_on").toInt() ? 1 : 0);

  resetMqttConnection();
  if (writeFile("config_mqtt.json", config) != "Write sucsses") {
    errorMessage = "cannot save MQTT settings";
    LOG.println(F("Не удалось сохранить настройки MQTT"));
    return false;
  }
  loadMqttConfig();
  mqttNeedToPublish = true;
  LOG.print(F("Настройки MQTT сохранены: брокер "));
  LOG.print(mqttServer);
  LOG.print(':');
  LOG.print(mqttPort);
  LOG.print(F(", базовый топик "));
  LOG.println(mqttTopicBase);
  return true;
}

void sendMqttSettingsResult(bool saved, const String& errorMessage) {
  if (!saved) {
    LOG.print(F("Настройки MQTT отклонены: "));
    LOG.println(errorMessage);
    String response = String("{\"ok\":false,\"error\":\"") + errorMessage + "\"}";
    HTTP.send(400, "application/json", response);
    return;
  }
  HTTP.send(200, "application/json", "{\"ok\":true,\"should_refresh\":true}");
}

void handle_mqtt_set() {
  String errorMessage;
  sendMqttSettingsResult(saveMqttConnectionSettings(errorMessage), errorMessage);
}

void handle_set_mqtt() {
  String errorMessage;
  sendMqttSettingsResult(saveMqttConnectionSettings(errorMessage), errorMessage);
}

void handle_mqtt_on() {
  uint8_t requestedMode = HTTP.arg("mq_on").toInt() ? 1 : 0;
  if (requestedMode && !mqttConfigValid) {
    LOG.println(F("MQTT не включён: сначала настройте брокер"));
    HTTP.send(400, "application/json",
              "{\"ok\":false,\"error\":\"configure MQTT broker first\"}");
    return;
  }

  String config = readFile("config_mqtt.json", 2048);
  if (config == "Failed" || config == "Large") config = "{}";
  jsonWrite(config, "mq_on", requestedMode);
  resetMqttConnection();
  writeFile("config_mqtt.json", config);
  loadMqttConfig();
  LOG.print(F("MQTT: "));
  LOG.println(requestedMode ? F("включён") : F("выключен"));
  HTTP.send(200, "application/json", "{\"ok\":true,\"should_refresh\":true}");
}

void handle_mqtt_period() {
  String periodText = HTTP.arg("mq_prd");
  if (!isDecimalString(periodText)) {
    HTTP.send(400, "application/json",
              "{\"ok\":false,\"error\":\"period must be 0..60\"}");
    return;
  }
  int period = periodText.toInt();
  if (period < 0 || period > 60) {
    HTTP.send(400, "application/json",
              "{\"ok\":false,\"error\":\"period must be 0..60\"}");
    return;
  }

  String config = readFile("config_mqtt.json", 2048);
  if (config == "Failed" || config == "Large") config = "{}";
  jsonWrite(config, "mq_prd", period);
  writeFile("config_mqtt.json", config);
  mqttPeriod = (uint8_t)period;
  LOG.print(F("Период публикации MQTT сохранён: "));
  LOG.print(mqttPeriod);
  LOG.println(F(" с"));
  HTTP.send(200, "application/json", "{\"ok\":true,\"should_refresh\":true}");
}

void handle_mqtt_status() {
  DynamicJsonDocument status(768);
  String statusText;
  if (!useMQTT) {
    statusText = "Отключено";
  } else if (!mqttConfigValid) {
    statusText = "Ошибка настроек";
  } else if (client.connected()) {
    statusText = "Подключено";
  } else {
    statusText = "Нет соединения";
  }

  status["mqtt_status"] = statusText;
  status["mqtt_enabled"] = useMQTT;
  status["mqtt_config_valid"] = mqttConfigValid;
  status["mqtt_connected"] = client.connected();
  status["mqtt_error"] = client.state();
  status["mqtt_error_text"] = mqttStateLabel(client.state());
  status["mqtt_client_id"] = mqttClientID;
  status["mqtt_command_topic"] = mqttCommandTopic;
  status["mqtt_state_topic"] = mqttStateTopic;
  status["mqtt_period"] = mqttPeriod;

  String response;
  serializeJson(status, response);
  HTTP.send(200, "application/json; charset=utf-8", response);
}

void connectToMqtt() {
  if (!useMQTT || !mqttConfigValid || espMode != 1 ||
      WiFi.status() != WL_CONNECTED) return;
  if (mqttLastConnectingAttempt &&
      millis() - mqttLastConnectingAttempt < MQTT_RECONNECT_INTERVAL) return;

  mqttLastConnectingAttempt = millis();
  LOG.print(F("Подключение к MQTT брокеру "));
  LOG.print(mqttServer);
  LOG.print(':');
  LOG.print(mqttPort);
  LOG.print(F("..."));

  bool connected;
  if (mqttUser.length()) {
    connected = client.connect(mqttClientID.c_str(), mqttUser.c_str(),
                               mqttPassword.c_str(), mqttAvailabilityTopic.c_str(),
                               1, true, "offline");
  } else {
    connected = client.connect(mqttClientID.c_str(), mqttAvailabilityTopic.c_str(),
                               1, true, "offline");
  }

  if (connected) {
    LOG.println(F(" подключено"));
    client.publish(mqttAvailabilityTopic.c_str(), "online", true);
    client.subscribe(mqttCommandTopic.c_str(), 1);
    client.subscribe("motor", 1); // Совместимость со старыми настройками.
    mqttLastConnectingAttempt = 0;
    mqttNeedToPublish = true;
  } else {
    LOG.print(F(" ошибка, код "));
    LOG.print(client.state());
    LOG.print(F(" ("));
    LOG.print(mqttStateLabel(client.state()));
    LOG.println(')');
  }
}

bool publishMqttSprayEvent() {
  if (!client.connected() || !mqttSprayEventPending) return false;
  StaticJsonDocument<192> event;
  event["event"] = "spray";
  event["source"] = mqttPendingSpraySource;
  event["light"] = lightLevel;
  event["work"] = workmode ? "ON" : "OFF";
  String payload;
  serializeJson(event, payload);
  bool published = client.publish(mqttEventTopic.c_str(), payload.c_str(), false);
  mqttSprayEventPending = false;
  if (!published) LOG.println(F("Ошибка публикации события распыления MQTT"));
  return published;
}

bool publishMqttState() {
  if (!client.connected()) return false;
  StaticJsonDocument<320> state;
  state["online"] = true;
  state["light"] = lightLevel;
  state["threshold"] = lightTreshold;
  state["work"] = workmode ? "ON" : "OFF";
  state["pretimer"] = preTimer / 60000UL;
  state["interval"] = timerDuration / 60000UL;
  state["ip"] = WiFi.localIP().toString();
  String payload;
  serializeJson(state, payload);
  if (!client.publish(mqttStateTopic.c_str(), payload.c_str(), true)) {
    LOG.println(F("Ошибка публикации состояния MQTT"));
    return false;
  }
  mqttNeedToPublish = false;
  mqttPublishTimer = millis();
  return true;
}

void mqttLoop() {
  if (!useMQTT || espMode != 1 || WiFi.status() != WL_CONNECTED) {
    if (!useMQTT) mqttSprayEventPending = false;
    if (client.connected()) {
      client.publish(mqttAvailabilityTopic.c_str(), "offline", true);
      client.disconnect();
    }
    return;
  }

  if (client.connected()) client.loop();
  if (!client.connected()) {
    connectToMqtt();
    return;
  }

  if (mqttSprayEventPending) publishMqttSprayEvent();

  bool periodElapsed = mqttPeriod &&
      millis() - mqttPublishTimer >= (uint32_t)mqttPeriod * 1000UL;
  if (mqttNeedToPublish || periodElapsed) publishMqttState();
}

void mqttCallback(char* receivedTopic, byte* payload, unsigned int length) {
  if (payload == nullptr || length == 0 || length > 64) {
    LOG.println(F("Получена пустая или слишком длинная команда MQTT"));
    return;
  }
  char commandBuffer[65];
  memcpy(commandBuffer, payload, length);
  commandBuffer[length] = '\0';
  String command(commandBuffer);
  command.trim();
  command.toUpperCase();

  LOG.print(F("Получен MQTT, топик: "));
  LOG.print(receivedTopic);
  LOG.print(F(", команда: "));
  LOG.println(command);

  if (command == "SPRAY" || command == "ON" || command == "1" ||
      command == "TRUE" || command == "P_ON") {
    activateSprayer(F("Распыление по MQTT!"), "mqtt");
  } else if (command == "STATE") {
    mqttNeedToPublish = true;
  } else {
    LOG.println(F("Неизвестная команда MQTT проигнорирована"));
  }
}
