#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

/* Single configuration, the configuration file
 * can contain multiple of these */
struct config {
  char          *name;                   /* The name of this configuration */
  float          level;                  /* The volume level where this configuration is activated */
  float          min_offset;             /* Minimum sample offset in audio file in seconds */
  float          max_offset;             /* Maximum sample offset in audio file in seconds */
  float          min_length;             /* Minimum length of grain in seconds */
  float          max_length;             /* Maximum length of grain in seconds */
  float          min_cooldown;           /* Minimum number of seconds before the grain is activated */
  float          max_cooldown;           /* Minimum number of seconds before the grain is activated */
  float          min_gain;               /* Minimum gain */
  float          max_gain;               /* Maximum gain */
  float          min_multiplier;         /* Minimum time scaling factor */
  float          max_multiplier;         /* Maximum time scaling factor */
  float          reverse_probability;    /* The probability that a single grain will be played back in reverse */
  unsigned int   num_slots;              /* The number of active grains */
};

/* List of configurations, this corresponds
 * to the configuration file */
struct config_list {
  struct config *cfgs;
  size_t         size;
};

const char *load_config_list(const char *path, struct config_list *cl);
void free_config_list(struct config_list *cl);

#endif
