volatile bool compatibleLightSleepWoke = false;

void compatibleLightSleepWakeCallback() {
  compatibleLightSleepWoke = true;
}

bool compatiblePowerSavingActive() {
  return powerSavingMode == POWER_SAVE_LIGHT;
}

uint32_t powerSavingNow() {
  uint32_t now = millis();
  return now == 0 ? 1 : now;
}

bool compatiblePowerTimersIdle() {
  return pretimerStartTime == 0 &&
         timerStartTime == 0 &&
         !workmode &&
         sprayCooldownRemainingMs() == 0;
}

bool compatiblePowerSleepAllowed() {
  if (!compatiblePowerSavingActive()) return false;
  if (lightLevel > lightTreshold) return false;
  if (!compatiblePowerTimersIdle()) return false;
  if (espMode == 1 && !stationHasValidConnection()) return false;

  return digitalRead(buttonPin) == HIGH;
}

void resetPowerSavingIdleWindow() {
  lightIdleStartMs = 0;
}

void notePowerSavingActivity() {
  if (compatiblePowerSavingActive()) {
    lightIdleStartMs = powerSavingNow();
  }
}

void notePowerSavingWebActivity() {
  if (compatiblePowerSavingActive()) {
    lightLastWebActivityMs = powerSavingNow();
    notePowerSavingActivity();
  }
}

void releasePowerSavingWebActivity() {
  lightLastWebActivityMs = 0;
  if (compatiblePowerSavingActive()) {
    lightIdleStartMs = powerSavingNow();
  }
}

void holdCompatiblePowerForMaintenance() {
  if (!compatiblePowerSavingActive()) return;
  lightSleepHoldActive = true;
  lightSleepHoldStartMs = powerSavingNow();
  notePowerSavingWebActivity();
}

void clearPowerSavingRuntimeState() {
  lightIdleStartMs = 0;
  lightLastWebActivityMs = 0;
  lightSleepHoldStartMs = 0;
  lightSleepHoldActive = false;
}

uint32_t remainingPowerSavingDelay(uint32_t now, uint32_t started,
                                   uint32_t durationMs) {
  if (started == 0) return 0;
  uint32_t elapsed = now - started;
  if (elapsed >= durationMs) return 0;
  return (durationMs - elapsed + 999UL) / 1000UL;
}

uint32_t compatiblePowerSecondsUntilSleep() {
  if (!compatiblePowerSleepAllowed()) return 0;

  uint32_t now = powerSavingNow();
  uint32_t remaining = lightIdleStartMs == 0 ? lightAwakeSeconds :
      remainingPowerSavingDelay(now, lightIdleStartMs,
                                (uint32_t)lightAwakeSeconds * 1000UL);

  uint32_t webRemaining = remainingPowerSavingDelay(
      now, lightLastWebActivityMs, LIGHT_SLEEP_WEB_SESSION_TIMEOUT_MS);
  if (webRemaining > remaining) remaining = webRemaining;

  if (lightSleepHoldActive) {
    uint32_t holdRemaining = remainingPowerSavingDelay(
        now, lightSleepHoldStartMs, LIGHT_SLEEP_HOLD_MS);
    if (holdRemaining > remaining) remaining = holdRemaining;
  }
  return remaining;
}

void initPowerSavingManagement() {
  clearPowerSavingRuntimeState();

  if (!compatiblePowerSavingActive()) {
    LOG.println(F("Энергосбережение отключено"));
    return;
  }

  LOG.println(F("Энергосбережение включено"));
  LOG.print(F("Период сна: "));
  LOG.print(lightSleepSeconds);
  LOG.print(F(" с; окно Wi-Fi, Web и MQTT: "));
  LOG.print(lightAwakeSeconds);
  LOG.println(F(" с"));
}

void restoreNetworkAfterCompatibleLightSleep() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  wifiDisconnectEventPending = false;
  wifiGotIpEventPending = false;
  routerConnected = false;
  lastStationIP = IPAddress();
  apFallbackActive = false;
  apFallbackStartMs = 0;
  lastRouterRetryMs = millis();

  if (espMode == 0) {
    StartAPMode();
    HTTP.begin();
    restartDiscoveryUdp();
    LOG.println(F("Точка доступа и Web восстановлены после сна"));
  } else {
    WiFi.mode(WIFI_STA);
    keepWiFiRadioAwake();
    HTTP.begin();
    if (beginNextConfiguredNetwork()) {
      LOG.println(F("Восстановление Wi-Fi после сна запущено"));
    } else {
      LOG.println(F("Нет сохранённой сети, запускается точка доступа"));
      apFallbackActive = StartAPMode();
      apFallbackStartMs = millis();
      restartDiscoveryUdp();
    }
  }

  lightIdleStartMs = 0;
  lightLastWebActivityMs = 0;
  previousTime = millis() - interval;
  mqttNeedToPublish = true;
}

void creditTimedLightSleepToPretimer(uint32_t timedSleepMs) {
  if (timedSleepMs == 0 || pretimerStartTime != 0 || workmode) return;

  lightLevel = analogRead(lightSensorPin);
  LOG.print(F("Датчик освещения после сна: "));
  LOG.println(lightLevel);
  if (lightLevel <= lightTreshold) return;

  uint32_t creditedMs = timedSleepMs > preTimer ? preTimer : timedSleepMs;
  uint32_t now = powerSavingNow();
  pretimerStartTime = now - creditedMs;
  if (pretimerStartTime == 0) pretimerStartTime = 0xFFFFFFFFUL;

  LOG.print(F("Свет включён после сна. В предтаймер зачтено: "));
  LOG.print(creditedMs / 1000UL);
  LOG.println(F(" с"));
  if (creditedMs >= preTimer) {
    LOG.println(F("Время предтаймера уже набрано во время сна"));
  }
  mqttNeedToPublish = true;
}

uint32_t compatibleRtcElapsedMs(uint32_t startRtcTicks,
                                uint32_t rtcCalibration) {
  if (rtcCalibration == 0) return 0;
  uint32_t elapsedTicks = system_get_rtc_time() - startRtcTicks;
  uint64_t elapsedUs = ((uint64_t)elapsedTicks * rtcCalibration) >> 12;
  uint64_t elapsedMs = elapsedUs / 1000ULL;
  return elapsedMs > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : (uint32_t)elapsedMs;
}

void waitForCompatibleSleepButtonRelease() {
  do {
    while (digitalRead(buttonPin) == LOW) delay(5);
    delay(BUTTON_DEBOUNCE_MS);
  } while (digitalRead(buttonPin) == LOW);

  buttonLastReading = HIGH;
  buttonStableState = HIGH;
  buttonLastChangeMs = millis();
}

void enterCompatibleLightSleep() {
  if (!compatiblePowerSleepAllowed()) return;

  if (client.connected()) {
    mqttNeedToPublish = true;
    publishMqttState();
    client.loop();
    client.publish(mqttAvailabilityTopic.c_str(), "offline", true);
    client.loop();
    delay(30);
    client.disconnect();
  }

  LOG.print(F("Темно, таймеры остановлены. Сон на "));
  LOG.print(lightSleepSeconds);
  LOG.println(F(" с"));

  HTTP.stop();
  stopDiscoveryUdp();
  stopSSDP();
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_OFF);
  delay(10);

  digitalWrite(motorPin, LOW);
  Serial.flush();

  uint32_t scheduledSleepMs = (uint32_t)lightSleepSeconds * 1000UL;
  uint32_t remainingMs = scheduledSleepMs;
  uint32_t sleepRtcCalibration = system_rtc_clock_cali_proc();
  uint32_t sleepStartRtcTicks = system_get_rtc_time();
  bool sleepStarted = true;
  bool sleepButtonSprayed = false;
  uint32_t lastSleepButtonRtcTicks = 0;
  while (remainingMs > 0) {
    if (remainingMs < LIGHT_SLEEP_MIN_SEGMENT_MS) {
      remainingMs = 0;
      break;
    }
    uint32_t segmentMs = remainingMs > LIGHT_SLEEP_BUTTON_POLL_MS ?
                         LIGHT_SLEEP_BUTTON_POLL_MS : remainingMs;
    compatibleLightSleepWoke = false;
    wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
    wifi_fpm_set_wakeup_cb(compatibleLightSleepWakeCallback);
    wifi_fpm_open();

    int8_t sleepResult = wifi_fpm_do_sleep(segmentMs * 1000UL);
    if (sleepResult != 0) {
      LOG.print(F("Сон не запущен, код: "));
      LOG.println((int)sleepResult);
      wifi_fpm_close();
      sleepStarted = false;
      break;
    }

    delay(segmentMs + 1UL);
    wifi_fpm_close();

    if (digitalRead(buttonPin) == LOW) {
      if (sleepButtonSprayed &&
          compatibleRtcElapsedMs(lastSleepButtonRtcTicks,
                                 sleepRtcCalibration) >= SPRAY_LOCKOUT_MS) {
        sprayHasRun = false;
      }
      bool sprayed = activateSprayer(nullptr, "button");
      if (sprayed) {
        sleepButtonSprayed = true;
        lastSleepButtonRtcTicks = system_get_rtc_time();
        LOG.println(F("Распыление выполнено во время сна"));
      }
      LOG.println(F("Wi-Fi остаётся выключенным до окончания сна"));
      waitForCompatibleSleepButtonRelease();
      uint32_t elapsedMs = compatibleRtcElapsedMs(
          sleepStartRtcTicks, sleepRtcCalibration);
      if (elapsedMs > 0 && elapsedMs < scheduledSleepMs) {
        remainingMs = scheduledSleepMs - elapsedMs;
        LOG.print(F("Кнопка отпущена. Сон продолжится, осталось: "));
        LOG.print((remainingMs + 999UL) / 1000UL);
        LOG.println(F(" с"));
      } else if (elapsedMs >= scheduledSleepMs) {
        remainingMs = 0;
        LOG.println(F("Период сна завершён во время обработки кнопки"));
      } else {
        remainingMs = remainingMs > segmentMs ? remainingMs - segmentMs : 0;
        LOG.println(F("RTC недоступен, остаток сна рассчитан по сегменту"));
      }
      Serial.flush();
      continue;
    }

    if (!compatibleLightSleepWoke) {
      LOG.println(F("Сон завершён без отметки callback"));
    }
    uint32_t elapsedMs = compatibleRtcElapsedMs(
        sleepStartRtcTicks, sleepRtcCalibration);
    if (elapsedMs > 0) {
      remainingMs = elapsedMs >= scheduledSleepMs ?
                    0 : scheduledSleepMs - elapsedMs;
    } else {
      remainingMs -= segmentMs;
    }
  }

  if (sleepButtonSprayed &&
      compatibleRtcElapsedMs(lastSleepButtonRtcTicks,
                             sleepRtcCalibration) >= SPRAY_LOCKOUT_MS) {
    sprayHasRun = false;
  }

  if (sleepStarted && remainingMs == 0) {
    creditTimedLightSleepToPretimer(scheduledSleepMs);
  }

  restoreNetworkAfterCompatibleLightSleep();
  LOG.println(sleepStarted ? F("Сон завершён, устройство проснулось") :
                             F("Сон отменён, устройство осталось включено"));
}

void powerSavingLoop() {
  if (!compatiblePowerSavingActive()) {
    clearPowerSavingRuntimeState();
    return;
  }

  uint32_t now = powerSavingNow();

  if (lightSleepHoldActive) {
    if (now - lightSleepHoldStartMs < LIGHT_SLEEP_HOLD_MS) return;
    lightSleepHoldActive = false;
    lightSleepHoldStartMs = 0;
  }

  if (!compatiblePowerSleepAllowed()) {
    resetPowerSavingIdleWindow();
    return;
  }

  if (remainingPowerSavingDelay(now, lightLastWebActivityMs,
                                LIGHT_SLEEP_WEB_SESSION_TIMEOUT_MS) > 0) {
    return;
  }

  if (lightIdleStartMs == 0) {
    lightIdleStartMs = now;
    LOG.print(F("Темно, таймеры остановлены. Окно Wi-Fi, Web и MQTT: "));
    LOG.print(lightAwakeSeconds);
    LOG.println(F(" с"));
    return;
  }

  if (now - lightIdleStartMs < (uint32_t)lightAwakeSeconds * 1000UL) return;
  enterCompatibleLightSleep();
}
