#ifndef LOG_H
#define LOG_H

void log_init(void);
void log_err(const char *fmt, ...);
void log_info(const char *fmt, ...);
void log_warn(const char *fmt, ...);
void log_info_hl(const char *fmt, ...);

#endif
