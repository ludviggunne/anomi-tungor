#ifndef SYNTHESIZER_H
#define SYNTHESIZER_H

#include "audio-file.h"
#include "config.h"

struct synthesizer;

/* Profile must be set with set_synthesizer_config */
struct synthesizer *create_synthesizer(struct audio_file *audio);

void free_synthesizer(struct synthesizer *syn);

void set_synthesizer_profile(struct synthesizer *syn, struct profile *profile, int set_now);

/* Synthesize length samples */
void synthesize(struct synthesizer *syn, size_t length);

/* Get a pointer to synthesized samples */
float *synthesizer_get_data_ptr(struct synthesizer *syn);

void sythesizer_fade_out(struct synthesizer *syn);

void sythesizer_set_interp_time(struct synthesizer *syn, float t);

/* Acquire/release lock on synthesizer (accessed in
 * threaded PulseAudio mainloop) */
void lock_synthesizer(struct synthesizer *syn);
void unlock_synthesizer(struct synthesizer *syn);

void synthesizer_note_on(struct synthesizer *syn, int pitch_class);
void synthesizer_note_off(struct synthesizer *syn, int pitch_class);

#endif
