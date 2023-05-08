#ifndef WEBVIEW_LOGGING_DEF
#define WEBVIEW_LOGGING_DEF

#ifndef NDEBUG
#include <stdarg.h>
#include <stdio.h>

void debug_logf_actual(int line, char *file, char *const fmt, ...) {
  va_list args;
  va_start(args, fmt);
  printf("%s#%d ", file, line);
  vprintf(fmt, args);
}
#define debug_logf(format, ...)                                                \
  debug_logf_actual(__LINE__, __FILE__, format, __VA_ARGS__)
#else
void debug_logf(char *const fmt, ...) {}
#endif

#endif