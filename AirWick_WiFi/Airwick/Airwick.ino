#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>

// Объект для обнавления с web страницы
ESP8266HTTPUpdateServer httpUpdater;

// Web интерфейс для устройства
ESP8266WebServer HTTP(80);

// Для файловой системы
File fsUploadFile;

#define FLL_VERSION (" Ver.1.1")

const int lightSensorPin = A0;  // Пин, к которому подключен датчик света
const int motorPin = D1;        // Пин, к которому подключен мотор
const int buttonPin = D3;       // Пин, к которому подключена кнопка

unsigned long previousTime = 0;        // Предыдущее время опроса датчика
const unsigned long interval = 10000;  // Интервал опроса датчика (10 секунд)

unsigned long pretimerStartTime = 0;   // Время старта предварительного таймера
unsigned long timerStartTime = 0;      // Время старта таймера интервала распыления
unsigned long timerDuration = 240000;  // Длительность таймера (120 секунд)
unsigned long preTimer = 60000;        // Длительность таймера (60 секунд)
uint16_t lightTreshold = 500;          // Порог срабатывания датчика света
bool workmode = false;                 // флаг запуска таймера режима распыления
int lightLevel;                        // Уровень освещения

String configSetup = "{}";
String configJson = "{}";
String mqttconfigJson = "{}";

String mqttServer = "";
int mqttPort = 1883;
String mqttUser = "";
String mqttPassword = "";
String topic = "motor";
String clientID="ESP8266Client-";     //id клиента
bool useMQTT=true;                    //флаг использования mqtt
bool lowPower = false;                //режим низкого энергопотребления

WiFiClient espClient;
PubSubClient client(espClient);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // обработка полученного сообщения
  if (strcmp(topic, "motor") == 0) {
    if (payload[0] == '1') {
      Serial.println("Распыление удалённо!"); //выводим в консоль, что мотор заработал
      digitalWrite(motorPin, HIGH);  // Включение мотора
      delay(700);
      digitalWrite(motorPin, LOW);  // Выключение мотора
    }
  }
}

void connectToMqtt() {
  while (!client.connected()) {
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
  void setup() {
    Serial.begin(115200);
    delay(5);
    Serial.println("");

    pinMode(lightSensorPin, INPUT);
    pinMode(motorPin, OUTPUT);
    pinMode(buttonPin, INPUT_PULLUP);  // Устанавливаем режим входа с подтяжкой вверх для кнопки
    digitalWrite(motorPin, LOW);       // Устанавливаем мотор в выключенное состояние
    pinMode(LED_BUILTIN, OUTPUT);      // Устанавливаем режим пина светодиода на OUTPUT
    digitalWrite(LED_BUILTIN, HIGH);   // Устанавливаем высокий уровень сигнала на пин светодиода (отключаем светодиод)

    //Запускаем файловую систему
    FS_init();
    configSetup = readFile("config.json", 4096);
    jsonWrite(configJson, "SSDP", jsonRead(configSetup, "SSDP"));
    jsonWrite(configJson, "ver", FLL_VERSION);
    //чтение параметров mqtt
    mqttconfigJson=readFile("mqtt_config.json", 4096);
    Serial.println(mqttconfigJson);
    mqttServer = jsonRead(mqttconfigJson, "mq_ip");
    mqttPort = jsonReadtoInt(mqttconfigJson, "mq_port");
    mqttUser = jsonRead(mqttconfigJson, "mq_ssid");
    mqttPassword = jsonRead(mqttconfigJson, "mq_pass");
    clientID = jsonRead(mqttconfigJson, "mq_id");
    useMQTT = jsonReadtoInt(mqttconfigJson, "mq_on");
    //чтение порогового значения датчика света
    lightTreshold = jsonReadtoInt(configSetup, "light");
    lowPower = jsonReadtoInt(configSetup, "lowPWR");
    if (lightTreshold==0) lightTreshold=500; 
    //Запускаем WIFI
    WIFIinit();
    // Получаем время из сети
    Time_init();
    // Таймеры
    Timer_init();
    //Настраиваем и запускаем SSDP интерфейс
    SSDP_init();
    //Настраиваем и запускаем HTTP интерфейс
    HTTP_init();
    //
    User_setings();
    // Подключение к MQTT-серверу
    client.setServer(mqttServer.c_str(), mqttPort);
    client.setCallback(mqttCallback);
    //Инициализация построения графика
    GRAF_init();
    lightLevel = analogRead(lightSensorPin);
  }

  void loop() {
    HTTP.handleClient();
    delay(1);
    unsigned long currentTime = millis();

    // Опрос датчика каждый заданный интервал секунд
    if (currentTime - previousTime >= interval) {
      previousTime = currentTime;
      int lightLevel = analogRead(lightSensorPin);
     Serial.println(lightLevel);
   
    
    /*if (client.connected()) //Отправка в топик сообщения об уровне освещения
    {
    char msg[80];
    sprintf(msg,"Уровень освещения: %d",lightLevel);
    client.publish(statusTopic.c_str(),msg);
    }
    */
      if (lightLevel > lightTreshold) {
        // Если свет горит, запускаем предварительный таймер
        if (pretimerStartTime == 0 && workmode == false) {
          Serial.println("Предтаймер запущен!");
          pretimerStartTime = currentTime;
        }
      } else {
        // Если свет выключен, останавливаем предварительныйтаймер
        pretimerStartTime = 0;
        Serial.println("Предтаймер остановлен");
        workmode = false;  //отключаем режим распыления
      }
    }
    bool buttonState = digitalRead(buttonPin);  // Считываем состояние кнопки

    if (buttonState == LOW) {
      // Кнопка нажата, запустить мотор на 1 секунду
      Serial.println("Кнопка нажата");
      digitalWrite(motorPin, HIGH);
      delay(700);
      digitalWrite(motorPin, LOW);
    }

    // Проверяем состояние предварительного таймера
    if (pretimerStartTime > 0 && currentTime - pretimerStartTime >= preTimer) {
      pretimerStartTime = 0;  // Таймер истек, сбрасываем его состояние
      Serial.println("Переключиться в рабочий режим");
      workmode = true;               //переходим в режим распыления
      timerStartTime = currentTime;  //запускаем таймер распыления
    }

    // Проверяем состояние основного таймера
    if (timerStartTime > 0 && currentTime - timerStartTime >= timerDuration) {
      //если свет все еще горит перезапускаем таймер, если нет - останавливаем
      if (workmode) {
        timerStartTime = currentTime;
        Serial.println("Перезапуск таймера");
      } else {
        timerStartTime = 0;
        Serial.println("Таймер остановлен");
      }
      Serial.println("Распыление!");  //выводим в консоль, что мотор заработал
      digitalWrite(motorPin, HIGH);  // Запускаем мотор на 1 секунду
      delay(700);
      digitalWrite(motorPin, LOW);  // Выключаем мотор
    }
    // Проверка подключения к MQTT-серверу
    if (!client.connected() && useMQTT) {
      connectToMqtt();
    }
    client.loop();
    //переходим в режим низкого энергопотребления
    if (lowPower && lightLevel<lightTreshold && timerStartTime==0){
      Serial.println("Идти спать!");
      ESP.deepSleep(10e6);
      }
      
  }
