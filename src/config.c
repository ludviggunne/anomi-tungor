#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <cjson/cJSON.h>

#include "log.h"
#include "xmalloc.h"
#include "config.h"

static char s_errorbuf[512] = {0};

static void default_config(struct config *cfg)
{

  /* Default configuration. Any field that is not
   * specified in the configuration file is filled
   * in with a value from here */

  cfg->name                = NULL;
  cfg->level               = 0.f;
  cfg->min_offset          = .01f;
  cfg->max_offset          = 2.f;
  cfg->min_length          = .01f;
  cfg->max_length          = 2.f;
  cfg->min_cooldown        = .01f;
  cfg->max_cooldown        = 2.f;
  cfg->min_gain            = 0.01f;
  cfg->max_gain            = 1.f;
  cfg->min_multiplier      = 0.8408964152537144;
  cfg->max_multiplier      = 1.1892071150027212f;
  cfg->reverse_probability = .1f;
  cfg->num_slots           = 8;
}

/* Just load a text file in to a string */
static char *load_file(const char *path)
{
  FILE *file;
  char *text;
  size_t offset, size;

  file = fopen(path, "r");
  if (file == NULL) {
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Unable to open file %s: %s\n",
             path, strerror(errno));
    return NULL;
  }

  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);

  text = xmalloc(size + 1);
  offset = 0;
  while (offset < size) {
    offset += fread(text + offset, 1, size - offset, file);
  }

  fclose(file);
  text[offset] = 0;
  return text;
}


/* Compute the line number for parse errors */
static unsigned long get_lineno(const char *text, const char *offset)
{
  unsigned int lineno = 1;

  for (; text < offset; text++) {
    if (*text == '\n') {
      lineno++;
    }
  }

  return lineno;
}

/* Convenience macro for loading a configuration value */
#define LOAD_VALUE(obj, cfg, name)\
{\
  cJSON *item;\
  if ((item = cJSON_GetObjectItem(obj, #name))) {\
    if (!cJSON_IsNumber(item)) {\
      snprintf(s_errorbuf, sizeof(s_errorbuf),\
               "Attribute '%s' is not a number",\
               #name);\
      cJSON_Delete(json);\
      free_config_list(cl);\
      return s_errorbuf;\
    }\
    cfg->name = item->valuedouble;\
  }\
}

/* Verify that min/max values are valid */
#define VERIFY_RANGE(cfg, name)\
{\
  if (cfg->min_##name >= cfg->max_##name) {\
    snprintf(s_errorbuf, sizeof(s_errorbuf),\
             "min_" #name " is greater or equal to max_" #name);\
    cJSON_Delete(json);\
    free_config_list(cl);\
    return s_errorbuf;\
  }\
}

static void warn_on_unknown_keys(cJSON *obj)
{
  static const char *keys[] = {
    "name",
    "level",
    "min_offset",
    "max_offset",
    "min_length",
    "max_length",
    "min_cooldown",
    "max_cooldown",
    "min_gain",
    "max_gain",
    "min_multiplier",
    "max_multiplier",
    "reverse_probability",
    "num_slots",
    NULL,
  };

  obj = obj->child;

  for (; obj; obj = obj->next) {
    int known = 0;
    for (const char **key = keys; *key; ++key) {
      if (strcmp(*key, obj->string) == 0) {
        known = 1;
        break;
      }
    }
    if (!known) {
      log_warn("Unknown config key: %s", obj->string);
    }
  }
}

const char *load_config_list(const char *path, struct config_list *cl)
{
  char *text;
  cJSON *json;
  
  /* Load text */
  text = load_file(path);
  if (text == NULL) {
    return s_errorbuf;
  }

  /* Parse JSON */
  json = cJSON_Parse(text);
  if (json == NULL) {
    const char *offset = cJSON_GetErrorPtr();
    unsigned int lineno = get_lineno(text, offset);
    free(text);
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Config parse error on line %d", lineno);
    return s_errorbuf;
  }
  free(text);

  if (!cJSON_IsArray(json)) {
    cJSON_Delete(json);
    return "Config file is not an array";
  }

  cl->size = cJSON_GetArraySize(json);
  cl->cfgs = xmalloc(sizeof(*cl->cfgs) * cl->size);

  /* Load all configurations */
  for (size_t i = 0; i < cl->size; ++i) {
    struct config *cfg = &cl->cfgs[i];
    cJSON *entry = cJSON_GetArrayItem(json, i);

    /* Fill in default values */
    default_config(cfg);

    if (!cJSON_IsObject(entry)) {
      cJSON_Delete(json);
      free_config_list(cl);
      snprintf(s_errorbuf, sizeof(s_errorbuf),
               "Config entry %zu is not an object",
               i);
      return s_errorbuf;
    }

    cJSON *item;
    if ((item = cJSON_GetObjectItem(entry, "name"))) {
      if (!cJSON_IsString(item)) {
        snprintf(s_errorbuf, sizeof(s_errorbuf),
                 "Attribute 'name' is not a string");
        cJSON_Delete(json);
        free_config_list(cl);
        return s_errorbuf;
      }

      free(cfg->name);
      cfg->name = strdup(item->valuestring);
    }

    /* Load values */
    LOAD_VALUE(entry, cfg, level);
    LOAD_VALUE(entry, cfg, min_offset);
    LOAD_VALUE(entry, cfg, max_offset);
    LOAD_VALUE(entry, cfg, min_length);
    LOAD_VALUE(entry, cfg, max_length);
    LOAD_VALUE(entry, cfg, min_cooldown);
    LOAD_VALUE(entry, cfg, max_cooldown);
    LOAD_VALUE(entry, cfg, min_gain);
    LOAD_VALUE(entry, cfg, max_gain);
    LOAD_VALUE(entry, cfg, min_multiplier);
    LOAD_VALUE(entry, cfg, max_multiplier);
    LOAD_VALUE(entry, cfg, reverse_probability);
    LOAD_VALUE(entry, cfg, num_slots);

    /* Verify min/max values */
    VERIFY_RANGE(cfg, offset);
    VERIFY_RANGE(cfg, length);
    VERIFY_RANGE(cfg, cooldown);
    VERIFY_RANGE(cfg, gain);
    VERIFY_RANGE(cfg, multiplier);

    warn_on_unknown_keys(entry);
  }

  cJSON_Delete(json);

  return NULL;
}

void free_config_list(struct config_list *cl)
{
  free(cl->cfgs);
  memset(cl, 0, sizeof(*cl));
}
