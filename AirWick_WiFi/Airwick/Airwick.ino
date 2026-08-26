#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>

// Объект для обнавления с web страницы
ESP8266HTTPUpdateServer httpUpdater;
// Web интерфейс для устройства
ESP8266WebServer HTTP(80);
// Для файловой системы и встроенного редактора
File fsUploadFile;

#define AIRWICK_VERSION ("3.0")

ESP8266WiFiMulti wifiMulti;

const uint8_t AP_STATIC_IP[] = {192, 168, 4, 1};
const uint32_t WIFI_FALLBACK_DELAY = 20000UL;
const uint32_t WIFI_ROUTER_RETRY_INTERVAL = 300000UL;
const uint32_t WIFI_ROUTER_RETRY_WINDOW = 20000UL;
const uint32_t WIFI_ROUTER_RETRY_STEP = 500UL;
const uint32_t MQTT_RECONNECT_INTERVAL = 10000UL;
const uint32_t SPRAY_LOCKOUT_MS = 3000UL;
const uint32_t BUTTON_DEBOUNCE_MS = 60UL;

const int lightSensorPin = A0;  // Пин, к которому подключен датчик света
const int motorPin = D1;        // Пин, к которому подключен мотор
const int buttonPin = D3;       // Пин, к которому подключена кнопка

unsigned long previousTime = 0;        // Предыдущее время опроса датчика
const unsigned long interval = 10000;  // Интервал опроса датчика (10 секунд)
unsigned long pretimerStartTime = 0;   // Время старта предварительного таймера
unsigned long timerStartTime = 0;      // Время старта таймера интервала распыления
unsigned long timerDuration = 120000;  // Интервал распыления по умолчанию (2 минуты)
unsigned long preTimer = 60000;        // Длительность таймера (60 секунд)
uint16_t lightTreshold = 500;          // Порог срабатывания датчика света
bool workmode = false;                 // Флаг запуска таймера режима распыления
int lightLevel;                        // Уровень освещения
bool buttonLastReading = HIGH;
bool buttonStableState = HIGH;
uint32_t buttonLastChangeMs = 0;
bool sprayHasRun = false;
uint32_t lastSprayMs = 0;
String lastSpraySource = "none";

String configSetup = "{}";
String configJson = "{}";
String mqttconfigJson = "{}";
String ipconfigJson = "{}";

uint8_t use_static_ip = 0;
IPAddress Static_IP;         // Статический IP
IPAddress Gateway;//         // Шлюз
IPAddress Subnet;            // маска подсети
IPAddress DNS1;              // Серверы DNS. Можно также DNS1(1,1,1,1) или DNS1(8,8,4,4);
IPAddress DNS2(8, 8, 8, 8);  // Резервный DNS
bool staticIpConfigValid = false;

uint8_t espMode = 0;
uint16_t ESP_CONN_TIMEOUT = 60;
bool routerConnected = false;
bool apFallbackActive = false;
bool ssdpInitialized = false;
uint32_t apFallbackStartMs = 0;
uint32_t lastRouterRetryMs = 0;
uint8_t configuredWiFiNetworks = 0;
bool routerRetryActive = false;
uint32_t routerRetryStartMs = 0;
uint32_t lastRouterRetryStepMs = 0;

String mqttServer = "";
uint16_t mqttPort = 1883;
String mqttUser = "";
String mqttPassword = "";
String mqttTopicBase = "AirWick";
String mqttClientID = "";
String mqttCommandTopic = "";
String mqttStateTopic = "";
String mqttEventTopic = "";
String mqttAvailabilityTopic = "";
bool useMQTT = false;
bool mqttConfigValid = false;
uint8_t mqttPeriod = 0;
uint32_t mqttLastConnectingAttempt = 0;
uint32_t mqttPublishTimer = 0;
bool mqttNeedToPublish = false;
bool mqttSprayEventPending = false;
String mqttPendingSpraySource = "none";

WiFiClient espClient;
PubSubClient client(espClient);

uint32_t sprayCooldownRemainingMs() {
  if (!sprayHasRun) return 0;
  uint32_t elapsed = millis() - lastSprayMs;
  return elapsed >= SPRAY_LOCKOUT_MS ? 0 : SPRAY_LOCKOUT_MS - elapsed;
}

bool activateSprayer(const __FlashStringHelper* message, const char* source) {
  uint32_t cooldown = sprayCooldownRemainingMs();
  if (cooldown) {
    Serial.print(F("Распыление заблокировано, осталось мс: "));
    Serial.println(cooldown);
    return false;
  }
  if (message != nullptr) {
    Serial.println(message);
  }
  lastSprayMs = millis();
  sprayHasRun = true;
  lastSpraySource = source != nullptr ? source : "unknown";
  digitalWrite(motorPin, HIGH);
  delay(50);
  digitalWrite(motorPin, LOW);
  mqttPendingSpraySource = lastSpraySource;
  mqttSprayEventPending = useMQTT && client.connected();
  mqttNeedToPublish = true;
  return true;
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

    // Запускаем файловую систему
    FS_init();
    configSetup = readFile("config.json", 4096);
    if (configSetup == "Failed" || configSetup == "Large") {
      configSetup = "{}";
    }
    migrateNetworkConfig();
    jsonWrite(configJson, "SSDP", jsonRead(configSetup, "SSDP"));
    jsonWrite(configJson, "ver", AIRWICK_VERSION);
    init_ip();
    // Чтение порогового значения датчика света
    int configuredLightTreshold = jsonReadtoInt(configSetup, "light");
    if (configuredLightTreshold < 10 || configuredLightTreshold > 1023) {
      configuredLightTreshold = 500;
      jsonWrite(configSetup, "light", configuredLightTreshold);
      saveConfig();
    }
    lightTreshold = (uint16_t)configuredLightTreshold;
    // Запускаем WIFI
    WIFIinit();
    // Быстрый поиск через Fiery Lamp Control: UDP DISCOVER + HTTP API.
    initDiscovery();
    // Получаем время из сети
    Time_init();
    // Таймеры
    Timer_init();
    // HTTP-обработчики SSDP нужны и в режиме точки доступа.
    registerSSDPHandlers();
    // SSDP запускается только после успешного подключения к роутеру.
    if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
      SSDP_init();
      ssdpInitialized = true;
    }
    // Регистрируем обработчики web-интерфейса до запуска HTTP сервера.
    User_setings();
    init_mqtt();
    GRAF_init();
    // Настраиваем и запускаем HTTP интерфейс
    HTTP_init();
    lightLevel = analogRead(lightSensorPin);
  }
   
  void loop() {
    discoveryLoop();
    checkWiFiFallback();
    wifiReconnect();
    HTTP.handleClient();
    delay(1);
    unsigned long currentTime = millis();

    // Опрос датчика каждый заданный интервал секунд
    if (currentTime - previousTime >= interval) {
      previousTime = currentTime;
      lightLevel = analogRead(lightSensorPin);
     Serial.println(lightLevel);
   
    
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
    bool buttonReading = digitalRead(buttonPin);
    if (buttonReading != buttonLastReading) {
      buttonLastReading = buttonReading;
      buttonLastChangeMs = currentTime;
    }
    if (buttonReading != buttonStableState &&
        currentTime - buttonLastChangeMs >= BUTTON_DEBOUNCE_MS) {
      buttonStableState = buttonReading;
      if (buttonStableState == LOW) {
        activateSprayer(F("Кнопка нажата"), "button");
      }
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
      activateSprayer(F("Распыление!"), "auto");
    }
    mqttLoop();
  }
