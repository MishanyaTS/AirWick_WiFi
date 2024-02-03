void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // обработка полученного сообщения
  if (strcmp(topic, "motor") == 0) {
    if (payload[0] == '1') {
      Serial.println("Распыление удалённо!"); //выводим в консоль, что мотор заработал
      digitalWrite(motorPin, HIGH);  // Включение мотора
      delay(300);
      digitalWrite(motorPin, LOW);  // Выключение мотора
    }
  }
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

//функция сохранения и применения настроек mqtt
void handle_mqtt_save() {
  useMQTT=HTTP.arg("mq_on").toInt();// включение mqtt
  mqttServer=HTTP.arg("mq_ip");// адрес сервера
  mqttPort=HTTP.arg("mq_port").toInt();// порт mqtt
  mqttUser=HTTP.arg("mq_ssid");// логин
  mqttPassword=HTTP.arg("mq_pass");// пароль
  clientID=HTTP.arg("mq_id");// id
  jsonWrite(mqttconfigJson, "mq_on", useMQTT); // сохраняем в json
  jsonWrite(mqttconfigJson, "mq_ip", mqttServer);  //сохраняем в json
  jsonWrite(mqttconfigJson, "mq_port", mqttPort); // сохраняем в json
  jsonWrite(mqttconfigJson, "mq_ssid", mqttUser);  //сохраняем в json
  jsonWrite(mqttconfigJson, "mq_pass", mqttPassword); // сохраняем в json
  jsonWrite(mqttconfigJson, "mq_id", clientID);  //сохраняем в json
  writeFile("mqtt_config.json", mqttconfigJson); //сохраняем в фс
  //переподключаемся к серверу
  client.setServer(mqttServer.c_str(), mqttPort);
  connectToMqtt();
  HTTP.send(200, "text/plain", "OK");
}

  void init_mqtt(){
//чтение параметров mqtt
    mqttconfigJson=readFile("mqtt_config.json", 4096);
    Serial.println(mqttconfigJson);
    mqttServer = jsonRead(mqttconfigJson, "mq_ip");
    mqttPort = jsonReadtoInt(mqttconfigJson, "mq_port");
    mqttUser = jsonRead(mqttconfigJson, "mq_ssid");
    mqttPassword = jsonRead(mqttconfigJson, "mq_pass");
    clientID = jsonRead(mqttconfigJson, "mq_id");
    useMQTT = jsonReadtoInt(mqttconfigJson, "mq_on");
    client.setServer(mqttServer.c_str(), mqttPort);
    client.setCallback(mqttCallback);
  }
