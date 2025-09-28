#ifndef FTN_LOG_H
#define FTN_LOG_H

#include <stdarg.h>
#include "log_levels.h"

/* Logging initialization and cleanup - using void* to avoid circular includes */
void ftn_log_init(const void* config);
void ftn_log_cleanup(void);

/* Logging level management */
ftn_log_level_t ftn_log_get_level(void);
void ftn_log_set_level(ftn_log_level_t level);

/* Logging functions */
void ftn_log(ftn_log_level_t level, const char* message);
void ftn_vlogf(ftn_log_level_t level, const char* format, va_list args);
void ftn_logf(ftn_log_level_t level, const char* format, ...);

/* Convenience functions for mailer */
void ftn_log_debug(const char* format, ...);
void ftn_log_info(const char* format, ...);
void ftn_log_warning(const char* format, ...);
void ftn_log_error(const char* format, ...);
void ftn_log_critical(const char* format, ...);

/* Legacy macros for compatibility */
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