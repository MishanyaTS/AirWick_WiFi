void init_mqtt(){
  
HTTP.on("/set_mqtt", handle_set_mqtt);   // Установки MQTT

    mqttconfigJson=readFile("config_mqtt.json", 4096);
    Serial.println(mqttconfigJson);
    mqttServer = jsonRead(mqttconfigJson, "mq_ip");
    mqttPort = jsonReadtoInt(mqttconfigJson, "mq_port");
    mqttUser = jsonRead(mqttconfigJson, "mq_ssid");
    mqttPassword = jsonRead(mqttconfigJson, "mq_pass");
    useMQTT = jsonReadtoInt(mqttconfigJson, "mq_on");
    client.setServer(mqttServer.c_str(), mqttPort);
    client.setCallback(mqttCallback);
  }

void handle_set_mqtt ()   {
  String mqttconfigJson = readFile("config_mqtt.json", 2048);
  useMQTT = HTTP.arg("mq_on").toInt();
  jsonWrite(mqttconfigJson, "mq_on", useMQTT);
  jsonWrite(mqttconfigJson, "mq_ip", HTTP.arg("mq_ip"));
  jsonWrite(mqttconfigJson, "mq_port", HTTP.arg("mq_port"));
  jsonWrite(mqttconfigJson, "mq_ssid", HTTP.arg("mq_ssid"));
  jsonWrite(mqttconfigJson, "mq_pass", HTTP.arg("mq_pass"));
  writeFile("config_mqtt.json", mqttconfigJson );
  client.setServer(mqttServer.c_str(), mqttPort);
  connectToMqtt();
  HTTP.send(200, "text/plain", "OK");
}

void connectToMqtt() {
  int attempts = 0;  // счетчик попыток подключения
  while (!client.connected() && attempts < 2 && !connectionCancelled) {
    Serial.print("Подключение к MQTT...");
    if (client.connect((clientID+ESP.getChipId()).c_str(), mqttUser.c_str(), mqttPassword.c_str())) {
      Serial.println("подключено");

      // Подписка на топик
      client.subscribe(topic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" попробуй еще раз через 5 секунд");
      delay(5000);
    }
    attempts++;
  }
  if (!client.connected()) {
    Serial.println("Отмена подключения к MQTT");
    connectionCancelled = true;  // установка флага отмены подключения
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // обработка полученного сообщения
  if (strcmp(topic, "motor") == 0) {
    if (payload[0] == '1') {
      Serial.println("Распыление удалённо!"); //выводим в консоль, что мотор заработал
      digitalWrite(motorPin, HIGH);  // Включение мотора
      delay(50);
      digitalWrite(motorPin, LOW);  // Выключение мотора
    }
  }
}
