#ifndef AUDIO_FILE_H
#define AUDIO_FILE_H

#include <stddef.h>

struct audio_file {
  float        *data;
  unsigned int  size; /* Number of samples */
  unsigned int  samplerate;
  unsigned int  channels;
};

/* Load an audio file from disk.
 * Returns NULL or an error string. */
const char *load_audio_file(const char *path, struct audio_file *af);

void free_audio_file(struct audio_file *af);

#endif
