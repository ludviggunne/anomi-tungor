#ifndef WATCH_H
#define WATCH_H

const char *start_file_watch(const char *path);
int get_watch_descriptor(void);
void consume_watch_event(void);

#endif
