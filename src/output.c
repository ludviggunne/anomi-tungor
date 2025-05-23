#include <sys/stat.h>
#include <stdlib.h>
#include <sndfile.h>
#include <stdio.h>
#include <string.h>

#include "output.h"
#include "log.h"

static const char *s_name;
static char s_errorbuf[512] = {0};
static SNDFILE *s_file = NULL;

static void cleanup(void)
{
  log_info("Closing output file %s", s_name);
  sf_close(s_file);
}

const char *open_output_file(const char *name, struct audio_file *input_file)
{
  struct stat st;

  if (stat(name, &st) == 0) {
    log_info("Output file %s already exists. Do you want to overwrite it? (y/n)", name);

    char c;
    while (strchr("ynqYNQ", (c = fgetc(stdin))) == NULL)
      ;

    switch (c) {
    case 'n':
    case 'N':
      log_info("Will not write to file %s", name);
      return NULL;
    case 'q':
    case 'Q':
      return "Aborted";
    default:
      break;
    }
  }

  SF_INFO info = {
    .channels = input_file->channels,
    .samplerate = input_file->samplerate,
    .format = SF_FORMAT_PCM_24 | SF_FORMAT_WAV,
  };

  s_file = sf_open(name, SFM_WRITE, &info);

  if (s_file == NULL) {
    const char *err = sf_strerror(NULL);
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Failed to open output file %s: %s",
             name, err);
    return s_errorbuf;
  }

  log_info("Output file is %s", name);
  atexit(cleanup);
  s_name = name;

  return NULL;
}

void write_to_output_file(float *data, size_t size)
{
  if (s_file == NULL) {
    return;
  }

  size_t offset = 0;
  while (offset < size) {
    offset += sf_write_float(s_file, data + offset, size - offset);
  }
}
