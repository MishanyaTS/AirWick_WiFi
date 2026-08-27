#include "SystemLog.h"
#include <time.h>

SystemLog::SystemLog() {
  _buffer.reserve(MAX_BUFFER + 64);
}

SystemLog& SystemLog::instance() {
  static SystemLog instance;
  return instance;
}

void SystemLog::setEnabled(bool enabled) {
  _enabled = enabled;
}

bool SystemLog::isEnabled() const {
  return _enabled;
}

void SystemLog::clear() {
  _buffer = "";
  _lineStart = true;
}

const String& SystemLog::getAll() const {
  return _buffer;
}

void SystemLog::addPrefixIfNeeded() {
  if (!_lineStart) return;

  char prefix[32];
  time_t now = time(nullptr);

  // После синхронизации NTP показываем текущее время устройства.
  if (now >= 1609459200L) {
    struct tm* timeInfo = localtime(&now);
    if (timeInfo != nullptr) {
      snprintf(prefix, sizeof(prefix), "[%02d:%02d:%02d] ",
               timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    } else {
      snprintf(prefix, sizeof(prefix), "[--:--:--] ");
    }
  } else {
    // До получения времени от NTP выводим время работы устройства.
    unsigned long totalSeconds = millis() / 1000UL;
    unsigned long hours = totalSeconds / 3600UL;
    unsigned long minutes = (totalSeconds / 60UL) % 60UL;
    unsigned long seconds = totalSeconds % 60UL;
    snprintf(prefix, sizeof(prefix), "[+%02lu:%02lu:%02lu] ",
             hours, minutes, seconds);
  }

  _buffer += prefix;
  _lineStart = false;
}

void SystemLog::appendToBuffer(char c) {
  if (!_enabled) return;

  addPrefixIfNeeded();
  _buffer += c;
  if (c == '\n') _lineStart = true;

  if (_buffer.length() > MAX_BUFFER) {
    int cut = (int)_buffer.length() - (int)MAX_BUFFER;
    int nextLine = _buffer.indexOf('\n', cut);
    if (nextLine >= 0 && nextLine + 1 < (int)_buffer.length()) {
      cut = nextLine + 1;
    }
    _buffer.remove(0, cut);
  }
}

size_t SystemLog::write(uint8_t c) {
  Serial.write(c);
  appendToBuffer((char)c);
  return 1;
}

size_t SystemLog::write(const uint8_t* buffer, size_t size) {
  Serial.write(buffer, size);
  if (_enabled && buffer != nullptr) {
    for (size_t i = 0; i < size; i++) appendToBuffer((char)buffer[i]);
  }
  return size;
}

void SystemLog::appendFormatted(const char* format, va_list args, bool progmem) {
  if (format == nullptr) return;

  char stackBuffer[192];
  va_list copy;
  va_copy(copy, args);
  int length = progmem
      ? vsnprintf_P(stackBuffer, sizeof(stackBuffer), format, copy)
      : vsnprintf(stackBuffer, sizeof(stackBuffer), format, copy);
  va_end(copy);

  if (length < 0) return;
  if (length < (int)sizeof(stackBuffer)) {
    print(stackBuffer);
    return;
  }

  char* heapBuffer = (char*)malloc((size_t)length + 1);
  if (heapBuffer == nullptr) {
    print(stackBuffer);
    return;
  }

  if (progmem) {
    vsnprintf_P(heapBuffer, (size_t)length + 1, format, args);
  } else {
    vsnprintf(heapBuffer, (size_t)length + 1, format, args);
  }
  print(heapBuffer);
  free(heapBuffer);
}

size_t SystemLog::printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  appendFormatted(format, args, false);
  va_end(args);
  return 0;
}

size_t SystemLog::printf_P(PGM_P format, ...) {
  va_list args;
  va_start(args, format);
  appendFormatted((const char*)format, args, true);
  va_end(args);
  return 0;
}

void SystemLog::add(const char* format, ...) {
  va_list args;
  va_start(args, format);
  appendFormatted(format, args, false);
  va_end(args);
  println();
}

void SystemLog::add(const String& text) {
  println(text);
}

void SystemLog::add(char c) {
  write((uint8_t)c);
}

void initSystemLogRoutes() {
  HTTP.on("/logs", HTTP_GET, []() {
    HTTP.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    HTTP.send(200, "text/plain; charset=utf-8", SystemLog::instance().getAll());
  });

  HTTP.on("/logs_clear", HTTP_GET, []() {
    SystemLog::instance().clear();
    LOG.println(F("Системный лог очищен через WEB"));
    HTTP.send(200, "text/plain; charset=utf-8", "OK");
  });

  HTTP.on("/logs_page", HTTP_GET, []() {
    if (!handleFileRead("/logs.htm")) {
      HTTP.send(404, "text/plain; charset=utf-8", "logs.htm not found");
    }
  });
}
