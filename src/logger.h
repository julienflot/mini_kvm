#ifndef LOG_H
#define LOG_H

#include <stdarg.h>

typedef enum LoggerLevel {
    LogTrace = 0,
    LogInfo,
    LogWarn,
    LogError,
    LogDisable,
} LoggerLevel;

void logger_init(const char *path);
void logger_set_level(LoggerLevel lvl);
void log_log(LoggerLevel lvl, const char *file, int line, const char *fmt, ...);
void logger_stop();

#define TRACE(...) log_log(LogTrace, __FILE__, __LINE__, __VA_ARGS__)
#define INFO(...) log_log(LogInfo, __FILE__, __LINE__, __VA_ARGS__)
#define WARN(...) log_log(LogWarn, __FILE__, __LINE__, __VA_ARGS__)
#define ERROR(...) log_log(LogError, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOG_H */
