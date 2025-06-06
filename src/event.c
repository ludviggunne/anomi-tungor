#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <poll.h>

#include "xmalloc.h"
#include "term.h"
#include "watch.h"
#include "event.h"

struct event_queue {
  struct event event;
  struct event_queue *prev;
  struct event_queue *next;
};

static struct pollfd s_pollfds[3];

/* eventfd() is used for custom events so they can
 * be handled by poll() */
static int s_eventfd;

/* Event queue */
static struct event_queue *s_queue_front = NULL;
static struct event_queue *s_queue_back = NULL;

/* Guard the event queue since it's accessed
 * by the threaded PulseAudio mainloop */
static pthread_mutex_t s_lock;

static void cleanup(void)
{
  close(s_eventfd);
}

const char *event_loop_start(const char *watch_path)
{
  const char *err;

  /* Set terminal raw mode so we can read characters
   * without echoing and without pressing enter */
  term_set_raw();

  /* Start watching the configuration file for
   * live updates */
  err = start_file_watch(watch_path);
  if (err != NULL) {
    return err;
  }

  /* Set EFD_SEMAPHORE so eventfd counter matches
   * length of event queue */
  s_eventfd = eventfd(0, EFD_SEMAPHORE);

  /* Set file descriptors to poll */
  s_pollfds[0].fd = get_watch_descriptor();
  s_pollfds[0].events = POLLIN;
  s_pollfds[1].fd = STDIN_FILENO;
  s_pollfds[1].events = POLLIN;
  s_pollfds[2].fd = s_eventfd;
  s_pollfds[2].events = POLLIN;

  atexit(cleanup);

  return NULL;
}

struct event event_loop_poll(void)
{
  struct event event;

  poll(s_pollfds, 3, -1);

  if (s_pollfds[0].revents & POLLIN) {
    event.type = EVENT_WATCH;

    /* Discard the contents of the inotify event */
    consume_watch_event();

    return event;
  }

  if (s_pollfds[1].revents & POLLIN) {
    event.type = EVENT_INPUT;
    event.c = fgetc(stdin);
    return event;
  }

  if (s_pollfds[2].revents & POLLIN) {
    pthread_mutex_lock(&s_lock);

    assert(s_queue_front && "trying to read eventfd with no events in queue");

    /* Discard the eventfd counter value */
    uint64_t event_value;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
    (void) read(s_eventfd, &event_value, sizeof(event_value));
#pragma GCC diagnostic pop

    /* Pop event from queue */
    memcpy(&event, &s_queue_front->event, sizeof(struct event));
    struct event_queue *prev = s_queue_front->prev;
    free(s_queue_front);

    if (s_queue_front == s_queue_back) {
      s_queue_front = s_queue_back = NULL;
    } else {
      s_queue_front = prev;
    }

    pthread_mutex_unlock(&s_lock);

    return event;
  }

  assert(0 && "no events ready");
}

void queue_event(struct event *event)
{
  pthread_mutex_lock(&s_lock);

  struct event_queue *eq = xcalloc(1, sizeof(*eq));
  memcpy(&eq->event, event, sizeof(struct event));
  if (s_queue_back == NULL) {
    s_queue_back = s_queue_front = eq;
  } else {
    eq->next = s_queue_back;
    s_queue_back->prev = eq;
    s_queue_back = eq;
  }

  /* Update eventfd counter so the event is
   * detected in the poll() call */
  uint64_t event_value = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"
  (void) write(s_eventfd, &event_value, sizeof(event_value));
#pragma GCC diagnostic pop

  pthread_mutex_unlock(&s_lock);
}
