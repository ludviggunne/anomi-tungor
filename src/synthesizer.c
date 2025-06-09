#include <string.h>
#include <math.h>
#include <pthread.h>

#include "log.h"
#include "xmalloc.h"
#include "synthesizer.h"

struct slot {
  unsigned int     offset;
  unsigned int     length;
  unsigned int     cooldown;
  unsigned int     cursor;
  float            gain;
  float            multiplier;
  int              reverse;
};

struct synthesizer {
  struct audio_file    *af;
  struct profile        profile;
  struct profile        target_profile;
  struct profile        source_profile;

  struct slot          *slots;
  size_t                num_slots;
  size_t                slots_capac;

  pthread_mutex_t       lock;
  size_t                fcursor;           /* Offset within audio file */

  float                *data;              /* Synthesized samples */
  size_t                data_size;

  float                 interp_time;       /* Time in seconds for profile interpolation */
  unsigned int          interp_counter;    /* Counter used for inteprolating between profiles */
};

struct synthesizer *create_synthesizer(struct audio_file *audio)
{
  struct synthesizer *syn;
  pthread_mutexattr_t mutexattr;
  
  syn = xcalloc(1, sizeof(*syn));
  syn->af = audio;

  syn->num_slots = 0;
  syn->slots_capac = syn->num_slots;
  syn->slots = NULL;

  syn->data_size = 4096;
  syn->data = xcalloc(sizeof(*syn->data), syn->data_size);

  syn->interp_time = 1.f;

  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&syn->lock, &mutexattr);

  return syn;
}

void free_synthesizer(struct synthesizer *syn)
{
  free(syn->slots);
}

void set_synthesizer_profile(struct synthesizer *syn, struct profile *profile, int set_now)
{
  if (set_now) {
    memcpy(&syn->profile, profile, sizeof(struct profile));
  } else {
    memcpy(&syn->target_profile, profile, sizeof(struct profile));
    memcpy(&syn->source_profile, &syn->profile, sizeof(struct profile));
    syn->interp_counter = (unsigned int) (syn->af->samplerate * syn->interp_time);
  }
}

/* Generate a random integer within a range */
static unsigned int randr(unsigned int min, unsigned int max)
{
  return min + rand() % (max - min);
}

/* Generate a random float within a range */
static float randf(float min, float max)
{
  return min + (max - min) * (float) rand() / (float) RAND_MAX;
}

static void init_slot(struct synthesizer *syn, struct slot *slot)
{
  /* Generate a random grain based on configuration */

  unsigned int max_cooldown = syn->profile.max_cooldown * syn->af->samplerate;
  unsigned int min_cooldown = syn->profile.min_cooldown * syn->af->samplerate;
  if (min_cooldown != max_cooldown) {
    slot->cooldown = randr((unsigned int) (syn->profile.min_cooldown * syn->af->samplerate), (unsigned int) (syn->profile.max_cooldown * syn->af->samplerate));
  } else {
    slot->cooldown = min_cooldown;
  }

  /* The offset is converted to an absolute offset within the file */
  unsigned int max_offset = syn->profile.max_offset * syn->af->samplerate;
  unsigned int min_offset = syn->profile.min_offset * syn->af->samplerate;
  if (min_offset != max_offset) {
    slot->offset = randr((unsigned int) (syn->profile.min_offset * syn->af->samplerate), (unsigned int) (syn->profile.max_offset * syn->af->samplerate));
  } else {
    slot->offset = min_offset;
  }
  slot->offset = (syn->af->size + syn->fcursor - slot->offset) % syn->af->size;

  unsigned int max_length = syn->profile.max_length * syn->af->samplerate;
  unsigned int min_length = syn->profile.min_length * syn->af->samplerate;
  if (min_length != max_length) {
    slot->length = randr((unsigned int) (syn->profile.min_length * syn->af->samplerate), (unsigned int) (syn->profile.max_length * syn->af->samplerate));
  } else {
    slot->length = min_length;
  }

  slot->gain = randf(syn->profile.min_gain, syn->profile.max_gain);
  slot->multiplier = randf(syn->profile.min_multiplier, syn->profile.max_multiplier);
  slot->reverse = randf(0.f, 1.f) < syn->profile.reverse_probability;

  slot->cursor = 0;
}

void synthesize(struct synthesizer *syn, size_t length)
{
  if (syn->profile.num_slots > syn->slots_capac) {
    syn->slots = xrealloc(syn->slots, sizeof(*syn->slots) * syn->profile.num_slots);
    syn->slots_capac = syn->profile.num_slots;
  }

  if (syn->profile.num_slots > syn->num_slots) {
    for (unsigned int i = syn->num_slots; i < syn->profile.num_slots; ++i) {
      init_slot(syn, &syn->slots[i]);
    }
  }

  syn->num_slots = syn->profile.num_slots;

  if (length > syn->data_size) {
    syn->data = xrealloc(syn->data, sizeof(*syn->data) * length);
    syn->data_size = length;
  }

  for (size_t i = 0; i < length; ++i) {

    float sample = 0.f;
    float profile_interp = 1.f - (float) syn->interp_counter / (syn->interp_time * (float) syn->af->samplerate);

    for (unsigned int i = 0; i < syn->slots_capac; ++i) {

      struct slot *s = &syn->slots[i];

      /* Cooldown mode while cooldown is non-zero */
      if (s->cooldown) {
        if (i >= syn->num_slots) {
          /* If this grain is supposed to die,
           * don't decrement cooldown counter */
          continue;
        }
        s->cooldown--;
        continue;
      }

      if (s->cursor == s->length) {
        /* Grain finished playing, create a new one */
        init_slot(syn, s);
        continue;
      }

      unsigned int cursor = s->cursor;
      if (s->reverse) {
        cursor = s->length - cursor;
      }

      /* Scale cursor by multiplier */
      float fcursor;
      float interp = modff(s->multiplier * (float) cursor, &fcursor);
      cursor = (unsigned int) fcursor;

      /* Interpolate sample based on fractional part after scaling */
      unsigned int lpos = (syn->af->size + s->offset + cursor) % syn->af->size;
      unsigned int rpos = (lpos + 1) % syn->af->size;

      float lsample = syn->af->data[lpos];
      float rsample = syn->af->data[rpos];

      float af_sample = lsample + (rsample - lsample) * interp;

      /* Compute envelope */
      float t = (float) s->cursor / (float) s->length;
      // float env = 1.f - (2.f * t - 1.f) * (2.f * t - 1.f);
      float env = t < .25f ? 4.f * t : 4.f * (1.f - t) / 3.f;

      /* Scale new/old slots based on configuration interpolation */
      float profile_scaling = 1.f;
      if (syn->source_profile.num_slots < syn->target_profile.num_slots && i > syn->source_profile.num_slots) {
        profile_scaling = profile_interp;
      } else if (syn->source_profile.num_slots > syn->target_profile.num_slots && i > syn->target_profile.num_slots) {
        profile_scaling = 1.f - profile_interp;
      }

      /* Accumulate sample */
      sample += af_sample * env * s->gain * profile_scaling;
      s->cursor++;
    }

    syn->fcursor++;
    if (syn->fcursor == syn->af->size) {
      syn->fcursor = 0;
    }

    syn->data[i] = sample;

    /* Interpolate between profiles */
    if (syn->interp_counter) {

      syn->interp_counter--;

      float num_slots_interp = ((float) syn->source_profile.num_slots + profile_interp * ((float) syn->target_profile.num_slots - (float) syn->source_profile.num_slots));
      syn->profile.num_slots = (unsigned int) (num_slots_interp < .0f ? .0f : num_slots_interp);
      syn->profile.min_offset = syn->source_profile.min_offset + profile_interp * (syn->target_profile.min_offset - syn->source_profile.min_offset);
      syn->profile.max_offset = syn->source_profile.max_offset + profile_interp * (syn->target_profile.max_offset - syn->source_profile.max_offset);
      syn->profile.min_length = syn->source_profile.min_length + profile_interp * (syn->target_profile.min_length - syn->source_profile.min_length);
      syn->profile.max_length = syn->source_profile.max_length + profile_interp * (syn->target_profile.max_length - syn->source_profile.max_length);
      syn->profile.min_cooldown = syn->source_profile.min_cooldown + profile_interp * (syn->target_profile.min_cooldown - syn->source_profile.min_cooldown);
      syn->profile.max_cooldown = syn->source_profile.max_cooldown + profile_interp * (syn->target_profile.max_cooldown - syn->source_profile.max_cooldown);
      syn->profile.min_multiplier = syn->source_profile.min_multiplier + profile_interp * (syn->target_profile.min_multiplier - syn->source_profile.min_multiplier);
      syn->profile.max_multiplier = syn->source_profile.max_multiplier + profile_interp * (syn->target_profile.max_multiplier - syn->source_profile.max_multiplier);
      syn->profile.min_gain = syn->source_profile.min_gain + profile_interp * (syn->target_profile.min_gain - syn->source_profile.min_gain);
      syn->profile.max_gain = syn->source_profile.max_gain + profile_interp * (syn->target_profile.max_gain - syn->source_profile.max_gain);
      syn->profile.reverse_probability = syn->source_profile.reverse_probability + profile_interp * (syn->target_profile.reverse_probability - syn->source_profile.reverse_probability);

      if (syn->interp_counter == 0) {
        log_info("Done interpolating/fading");
      }
    }
  }
}

float *synthesizer_get_data_ptr(struct synthesizer *syn)
{
  return syn->data;
}

void lock_synthesizer(struct synthesizer *syn)
{
  pthread_mutex_lock(&syn->lock);
}

void unlock_synthesizer(struct synthesizer *syn)
{
  pthread_mutex_unlock(&syn->lock);
}

void sythesizer_fade_out(struct synthesizer *syn)
{
  memcpy(&syn->source_profile, &syn->profile, sizeof(struct profile));
  memcpy(&syn->target_profile, &syn->profile, sizeof(struct profile));
  syn->target_profile.min_gain = 0.f;
  syn->target_profile.max_gain = 0.f;
  syn->interp_counter = (unsigned int) (syn->af->samplerate * syn->interp_time);
}

void sythesizer_set_interp_time(struct synthesizer *syn, float t)
{
  syn->interp_time = t;
}
