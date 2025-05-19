#ifndef WATCH_H
#define WATCH_H

/* Add a live file watch to the config file */
const char *start_file_watch(const char *path);

/* Used when polling for events */
int get_watch_descriptor(void);

/* Used to discard inotify events, we
 * don't care about their content, just
 * that they happened */
void consume_watch_event(void);

#endif
