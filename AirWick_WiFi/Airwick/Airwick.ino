#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPUpdateServer.h>
#ifndef MQTT_MAX_PACKET_SIZE
#define MQTT_MAX_PACKET_SIZE 768
#endif
#include <PubSubClient.h>
#include "SystemLog.h"

extern "C" {
#include <user_interface.h>
#include <ping.h>
}
#define LOG SystemLog::instance()

// Объект для обнавления с web страницы
ESP8266HTTPUpdateServer httpUpdater;
// Web интерфейс для устройства
ESP8266WebServer HTTP(80);
// Для файловой системы и встроенного редактора
File fsUploadFile;

#define AIRWICK_VERSION ("3.3")

ESP8266WiFiMulti wifiMulti;

void notePowerSavingActivity();
void notePowerSavingWebActivity();
void releasePowerSavingWebActivity();
void holdCompatiblePowerForMaintenance();
void clearPowerSavingRuntimeState();
bool compatiblePowerSavingActive();
bool compatiblePowerTimersIdle();
bool compatiblePowerSleepAllowed();
uint32_t compatiblePowerSecondsUntilSleep();
void initPowerSavingManagement();
void powerSavingLoop();
bool publishMqttState();
bool stationHasValidConnection();
bool StartAPMode();
bool beginNextConfiguredNetwork();
bool applyWiFiPowerMode();
bool keepWiFiRadioAwake();
bool applyStaticIpConfig();
void stopDiscoveryUdp();
void restartDiscoveryUdp();
void stopSSDP();
void wifiHealthLoop();
void forceWiFiStackRecovery();
void stopMqttForNetworkRecovery();

const uint8_t AP_STATIC_IP[] = {192, 168, 4, 1};
const uint32_t WIFI_FALLBACK_DELAY = 20000UL;
const uint32_t WIFI_ROUTER_RETRY_INTERVAL = 15000UL;
const uint32_t WIFI_DIAGNOSTIC_INTERVAL = 300000UL;
const uint32_t WIFI_PREVENTIVE_RECOVERY_INTERVAL = 21600000UL; // 6 часов
const uint32_t WIFI_HEAP_GUARD_INTERVAL = 60000UL;              // 1 минута
const uint32_t WIFI_CRITICAL_FREE_HEAP = 8000UL;
const uint32_t MQTT_RECONNECT_INTERVAL = 10000UL;
const uint32_t MQTT_PUBLISH_RETRY_INTERVAL = 5000UL;
const uint16_t MQTT_TCP_CONNECT_TIMEOUT_MS = 1500;
const uint8_t MQTT_TRANSPORT_FAILURE_LIMIT = 3;
const uint32_t SPRAY_LOCKOUT_MS = 3000UL;
const uint16_t SPRAY_PULSE_MS = 50;
const uint32_t BUTTON_DEBOUNCE_MS = 60UL;
const uint8_t POWER_SAVE_OFF = 0;
const uint8_t POWER_SAVE_LIGHT = 1;
const uint16_t LIGHT_SLEEP_MIN_SECONDS = 10;
const uint16_t LIGHT_SLEEP_MAX_SECONDS = 300;
const uint16_t LIGHT_AWAKE_MIN_SECONDS = 3;
const uint16_t LIGHT_AWAKE_MAX_SECONDS = 60;
const uint32_t LIGHT_SLEEP_HOLD_MS = 600000UL;
const uint32_t LIGHT_SLEEP_WEB_SESSION_TIMEOUT_MS = 15000UL;
const uint16_t LIGHT_SLEEP_WIFI_TIMEOUT_SECONDS = 8;
const uint16_t LIGHT_SLEEP_MIN_SEGMENT_MS = 10;
const uint16_t LIGHT_SLEEP_BUTTON_POLL_MS = 50;
const uint8_t POWER_SAVING_SCHEMA_VERSION = 3;

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
uint8_t powerSavingMode = POWER_SAVE_OFF;
uint16_t lightSleepSeconds = 20;
uint16_t lightAwakeSeconds = 5;
uint32_t lightIdleStartMs = 0;
uint32_t lightLastWebActivityMs = 0;
uint32_t lightSleepHoldStartMs = 0;
bool lightSleepHoldActive = false;
uint16_t ESP_CONN_TIMEOUT = 60;
bool routerConnected = false;
bool apFallbackActive = false;
bool ssdpInitialized = false;
uint32_t apFallbackStartMs = 0;
uint32_t lastRouterRetryMs = 0;
uint8_t configuredWiFiNetworks = 0;
uint8_t nextWiFiNetworkIndex = 0;
uint32_t lastWiFiDiagnosticMs = 0;
uint32_t lastWiFiPreventiveRecoveryMs = 0;
uint32_t lastWiFiHeapGuardMs = 0;
uint32_t lastNetworkSuccessMs = 0;
uint16_t wifiRecoveryCount = 0;
IPAddress lastStationIP;
WiFiEventHandler wifiGotIpEventHandler;
WiFiEventHandler wifiDisconnectedEventHandler;
volatile bool wifiGotIpEventPending = false;
volatile bool wifiDisconnectEventPending = false;
volatile uint8_t lastWiFiDisconnectReason = 0;

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
uint32_t mqttLastPublishFailureMs = 0;
uint8_t mqttTransportFailureCount = 0;
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
    LOG.print(F("Распыление заблокировано, осталось мс: "));
    LOG.println(cooldown);
    return false;
  }
  if (message != nullptr) {
    LOG.println(message);
  }
  lastSprayMs = millis();
  sprayHasRun = true;
  lastSpraySource = source != nullptr ? source : "unknown";
  digitalWrite(motorPin, HIGH);
  delay(SPRAY_PULSE_MS);
  digitalWrite(motorPin, LOW);
  mqttPendingSpraySource = lastSpraySource;
  mqttSprayEventPending = useMQTT && client.connected();
  mqttNeedToPublish = true;
  notePowerSavingActivity();
  return true;
}

  void setup() {
    Serial.begin(115200);
    delay(5);
    LOG.println();
    LOG.println(F("========================================"));
    LOG.println(F("SYSTEM START AIRWICK ESP8266"));
    LOG.print(F("Версия прошивки: "));
    LOG.println(AIRWICK_VERSION);
    LOG.print(F("Причина перезапуска: "));
    LOG.println(ESP.getResetReason());
    LOG.print(F("Свободная память: "));
    LOG.print(ESP.getFreeHeap());
    LOG.println(F(" байт"));

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
      LOG.println(F("config.json не прочитан, используются безопасные значения"));
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
    lightLevel = analogRead(lightSensorPin);
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
    initSystemLogRoutes();
    // Настраиваем и запускаем HTTP интерфейс
    HTTP_init();
    lightLevel = analogRead(lightSensorPin);
    initPowerSavingManagement();
    LOG.println(F("Инициализация AirWick завершена"));
  }
   
  void loop() {
    discoveryLoop();
    checkWiFiFallback();
    wifiReconnect();
    wifiHealthLoop();
    HTTP.handleClient();
    delay(1);
    unsigned long currentTime = millis();

    // Опрос датчика каждый заданный интервал секунд
    if (currentTime - previousTime >= interval) {
      previousTime = currentTime;
      lightLevel = analogRead(lightSensorPin);
     LOG.print(F("Датчик освещения: "));
     LOG.println(lightLevel);
   
    
     if (lightLevel > lightTreshold) {
        // Если свет горит, запускаем предварительный таймер
        if (pretimerStartTime == 0 && workmode == false) {
          LOG.println(F("Предтаймер запущен"));
          pretimerStartTime = currentTime;
        }
      } else {
        // Если свет выключен, останавливаем предварительныйтаймер
        pretimerStartTime = 0;
        LOG.println(F("Предтаймер остановлен"));
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
      LOG.println(F("Включён рабочий режим распыления"));
      workmode = true;               //переходим в режим распыления
      timerStartTime = currentTime;  //запускаем таймер распыления
    }

    // Проверяем состояние основного таймера
    if (timerStartTime > 0 && currentTime - timerStartTime >= timerDuration) {
      //если свет все еще горит перезапускаем таймер, если нет - останавливаем
      if (workmode) {
        timerStartTime = currentTime;
        LOG.println(F("Таймер распыления перезапущен"));
      } else {
        timerStartTime = 0;
        LOG.println(F("Таймер распыления остановлен"));
      }
      activateSprayer(F("Распыление!"), "auto");
    }
    mqttLoop();
    powerSavingLoop();
  }
