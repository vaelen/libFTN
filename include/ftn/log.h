#ifndef FTN_LOG_H
#define FTN_LOG_H

#include <stdarg.h>

#include "log_levels.h"

/* Logging functions */
void ftn_log_init(ftn_log_level_t level, int use_syslog, const char* ident);
void ftn_log_close(void);
void ftn_log(ftn_log_level_t level, const char* format, ...);

/* Convenience macros for logging */
#define log_debug(fmt, ...) ftn_log(FTN_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define log_info(fmt, ...) ftn_log(FTN_LOG_INFO, fmt, ##__VA_ARGS__)
#define log_warning(fmt, ...) ftn_log(FTN_LOG_WARNING, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) ftn_log(FTN_LOG_ERROR, fmt, ##__VA_ARGS__)
#define log_critical(fmt, ...) ftn_log(FTN_LOG_CRITICAL, fmt, ##__VA_ARGS__)

#endif /* FTN_LOG_H */
