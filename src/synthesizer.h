#ifndef SYNTHESIZER_H
#define SYNTHESIZER_H

#include "audio-file.h"
#include "config.h"

struct synthesizer;

/* Configuration must be set with set_synthesizer_config */
struct synthesizer *create_synthesizer(struct audio_file *audio);

void free_synthesizer(struct synthesizer *syn);

void set_synthesizer_config(struct synthesizer *syn, struct config *cfg, int set_now);

/* Synthesize length samples */
void synthesize(struct synthesizer *syn, size_t length);

/* Get a pointer to synthesized samples */
float *synthesizer_get_data_ptr(struct synthesizer *syn);

/* Acquire/release lock on synthesizer (accessed in
 * threaded PulseAudio mainloop) */
void lock_synthesizer(struct synthesizer *syn);
void unlock_synthesizer(struct synthesizer *syn);

#endif
