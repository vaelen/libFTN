#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>

#include "ftn/log.h"
#include "ftn/compat.h"

static ftn_log_level_t current_log_level = FTN_LOG_INFO;
static int use_syslog = 0;

/* Custom vsyslog implementation */
static void ftn_vsyslog(int priority, const char *format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    syslog(priority, "%s", buffer);
}

void ftn_log_init(ftn_log_level_t level, int use_syslog_flag, const char* ident) {
    current_log_level = level;
    use_syslog = use_syslog_flag;

    if (use_syslog) {
        openlog(ident ? ident : "fntosser", LOG_PID | LOG_CONS, LOG_DAEMON);
    }
}

void ftn_log_close(void) {
    if (use_syslog) {
        closelog();
    }
}

static void get_log_level_str(ftn_log_level_t level, const char** level_str, int* syslog_level) {
    switch (level) {
        case FTN_LOG_DEBUG:
            *level_str = "DEBUG";
            *syslog_level = LOG_DEBUG;
            break;
        case FTN_LOG_INFO:
            *level_str = "INFO";
            *syslog_level = LOG_INFO;
            break;
        case FTN_LOG_WARNING:
            *level_str = "WARNING";
            *syslog_level = LOG_WARNING;
            break;
        case FTN_LOG_ERROR:
            *level_str = "ERROR";
            *syslog_level = LOG_ERR;
            break;
        case FTN_LOG_CRITICAL:
            *level_str = "CRITICAL";
            *syslog_level = LOG_CRIT;
            break;
        default:
            *level_str = "UNKNOWN";
            *syslog_level = LOG_INFO;
            break;
    }
}

void ftn_log(ftn_log_level_t level, const char* message) {
    const char* level_str;
    int syslog_level;
    FILE* output;
    time_t now;
    struct tm* tm_info;
    char timestamp[32];

    if (level < current_log_level) {
        return;
    }

    get_log_level_str(level, &level_str, &syslog_level);

    if (use_syslog) {
        syslog(syslog_level, "%s", message);
    } else {
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        output = (level >= FTN_LOG_ERROR) ? stderr : stdout;
        fprintf(output, "[%s] %s: %s\n", timestamp, level_str, message);
        fflush(output);
    }
}

void ftn_vlogf(ftn_log_level_t level, const char* format, va_list args) {
    const char* level_str;
    int syslog_level;
    FILE* output;
    time_t now;
    struct tm* tm_info;
    char timestamp[32];

    if (level < current_log_level) {
        return;
    }

    get_log_level_str(level, &level_str, &syslog_level);

    if (use_syslog) {
        ftn_vsyslog(syslog_level, format, args);
    } else {
        time(&now);
        tm_info = localtime(&now);
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        output = (level >= FTN_LOG_ERROR) ? stderr : stdout;
        fprintf(output, "[%s] %s: ", timestamp, level_str);
        vfprintf(output, format, args);
        fprintf(output, "\n");
        fflush(output);
    }
}

void ftn_logf(ftn_log_level_t level, const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(level, format, args);
    va_end(args);
}

void logf_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_DEBUG, format, args);
    va_end(args);
}

void logf_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_INFO, format, args);
    va_end(args);
}

void logf_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_WARNING, format, args);
    va_end(args);
}

void logf_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_ERROR, format, args);
    va_end(args);
}

void logf_critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_CRITICAL, format, args);
    va_end(args);
}
