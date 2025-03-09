#ifndef LIB_H
#define LIB_H
#include <wchar.h>
typedef struct {
  char *dir;
  char *name;
  char *ext;
  int max_num_logs;
  size_t size_hint_bytes;
} FlogAttrs;

void flog_open(FlogAttrs *fa);
void flog_close();
void info(char *s);
void info_w(wchar_t *s);

#define TRACE_TAG "[TRACE]"
#define DEBUG_TAG "[DEBUG]"
#define INFO_TAG "[INFO]"
#define WARN_TAG "[WARN]"
#define ERROR_TAG "[ERROR]"
#define START_TAG "[SERVICE STARTING]"
#define STOP_TAG "[SERVICE STOPPING]"

typedef enum {
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARN,
  LOG_ERROR,
  LOG_SERVICE_STARTING,
  LOG_SERVICE_STOPPING
} LogLevel;

#endif // LIB_H
