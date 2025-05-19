#include <string.h>
#include <math.h>
#include <pthread.h>

#include "log.h"
#include "xmalloc.h"
#include "synthesizer.h"

static const float s_interp_length = 4.f;

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
  struct config        cfg;
  struct config        target_cfg;
  struct config        source_cfg;

  struct slot          *slots;
  size_t                num_slots;
  size_t                slots_capac;

  pthread_mutex_t       lock;
  size_t                fcursor;           /* Offset within audio file */

  float                *data;              /* Synthesized samples */
  size_t                data_size;

  unsigned int          interp_counter;    /* Counter used for inteprolating between configurations */
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

  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&syn->lock, &mutexattr);

  return syn;
}

void free_synthesizer(struct synthesizer *syn)
{
  free(syn->slots);
}

void set_synthesizer_config(struct synthesizer *syn, struct config *cfg, int set_now)
{
  if (set_now) {
    memcpy(&syn->cfg, cfg, sizeof(struct config));
  } else {
    memcpy(&syn->target_cfg, cfg, sizeof(struct config));
    memcpy(&syn->source_cfg, &syn->cfg, sizeof(struct config));
    syn->interp_counter = (unsigned int) (syn->af->samplerate * (float) s_interp_length);
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

  slot->cooldown   = randr((unsigned int) (syn->cfg.min_cooldown * syn->af->samplerate),   (unsigned int) (syn->cfg.max_cooldown * syn->af->samplerate));

  /* The offset is converted to an absolute offset within the file */
  slot->offset     = randr((unsigned int) (syn->cfg.min_offset * syn->af->samplerate),     (unsigned int) (syn->cfg.max_offset * syn->af->samplerate));
  slot->offset     = (syn->af->size + syn->fcursor - slot->offset) % syn->af->size;

  slot->length     = randr((unsigned int) (syn->cfg.min_length * syn->af->samplerate),     (unsigned int) (syn->cfg.max_length * syn->af->samplerate));

  slot->gain       = randf(syn->cfg.min_gain, syn->cfg.max_gain);
  slot->multiplier = randf(syn->cfg.min_multiplier, syn->cfg.max_multiplier);
  slot->reverse    = randf(0.f, 1.f) < syn->cfg.reverse_probability;

  slot->cursor     = 0;
}

void synthesize(struct synthesizer *syn, size_t length)
{
  if (syn->cfg.num_slots > syn->slots_capac) {
    syn->slots = xrealloc(syn->slots, sizeof(*syn->slots) * syn->cfg.num_slots);
    syn->slots_capac = syn->cfg.num_slots;
  }

  if (syn->cfg.num_slots > syn->num_slots) {
    for (unsigned int i = syn->num_slots; i < syn->cfg.num_slots; ++i) {
      init_slot(syn, &syn->slots[i]);
    }
  }

  syn->num_slots = syn->cfg.num_slots;

  if (length > syn->data_size) {
    syn->data = xrealloc(syn->data, sizeof(*syn->data) * length);
    syn->data_size = length;
  }

  for (size_t i = 0; i < length; ++i) {

    float sample = 0.f;
    float cfg_interp = 1.f - (float) syn->interp_counter / (s_interp_length * (float) syn->af->samplerate);

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

      /* Accumulate sample */
      sample += af_sample * env * s->gain;
      s->cursor++;
    }

    syn->fcursor++;
    if (syn->fcursor == syn->af->size) {
      syn->fcursor = 0;
    }

    syn->data[i] = sample;

    /* Interpolate between configurations */
    if (syn->interp_counter) {

      syn->interp_counter--;

      syn->cfg.num_slots = (unsigned int) ((float) syn->source_cfg.num_slots + cfg_interp * ((float) syn->target_cfg.num_slots - (float) syn->source_cfg.num_slots));
      syn->cfg.min_offset = syn->source_cfg.min_offset + cfg_interp * (syn->target_cfg.min_offset - syn->source_cfg.min_offset);
      syn->cfg.max_offset = syn->source_cfg.max_offset + cfg_interp * (syn->target_cfg.max_offset - syn->source_cfg.max_offset);
      syn->cfg.min_length = syn->source_cfg.min_length + cfg_interp * (syn->target_cfg.min_length - syn->source_cfg.min_length);
      syn->cfg.max_length = syn->source_cfg.max_length + cfg_interp * (syn->target_cfg.max_length - syn->source_cfg.max_length);
      syn->cfg.min_cooldown = syn->source_cfg.min_cooldown + cfg_interp * (syn->target_cfg.min_cooldown - syn->source_cfg.min_cooldown);
      syn->cfg.max_cooldown = syn->source_cfg.max_cooldown + cfg_interp * (syn->target_cfg.max_cooldown - syn->source_cfg.max_cooldown);
      syn->cfg.min_multiplier = syn->source_cfg.min_multiplier + cfg_interp * (syn->target_cfg.min_multiplier - syn->source_cfg.min_multiplier);
      syn->cfg.max_multiplier = syn->source_cfg.max_multiplier + cfg_interp * (syn->target_cfg.max_multiplier - syn->source_cfg.max_multiplier);
      syn->cfg.min_gain = syn->source_cfg.min_gain + cfg_interp * (syn->target_cfg.min_gain - syn->source_cfg.min_gain);
      syn->cfg.max_gain = syn->source_cfg.max_gain + cfg_interp * (syn->target_cfg.max_gain - syn->source_cfg.max_gain);
      syn->cfg.reverse_probability = syn->source_cfg.reverse_probability + cfg_interp * (syn->target_cfg.reverse_probability - syn->source_cfg.reverse_probability);
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
