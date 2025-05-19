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
  struct config *       cfg;

  struct slot *         slots;
  size_t                num_slots;
  size_t                slots_capac;

  pthread_mutex_t       lock;
  size_t                fcursor;

  float                *data;
  size_t                data_size;
};

struct synthesizer *create_synthesizer(struct audio_file *audio)
{
  struct synthesizer *syn;
  pthread_mutexattr_t mutexattr;
  
  syn = xcalloc(1, sizeof(*syn));
  syn->af = audio;
  syn->cfg = NULL;

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

void set_synthesizer_config(struct synthesizer *syn, struct config *cfg)
{
  syn->cfg = cfg;
}

static unsigned int randr(unsigned int min, unsigned int max)
{
  return min + rand() % (max - min);
}

static float randf(float min, float max)
{
  return min + (max - min) * (float) rand() / (float) RAND_MAX;
}

static void init_slot(struct synthesizer *syn, struct slot *slot)
{
  slot->cooldown   = randr((unsigned int) (syn->cfg->min_cooldown * syn->af->samplerate),   (unsigned int) (syn->cfg->max_cooldown * syn->af->samplerate));
  slot->offset     = randr((unsigned int) (syn->cfg->min_offset * syn->af->samplerate),     (unsigned int) (syn->cfg->max_offset * syn->af->samplerate));
  slot->offset     = (syn->af->size + syn->fcursor - slot->offset) % syn->af->size;
  slot->length     = randr((unsigned int) (syn->cfg->min_length * syn->af->samplerate),     (unsigned int) (syn->cfg->max_length * syn->af->samplerate));
  slot->gain       = randf(syn->cfg->min_gain, syn->cfg->max_gain);
  slot->multiplier = randf(syn->cfg->min_multiplier, syn->cfg->max_multiplier);
  slot->reverse    = randf(0.f, 1.f) < syn->cfg->reverse_probability;
  slot->cursor     = 0;
}

void synthesize(struct synthesizer *syn, size_t length)
{
  if (syn->cfg->num_slots > syn->slots_capac) {
    log_info("Making room for %d slots", syn->cfg->num_slots);
    syn->slots = xrealloc(syn->slots, sizeof(*syn->slots) * syn->cfg->num_slots);
    syn->slots_capac = syn->cfg->num_slots;
  }

  if (syn->cfg->num_slots > syn->num_slots) {
    log_info("Initializing %d new slots", syn->cfg->num_slots - syn->num_slots);
    for (unsigned int i = syn->num_slots; i < syn->cfg->num_slots; ++i) {
      init_slot(syn, &syn->slots[i]);
    }
  }

  syn->num_slots = syn->cfg->num_slots;

  if (length > syn->data_size) {
    log_info("Growing synthesizer buffer to %d samples", length);
    syn->data = xrealloc(syn->data, sizeof(*syn->data) * length);
    syn->data_size = length;
  }

  for (size_t i = 0; i < length; ++i) {

    float sample = 0.f;

    for (unsigned int i = 0; i < syn->slots_capac; ++i) {

      struct slot *s = &syn->slots[i];

      if (s->cooldown) {
        if (i >= syn->num_slots)
          continue;
        s->cooldown--;
        continue;
      }

      if (s->cursor == s->length) {
        init_slot(syn, s);
        continue;
      }

      unsigned int cursor = s->cursor;
      if (s->reverse) {
        cursor = s->length - cursor;
      }

      float fcursor;
      float interp = modff(s->multiplier * (float) cursor, &fcursor);
      cursor = (unsigned int) fcursor;

      unsigned int lpos = (syn->af->size + s->offset + cursor) % syn->af->size;
      unsigned int rpos = (lpos + 1) % syn->af->size;

      float lsample = syn->af->data[lpos];
      float rsample = syn->af->data[rpos];

      float af_sample = lsample + (rsample - lsample) * interp;

      float t = (float) s->cursor / (float) s->length;
      float env = 1.f - (2.f * t - 1.f) * (2.f * t - 1.f);

      sample += af_sample * env * s->gain;
      s->cursor++;
    }

    syn->fcursor++;
    if (syn->fcursor == syn->af->size) {
      syn->fcursor = 0;
    }

    syn->data[i] = sample;
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
