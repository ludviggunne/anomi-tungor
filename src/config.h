#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

struct config {
  float          level;
  float          min_offset;
  float          max_offset;
  float          min_length;
  float          max_length;
  float          min_cooldown;
  float          max_cooldown;
  float          min_gain;
  float          max_gain;
  float          min_multiplier;
  float          max_multiplier;
  float          reverse_probability;
  unsigned int   num_slots;
};

struct config_list {
  struct config *cfgs;
  size_t         size;
};

const char *load_config_list(const char *path, struct config_list *cl);
void free_config_list(struct config_list *cl);

#endif
