#pragma once

#include <Arduino.h>

#ifdef printf_P
#undef printf_P
#endif

class SystemLog : public Print {
public:
  static SystemLog& instance();

  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buffer, size_t size) override;

  size_t printf(const char* format, ...);
  size_t printf_P(PGM_P format, ...);
  void add(const char* format, ...);
  void add(const String& text);
  void add(char c);

  void clear();
  const String& getAll() const;
  void setEnabled(bool enabled);
  bool isEnabled() const;

private:
  SystemLog();
  void appendToBuffer(char c);
  void appendFormatted(const char* format, va_list args, bool progmem);
  void addPrefixIfNeeded();

  String _buffer;
  bool _enabled = true;
  bool _lineStart = true;
  static const size_t MAX_BUFFER = 6144;
};

void initSystemLogRoutes();
