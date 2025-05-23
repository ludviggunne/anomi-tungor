#ifndef OUTPUT_H
#define OUTPUT_H

#include <stddef.h>

#include "audio-file.h"

const char *open_output_file(const char *name, struct audio_file *input_file);
void write_to_output_file(float *data, size_t size);

#endif
