#ifndef FTN_LOG_H
#define FTN_LOG_H

#include <stdarg.h>
#include "log_levels.h"

/* Logging functions */
void ftn_log(ftn_log_level_t level, const char* message);
void ftn_vlogf(ftn_log_level_t level, const char* format, va_list args);
void ftn_logf(ftn_log_level_t level, const char* format, ...);

#define log_debug(message) ftn_log(FTN_LOG_DEBUG, message)
#define log_info(message) ftn_log(FTN_LOG_INFO, message)
#define log_warning(message) ftn_log(FTN_LOG_WARNING, message)
#define log_error(message) ftn_log(FTN_LOG_ERROR, message)
#define log_critical(message) ftn_log(FTN_LOG_CRITICAL, message)

void logf_debug(const char* format, ...);
void logf_info(const char* format, ...);
void logf_warning(const char* format, ...);
void logf_error(const char* format, ...);
void logf_critical(const char* format, ...);

#endif /* FTN_LOG_H */