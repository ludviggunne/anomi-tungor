#include <stdio.h>
#include <string.h>

#include "audio-file.h"
#include "xmalloc.h"
#include "output.h"
#include "select.h"
#include "audio.h"
#include "config.h"
#include "event.h"
#include "log.h"
#include "synthesizer.h"

/* Set the profile automatically
 * by matching volume with 'level' field */
static int s_auto_profile = 0;
static size_t s_current_profile_index = 0;
static float s_current_volume = 0.f;
static float s_profile_interp_time = 2.f;

static void switch_profile(struct synthesizer *syn, struct config *cfg)
{
  struct profile *profile = &cfg->profiles[s_current_profile_index];

  if (profile->name) {
    log_info("Switching to profile %zu (%s)", s_current_profile_index, profile->name);
  } else {
    log_info("Switching to profile %zu", s_current_profile_index);
  }

  lock_synthesizer(syn);
  set_synthesizer_profile(syn, &cfg->profiles[s_current_profile_index], 0);
  unlock_synthesizer(syn);
}

static void help(void)
{
  log_info("Key mappings:");
  log_info("    q       Quit");
  log_info("    a       Toggle auto profile mode");
  log_info("    l       List profiles");
  log_info("    j       Go down profile list");
  log_info("    k       Go up profile list");
  log_info("    p       Print profile number");
  log_info("    v       Print volume");
  log_info("    h       Print this help message");
  log_info("    f       Fade out");
  log_info("    u       Increase fade out/profile interpolation time");
  log_info("    d       Decrease fade out/profile interpolation time");
  log_info("    0-9     Select profile by index");
}

int main(int argc, char **argv)
{
  (void) argc;
  (void) argv;

  int quit;
  const char *err;
  const char *audio_path = NULL;
  const char *config_path = NULL;
  const char *output_path = NULL;

  /* Parse command line arguments */
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
        s_auto_profile = 1;
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
      case 'o':
        output_path = arg;
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

  /* Load configuration */
  struct config cfg;
  err = load_config(config_path, &cfg);
  if (err != NULL) {
    log_err("Failed to load config %s: %s", config_path, err);
    return -1;
  }
  log_info("Configuration: %s", config_path);

  /* Load audio file */
  struct audio_file *af = xcalloc(1, sizeof(*af));
  err = load_audio_file(audio_path, af);
  if (err != NULL) {
    log_err("Failed to load audio file %s: %s", audio_path, err);
    return -1;
  }

  log_info("Input file:    %s", audio_path);
  log_info("Channels:      %d", af->channels);
  log_info("Sample rate:   %d", af->samplerate);

  err = event_loop_start(config_path);
  if (err != NULL) {
    log_err("Failed to start event loop: %s", err);
    return -1;
  }

  if (output_path) {
    err = open_output_file(output_path, af);
    if (err != NULL) {
      log_err("%s", err);
      return -1;
    }
  }

  if (init_audio() < 0) {
    err = get_audio_error_string();
    log_err("Failed to initalize audio: %s", err);
    return -1;
  }

  /* Match the streams settings with the audio file,
   * (sample rate and number of channels) */
  match_audio_file_sample_spec(af);

  /* Let the user select a sink... */
  struct list *l = list_sinks();
  log_info("Select sink ('q' to quit):");
  struct list *sink = list_select(l);

  if (sink == NULL) {
    log_info("Quit");
    return 0;
  }

  if (connect_sink(sink->name) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }
  log_info("Selected sink %s", sink->name);
  free_list(l);

  /* ...and source */
  l = list_sources();
  log_info("Select source ('q' to quit):");
  struct list *source = list_select(l);

  if (source == NULL) {
    log_info("Quit");
    return 0;
  }

  if (connect_source(source->name) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }
  log_info("Selected source %s", source->name);
  free_list(l);

  struct synthesizer *syn = create_synthesizer(af);
  set_synthesizer_profile(syn, &cfg.profiles[s_current_profile_index], 1);
  sythesizer_set_interp_time(syn, s_profile_interp_time);

  if (start_streams(syn) < 0) {
    err = get_audio_error_string();
    log_err("%s", err);
    return -1;
  }

  log_info("Ready");
  log_info("Press 'h' for a list of key bindings");

  /* Print current config */
  struct event pevent;
  pevent.type = EVENT_INPUT;
  pevent.c = 'p';
  queue_event(&pevent);

  quit = 0;
  while (!quit) {
    struct event ev = event_loop_poll();

    /* Poll events */
    switch (ev.type) {
      case EVENT_INPUT:

        switch (ev.c) {
        case 'q':
          log_info("Do you really want to quit? (y/n)");

          int c;
          do {
            c = fgetc(stdin);
          } while (strchr("ynYNq", c) == NULL);

          if (c == 'y' || c == 'Y' || c == 'q') {
            quit = 1;
          } else {
            log_info("Continuing...");
          }
          break;

        case 'a':
          /* Toggle auto config */
          s_auto_profile = !s_auto_profile;
          log_info("Auto-profile: %s", s_auto_profile ? "on" : "off");
          break;

        case 'j':
          /* Decrement config index */
          if (s_auto_profile || s_current_profile_index == 0)
            break;
          s_current_profile_index--;
          switch_profile(syn, &cfg);
          break;

        case 'k':
          /* Increment config index */
          if (s_auto_profile || s_current_profile_index == cfg.size - 1)
            break;
          s_current_profile_index++;
          switch_profile(syn, &cfg);
          break;

        case 'v':
          log_info("Volume: %.4f", s_current_volume);
          break;

        case 'p':
        {
          struct profile *profile = &cfg.profiles[s_current_profile_index];
          if (profile->name) {
            log_info("Current profile: %zu (%s)", s_current_profile_index, profile->name);
          } else {
            log_info("Current profile: %zu", s_current_profile_index);
          }
          break;
        }

        case 'u':
          if (s_profile_interp_time > 1.f) {
            s_profile_interp_time += .5f;
          } else {
            s_profile_interp_time *= 2.f;
          }

          log_info("Profile interpolation time set to %.3fs", s_profile_interp_time);
          sythesizer_set_interp_time(syn, s_profile_interp_time);
          break;

        case 'd':
          if (s_profile_interp_time <= 1.f) {
            s_profile_interp_time /= 2.f;
          } else {
            s_profile_interp_time -= .5f;
          }

          log_info("Profile interpolation time set to %.3fs", s_profile_interp_time);
          sythesizer_set_interp_time(syn, s_profile_interp_time);
          break;

        case 'l':
        {
          log_info("Profiles:");
          for (size_t i = 0; i < cfg.size; ++i) {
            struct profile *profile = &cfg.profiles[i];
            char sel = i == s_current_profile_index ? '*' : ' ';
            if (profile->name) {
              log_info("  %c %zu: %s", sel, i, profile->name);
            } else {
              log_info("  %c %zu", sel, i);
            }
          }
          break;
        }

        case 'h':
          help();
          break;

        case 'f':
          log_info("Fading out...");
          sythesizer_fade_out(syn);
          break;

        default:
          if ('0' <= ev.c && ev.c <= '9') {
            if (s_auto_profile) {
              /* Don't allow manual config selection
               * if auto-config is set */
              break;
            }

            size_t index = ev.c - '0';

            if (index >= cfg.size) {
              log_err("No profile with index %zu", index);
              break;
            }

            s_current_profile_index = index;
            switch_profile(syn, &cfg);
            break;
          }
          break;
        }
        break;

      case EVENT_WATCH:
      {

        /* Configuration file is modified */
        struct config new_cl;
        err = load_config(config_path, &new_cl);

        if (err != NULL) {
          log_err("Failed to load config: %s", err);
        } else {
          free_config(&cfg);
          memcpy(&cfg, &new_cl, sizeof(struct config));
          log_info("Config %s reloaded", config_path);

          if (s_current_profile_index >= cfg.size) {
            s_current_profile_index = cfg.size - 1;
            switch_profile(syn, &cfg);
          } else {
            /* No message if index didn't change */
            lock_synthesizer(syn);
            set_synthesizer_profile(syn, &cfg.profiles[s_current_profile_index], 0);
            unlock_synthesizer(syn);
          }
        }

        break;
      }

      case EVENT_VOLUME:
      {
        s_current_volume = ev.volume;

        if (!s_auto_profile)
          break;

        /* Select configuration based on volume if
         * auto-config is set */

        size_t i;
        for (i = 0; i < cfg.size - 1; ++i) {
          if (cfg.profiles[i + 1].level > ev.volume)
            break;
        }

        if (i != s_current_profile_index) {
          s_current_profile_index = i;
          switch_profile(syn, &cfg);
        }

        break;
      }
    }
  }
}
