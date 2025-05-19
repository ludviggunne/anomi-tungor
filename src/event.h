#ifndef EVENT_H
#define EVENT_H

enum event_type {
  EVENT_INPUT,
  EVENT_WATCH,
  EVENT_VOLUME,
};

struct event {
  enum event_type type;
  int c;
  float volume;
};

const char *event_loop_start(const char *watch_path);
struct event event_loop_poll(void);
void queue_event(struct event *event);

#endif
