#ifndef EVENT_H
#define EVENT_H

enum event_type {
  EVENT_INPUT,    /* User pressed a key */
  EVENT_WATCH,    /* Config file was modified */
  EVENT_MIDI,
};

struct event {
  enum event_type type;   /* The type of this event */
  int c;                  /* Character for input events */
  int pitch;              /* Midi event pitch class */
  int on;                 /* Midi on/off */
};

const char *event_loop_start(const char *watch_path);

/* Poll for events */
struct event event_loop_poll(void);

/* Add an event to the event queue */
void queue_event(struct event *event);

#endif
