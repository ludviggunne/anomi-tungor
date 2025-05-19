#include <stdio.h>
#include <string.h>

#include "audio-file.h"
#include "select.h"
#include "audio.h"
#include "config.h"
#include "event.h"
#include "log.h"
#include "synthesizer.h"

static int s_auto_config = 0;
static size_t s_current_config_index = 0;
static float s_current_volume = 0.f;

static void switch_config(struct synthesizer *syn, struct config_list *cl)
{
  log_info("Switching to config %zu", s_current_config_index);
  lock_synthesizer(syn);
  set_synthesizer_config(syn, &cl->cfgs[s_current_config_index]);
  unlock_synthesizer(syn);
}

static void help(void)
{
  log_info("Key mappings:");
  log_info("    q - Quit");
  log_info("    a - Toggle auto config mode");
  log_info("    j - Go down config list");
  log_info("    k - Go up config list");
  log_info("    c - Print config number");
  log_info("    v - Print volume");
  log_info("    f - Print this help message");
}

int main(int argc, char **argv)
{
  (void) argc;
  (void) argv;

  int quit;
  const char *err;
  const char *audio_path = NULL;
  const char *config_path = NULL;

  for (argv++; *argv; ++argv) {
    const char *arg = *argv;

    if (arg[0] == '-') {

      arg++;
      int c = *(arg++);

      if (c == 0) {
        log_err("Empty option");
        return -1;
      }

      switch (c) {
      case 'a':
        s_auto_config = 1;
        continue;
      default:
        break;
      }

      if (strlen(arg) == 0) {
        argv++;
        arg = *argv;
      }

      switch (c) {
      case 'f':
        audio_path = arg;
        break;
      case 'c':
        config_path = arg;
        break;
      default:
        log_err("Unknown option '-%c'", c);
        return -1;
      }

      continue;
    }

    log_err("Invalid argument '%s'", arg);
    return -1;
  }

  if (audio_path == NULL) {
    log_err("Missing audio file (specify with -f <file>)");
  }

  if (config_path == NULL) {
    log_err("Missing config file (specify with -c <file>)");
  }

  if (audio_path == NULL || config_path == NULL) {
    return -1;
  }

  struct config_list cl;
  err = load_config_list(config_path, &cl);
  if (err != NULL) {
    log_err("Failed to load config %s: %s", config_path, err);
    return -1;
  }
  log_info("Config %s loaded", config_path);

  struct audio_file af;
  err = load_audio_file(audio_path, &af);
  if (err != NULL) {
    log_err("Failed to load audio file %s: %s", audio_path, err);
    return -1;
  }

  log_info("Input file:  %s", audio_path);
  log_info("Channels:    %d", af.channels);
  log_info("Sample rate: %d", af.samplerate);

  err = event_loop_start(config_path);
  if (err != NULL) {
    log_err("Failed to start event loop: %s", err);
    return -1;
  }
  log_info("Event loop started");

  if (init_audio() < 0) {
    err = get_audio_error_string();
    log_err("Failed to initalize audio: %s", err);
    return -1;
  }
  log_info("Audio backend initialized");

  match_audio_file_sample_spec(&af);

  struct list *l = list_sinks();
  log_info("Select sink:");
  struct list *sink = list_select(l);

  if (connect_sink(sink->name) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }
  log_info("Connected to sink %s", sink->name);
  free_list(l);

  l = list_sources();
  log_info("Select source:");
  struct list *source = list_select(l);

  if (connect_source(source->name) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }
  log_info("Connected to source %s", source->name);
  free_list(l);

  struct synthesizer *syn = create_synthesizer(&af);
  set_synthesizer_config(syn, &cl.cfgs[s_current_config_index]);

  if (start_streams(syn) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }
  log_info("Started recording");

  quit = 0;
  struct config_list new_cl;
  while (!quit) {
    struct event ev = event_loop_poll();

    switch (ev.type) {
      case EVENT_INPUT:

        switch (ev.c) {
        case 'q':
          log_info("Do you really want to quit? (y/n)");

          int c = fgetc(stdin);
          if (c == 'y' || c == 'q') {
            quit = 1;
          } else {
            log_info("Continuing...");
          }
          break;

        case 'a':
          s_auto_config = !s_auto_config;
          log_info("Auto-config: %s", s_auto_config ? "on" : "off");
          break;

        case 'j':
          if (s_auto_config || s_current_config_index == 0)
            break;
          s_current_config_index--;
          switch_config(syn, &cl);
          break;

        case 'k':
          if (s_auto_config || s_current_config_index == cl.size - 1)
            break;
          s_current_config_index++;
          switch_config(syn, &cl);
          break;

        case 'v':
          log_info("Volume: %.4f", s_current_volume);
          break;

        case 'c':
          log_info("Current config: %d", s_current_config_index);
          break;

        case 'h':
          help();
          break;

        default:
          break;
        }
        break;

      case EVENT_WATCH:
        err = load_config_list(config_path, &new_cl);

        if (err != NULL) {
          log_err("Failed to load config: %s", err);
        } else {
          free_config_list(&cl);
          memcpy(&cl, &new_cl, sizeof(struct config_list));
          log_info("Config %s reloaded", config_path);

          if (s_current_config_index >= cl.size) {
            s_current_config_index = cl.size - 1;
          }

          switch_config(syn, &cl);
        }

        break;

      case EVENT_VOLUME:
      {
        s_current_volume = ev.volume;

        if (!s_auto_config)
          break;

        size_t i;
        for (i = 0; i < cl.size - 1; ++i) {
          if (cl.cfgs[i + 1].level > ev.volume)
            break;
        }

        if (i != s_current_config_index) {
          s_current_config_index = i;
          switch_config(syn, &cl);
        }

        break;
      }
    }
  }
}
