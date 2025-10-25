#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

typedef enum LoggerLevel {
    LogTrace = 0,
    LogInfo,
    LogWarn,
    LogError,
    LogDisable,
} LoggerLevel;

void logger_init(const char *path);
void logger_set_level(LoggerLevel lvl);
void logger_set_output(const char *path);
void log_log(LoggerLevel lvl, const char *file, int line, const char *fmt, ...);
void logger_stop();

#define TRACE(...) log_log(LogTrace, __FILENAME__, __LINE__, __VA_ARGS__)
#define INFO(...) log_log(LogInfo, __FILENAME__, __LINE__, __VA_ARGS__)
#define WARN(...) log_log(LogWarn, __FILENAME__, __LINE__, __VA_ARGS__)
#define ERROR(...) log_log(LogError, __FILENAME__, __LINE__, __VA_ARGS__)

#endif /* LOG_H */
