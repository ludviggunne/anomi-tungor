#ifndef SYNTHESIZER_H
#define SYNTHESIZER_H

#include "audio-file.h"
#include "config.h"

struct synthesizer;

struct synthesizer *create_synthesizer(struct audio_file *audio);
void free_synthesizer(struct synthesizer *syn);
void set_synthesizer_config(struct synthesizer *syn, struct config *cfg);
void synthesize(struct synthesizer *syn, size_t length);
float *synthesizer_get_data_ptr(struct synthesizer *syn);
void lock_synthesizer(struct synthesizer *syn);
void unlock_synthesizer(struct synthesizer *syn);

#endif
