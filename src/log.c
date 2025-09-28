#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>

#include "ftn/log.h"
#include "ftn/compat.h"

/* Include config types directly to avoid circular dependencies */
#include "ftn/log_levels.h"

typedef struct {
    char* level_str;
    ftn_log_level_t level;
    int use_syslog;
    char* log_file;
    char* ident;
} ftn_logging_config_t;

static ftn_log_level_t current_log_level = FTN_LOG_INFO;
static int use_syslog = 0;
static FILE* log_file = NULL;
static char* log_ident = NULL;

/* Custom vsyslog implementation */
static void ftn_vsyslog(int priority, const char *format, va_list args) {
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    syslog(priority, "%s", buffer);
}

void ftn_log_init(const void* config_ptr) {
    const ftn_logging_config_t* config = (const ftn_logging_config_t*)config_ptr;
    if (config) {
        current_log_level = config->level;
        use_syslog = config->use_syslog;

        /* Store identity string */
        if (log_ident) {
            free(log_ident);
            log_ident = NULL;
        }
        if (config->ident) {
            log_ident = malloc(strlen(config->ident) + 1);
            if (log_ident) {
                strcpy(log_ident, config->ident);
            }
        }

        /* Open log file if specified */
        if (config->log_file) {
            log_file = fopen(config->log_file, "a");
            if (!log_file) {
                fprintf(stderr, "Warning: Could not open log file %s\n", config->log_file);
            }
        }

        /* Initialize syslog if requested */
        if (use_syslog) {
            openlog(log_ident ? log_ident : "fnmailer", LOG_PID | LOG_CONS, LOG_DAEMON);
        }
    } else {
        /* Default configuration */
        current_log_level = FTN_LOG_INFO;
        use_syslog = 0;
        log_file = NULL;
    }
}

void ftn_log_cleanup(void) {
    if (use_syslog) {
        closelog();
    }

    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }

    if (log_ident) {
        free(log_ident);
        log_ident = NULL;
    }
}

ftn_log_level_t ftn_log_get_level(void) {
    return current_log_level;
}

void ftn_log_set_level(ftn_log_level_t level) {
    current_log_level = level;
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

        /* Use log file if available, otherwise console */
        if (log_file) {
            fprintf(log_file, "[%s] %s: %s\n", timestamp, level_str, message);
            fflush(log_file);
        } else {
            output = (level >= FTN_LOG_ERROR) ? stderr : stdout;
            fprintf(output, "[%s] %s: %s\n", timestamp, level_str, message);
            fflush(output);
        }
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

        /* Use log file if available, otherwise console */
        if (log_file) {
            fprintf(log_file, "[%s] %s: ", timestamp, level_str);
            vfprintf(log_file, format, args);
            fprintf(log_file, "\n");
            fflush(log_file);
        } else {
            output = (level >= FTN_LOG_ERROR) ? stderr : stdout;
            fprintf(output, "[%s] %s: ", timestamp, level_str);
            vfprintf(output, format, args);
            fprintf(output, "\n");
            fflush(output);
        }
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

/* Convenience functions for mailer */
void ftn_log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_DEBUG, format, args);
    va_end(args);
}

void ftn_log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_INFO, format, args);
    va_end(args);
}

void ftn_log_warning(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_WARNING, format, args);
    va_end(args);
}

void ftn_log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_ERROR, format, args);
    va_end(args);
}

void ftn_log_critical(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ftn_vlogf(FTN_LOG_CRITICAL, format, args);
    va_end(args);
}
