#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>
#include <sys/inotify.h>

#include "watch.h"
#include "log.h"

static char s_errorbuf[512];
static int s_inotifyfd;
static int s_watchfd = 0;

static void cleanup(void)
{
  close(s_watchfd);
  close(s_inotifyfd);
}

const char *start_file_watch(const char *path)
{
  int flags, mask;

  flags = IN_NONBLOCK;
  s_inotifyfd = inotify_init1(flags);
  if (s_inotifyfd < 0) {
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "inotify_init failed: %s",
             strerror(errno));
    return s_errorbuf;
  }

  mask = IN_CLOSE_WRITE;
  s_watchfd = inotify_add_watch(s_inotifyfd, path, mask);
  if (s_watchfd < 0) {
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Failed to watch file %s: %s",
             path, strerror(errno));
    return s_errorbuf;
  }

  atexit(cleanup);

  return NULL;
}

int get_watch_descriptor(void)
{
  return s_inotifyfd;
}

void consume_watch_event(void)
{
  char eventbuf[sizeof(struct inotify_event) + NAME_MAX + 1];
  while (read(s_inotifyfd, eventbuf, sizeof(eventbuf)) > 0);
}
