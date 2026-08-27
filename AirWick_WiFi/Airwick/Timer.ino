const uint8_t PRETIMER_MIN_MINUTES = 1;
const uint8_t PRETIMER_MAX_MINUTES = 10;
const uint8_t INTERVAL_MIN_MINUTES = 1;
const uint8_t INTERVAL_MAX_MINUTES = 60;

int clampMinutes(int value, int minimumValue, int maximumValue) {
  if (value < minimumValue) return minimumValue;
  if (value > maximumValue) return maximumValue;
  return value;
}

bool parseTimerMinutes(const String& value, int minimumValue,
                       int maximumValue, int& minutes) {
  if (!value.length()) return false;
  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value[i])) return false;
  }
  minutes = value.toInt();
  return minutes >= minimumValue && minutes <= maximumValue;
}

void sendTimerValidationError() {
  LOG.println(F("Отклонено неверное значение таймера"));
  HTTP.send(400, "application/json",
            "{\"ok\":false,\"error\":\"timer value is out of range\"}");
}

void setPreTimerMinutes(int minutes, bool persist) {
  minutes = clampMinutes(minutes, PRETIMER_MIN_MINUTES, PRETIMER_MAX_MINUTES);
  preTimer = (uint32_t)minutes * 60000UL;
  jsonWrite(configSetup, "preTimer", minutes);
  if (persist) {
    saveConfig();
    LOG.print(F("Предтаймер сохранён: "));
    LOG.print(minutes);
    LOG.println(F(" мин"));
  }
}

void setSprayIntervalMinutes(int minutes, bool persist) {
  minutes = clampMinutes(minutes, INTERVAL_MIN_MINUTES, INTERVAL_MAX_MINUTES);
  timerDuration = (uint32_t)minutes * 60000UL;
  jsonWrite(configSetup, "Interval", minutes);
  if (persist) {
    saveConfig();
    LOG.print(F("Интервал распыления сохранён: "));
    LOG.print(minutes);
    LOG.println(F(" мин"));
  }
}

void Timer_init() {
  int configuredPreTimer = jsonReadtoInt(configSetup, "preTimer");
  int configuredInterval = jsonReadtoInt(configSetup, "Interval");
  int validPreTimer =
      configuredPreTimer >= PRETIMER_MIN_MINUTES &&
      configuredPreTimer <= PRETIMER_MAX_MINUTES ? configuredPreTimer : 1;
  int validInterval =
      configuredInterval >= INTERVAL_MIN_MINUTES &&
      configuredInterval <= INTERVAL_MAX_MINUTES ? configuredInterval : 2;
  bool changed = validPreTimer != configuredPreTimer || validInterval != configuredInterval;

  setPreTimerMinutes(validPreTimer, false);
  setSprayIntervalMinutes(validInterval, false);
  if (changed) saveConfig();

  LOG.print(F("Таймеры: предтаймер="));
  LOG.print(validPreTimer);
  LOG.print(F(" мин, интервал распыления="));
  LOG.print(validInterval);
  LOG.println(F(" мин"));

  HTTP.on("/setPreTimers", handle_Pretimers);
  HTTP.on("/setpretimer", handle_pretimer);     // Совместимость со старой страницей.
  HTTP.on("/preTimerm", handle_preTimerm);
  HTTP.on("/preTimerp", handle_preTimerp);

  HTTP.on("/setTimers", handle_timers);
  HTTP.on("/setmaintimer", handle_maintimer);   // Совместимость со старой страницей.
  HTTP.on("/Timerm", handle_Timerm);
  HTTP.on("/Timerp", handle_Timerp);
}

void handle_Pretimers() {
  int minutes;
  if (!parseTimerMinutes(HTTP.arg("pretimer"), PRETIMER_MIN_MINUTES,
                         PRETIMER_MAX_MINUTES, minutes)) {
    sendTimerValidationError();
    return;
  }
  setPreTimerMinutes(minutes, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_pretimer() {
  int minutes;
  if (!parseTimerMinutes(HTTP.arg("val"), PRETIMER_MIN_MINUTES,
                         PRETIMER_MAX_MINUTES, minutes)) {
    sendTimerValidationError();
    return;
  }
  setPreTimerMinutes(minutes, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_preTimerm() {
  setPreTimerMinutes((int)(preTimer / 60000UL) - 1, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_preTimerp() {
  setPreTimerMinutes((int)(preTimer / 60000UL) + 1, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_timers() {
  int minutes;
  if (!parseTimerMinutes(HTTP.arg("interval"), INTERVAL_MIN_MINUTES,
                         INTERVAL_MAX_MINUTES, minutes)) {
    sendTimerValidationError();
    return;
  }
  setSprayIntervalMinutes(minutes, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_maintimer() {
  int minutes;
  if (!parseTimerMinutes(HTTP.arg("val"), INTERVAL_MIN_MINUTES,
                         INTERVAL_MAX_MINUTES, minutes)) {
    sendTimerValidationError();
    return;
  }
  setSprayIntervalMinutes(minutes, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_Timerm() {
  setSprayIntervalMinutes((int)(timerDuration / 60000UL) - 1, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}

void handle_Timerp() {
  setSprayIntervalMinutes((int)(timerDuration / 60000UL) + 1, true);
  HTTP.send(200, "application/json", "{\"should_refresh\":true}");
}
