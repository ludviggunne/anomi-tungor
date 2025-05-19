#ifndef AUDIO_FILE_H
#define AUDIO_FILE_H

#include <stddef.h>

struct audio_file {
  float        *data;
  unsigned int  size;
  unsigned int  samplerate;
  unsigned int  channels;
};

const char *load_audio_file(const char *path, struct audio_file *af);
void free_audio_file(struct audio_file *af);

#endif
