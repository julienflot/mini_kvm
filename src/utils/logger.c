#include "logger.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct LoggerState {
    FILE *output;
    bool enable_color;
    LoggerLevel level;
    time_t timer;
    pthread_mutex_t lock;
} LoggerState;

static const char *level_str[] = {
    "TRACE",
    "INFO ",
    "WARN ",
    "ERROR",
};

static const char *level_color[] = {
    "\e[1;34m", // TRACE = blue
    "\e[1;32m", // INFO = green
    "\e[1;33m", // WARN = yellow
    "\e[1;31m", // ERROR = red
    "\e[0m",    // reset
};

static LoggerState state = {
    .output = NULL,
    .enable_color = true,
    .level = LogTrace,
    .lock = PTHREAD_MUTEX_INITIALIZER,
};

static LoggerLevel parse_log_env() {
    const char *char_level = getenv("LOGGER_LEVEL");
    LoggerLevel level = LogTrace;

    if (char_level == NULL) {
        return level;
    }

    if (!strncmp(char_level, "INFO", 4)) {
        level = LogInfo;
    } else if (!strncmp(char_level, "TRACE", 5)) {
        level = LogTrace;
    } else if (!strncmp(char_level, "WARN", 4)) {
        level = LogWarn;
    } else if (!strncmp(char_level, "ERROR", 5)) {
        level = LogError;
    } else if (!strncmp(char_level, "DISABLE", 7)) {
        level = LogDisable;
    }

    return level;
}

void logger_init(const char *path) {
    pthread_mutex_lock(&state.lock);
    if (path != NULL && strlen(path) != 0) {
        state.output = fopen(path, "w");
        state.enable_color = false;
    } else {
        state.output = stdout;
    }
    state.level = parse_log_env();
    state.timer = time(NULL);
    pthread_mutex_unlock(&state.lock);
}

void logger_set_level(LoggerLevel level) {
    pthread_mutex_lock(&state.lock);
    state.level = level;
    pthread_mutex_unlock(&state.lock);
}

void log_log(LoggerLevel lvl, const char *file, int line, const char *fmt, ...) {
    pthread_mutex_lock(&state.lock);

    if (lvl < state.level) {
        pthread_mutex_unlock(&state.lock);
        return;
    }

    // format level string
    char level_buf[21];
    if (state.enable_color) {
        sprintf(level_buf, "%s%s%s", level_color[lvl], level_str[lvl], level_color[4]);
    } else {
        sprintf(level_buf, "%s", level_str[lvl]);
    }

    // format time string
    char time_buf[13];
    state.timer = time(NULL);
    struct tm *tm_info = localtime(&state.timer);
    strftime(time_buf, 26, "%H:%M:%S", tm_info);

    // format metadata string
    fprintf(state.output, "[%s] %s %s:%d ", time_buf, level_buf, file, line);

    // add user info
    va_list args;
    va_start(args, fmt);
    vfprintf(state.output, fmt, args);
    va_end(args);

    fprintf(state.output, "\n");
    fflush(state.output);

    pthread_mutex_unlock(&state.lock);
}

void logger_stop() {
    if (state.output != stdout) {
        fclose(state.output);
    }
}
