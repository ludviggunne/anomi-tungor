#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>

#include "synthesizer.h"
#include "audio-file.h"

/* Linked list with sink/source info */
struct list {
  unsigned int   index;
  char          *name;
  char          *description;
  struct list   *next;
};

int init_audio(void);

struct list *list_sources(void);

struct list *list_sinks(void);

/* Make streams match audio file in sample rate and number
 * of channels */
void match_audio_file_sample_spec(struct audio_file *af);

int connect_source(const char *name);

int connect_sink(const char *name);

const char *get_audio_error_string(void);

int start_streams(struct synthesizer *syn);

void free_list(struct list *l);

#endif
