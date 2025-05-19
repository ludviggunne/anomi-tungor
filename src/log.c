#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>

#include "log.h"

/* Some ANSI escape codes for terminal colors */
__attribute__((unused))
static const char *s_red   = "\x1b[31m";
__attribute__((unused))
static const char *s_green = "\x1b[32m";
__attribute__((unused))
static const char *s_yellow  = "\x1b[33m";
__attribute__((unused))
static const char *s_blue  = "\x1b[34m";
__attribute__((unused))
static const char *s_reset = "\x1b[0m";

static pthread_mutex_t s_lock;

void log_init(void)
{
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutex_init(&s_lock, &attr);
}

void log_err(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  pthread_mutex_lock(&s_lock);

  printf("%s[error]:   ", s_red);
  vprintf(fmt, ap);
  printf("%s\r\n", s_reset);
  va_end(ap);

  pthread_mutex_unlock(&s_lock);
}

void log_info(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  pthread_mutex_lock(&s_lock);

  printf("[info]:    ");
  vprintf(fmt, ap);
  printf("%s\r\n", s_reset);
  va_end(ap);

  pthread_mutex_unlock(&s_lock);
}

void log_warn(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  pthread_mutex_lock(&s_lock);

  printf("%s[warning]: ", s_yellow);
  vprintf(fmt, ap);
  printf("%s\r\n", s_reset);
  va_end(ap);

  pthread_mutex_unlock(&s_lock);
}

void log_info_hl(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);

  pthread_mutex_lock(&s_lock);

  printf("%s[info]:    ", s_blue);
  vprintf(fmt, ap);
  printf("%s\r\n", s_reset);
  va_end(ap);

  pthread_mutex_unlock(&s_lock);
}
