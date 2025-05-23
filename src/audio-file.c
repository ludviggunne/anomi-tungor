#include <string.h>
#include <stdlib.h>
#include <sndfile.h>

#include "log.h"
#include "xmalloc.h"
#include "audio-file.h"

const char *load_audio_file(const char *path, struct audio_file *af)
{
  SF_INFO info = {0};
  SNDFILE *file = NULL;
  unsigned int offset;

  memset(af, 0, sizeof(*af));

  file = sf_open(path, SFM_READ, &info);
  if (file == NULL) {
    return sf_strerror(file);
  }

  af->size = info.channels * info.frames;
  af->data = xmalloc(sizeof(*af->data) * af->size);
  af->channels = info.channels;
  af->samplerate = info.samplerate;

  offset = 0;
  while (offset < af->size) {
    offset += sf_read_float(file, af->data + offset, af->size - offset);
  }

  sf_close(file);

  if (af->channels > 1) {
    /* Mix channels */
    log_info("Audio file has %d channels, mixing them in to one", af->channels);
    af->size /= af->channels;
    for (size_t i = 0; i < af->size; ++i) {
      float a = 0.f;
      for (size_t c = 0; c < af->channels; ++c) {
        a += af->data[af->channels * i + c];
      }
      af->data[i] = a / af->channels;
    }
    af->channels = 1;
  }

  return NULL;
}

void free_audio_file(struct audio_file *af)
{
  free(af->data);
  memset(af, 0, sizeof(*af));
}
