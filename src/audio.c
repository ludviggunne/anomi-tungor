#include <string.h>
#include <stdio.h>
#include <pulse/pulseaudio.h>

#include "log.h"
#include "audio.h"
#include "synthesizer.h"
#include "output.h"
#include "xmalloc.h"

/* Names for publicly visible PulseAudio objects */
static const char *s_application_name = "Anomi";
static const char *s_playback_stream_name = "Anomi Output";

static char s_errorbuf[512] = {0};

/* Status flag for callbacks that might fail */
static int s_ok;

/* Status flag for running pa_operations */
static int s_op_done;

/* PulseAudio objects */
static pa_context *s_context;
static pa_threaded_mainloop *s_mainloop;
static pa_stream *s_playback_stream = NULL;

/* List with sink/source info */
static struct list *s_list;

/* Default sample spec, overwritten
 * by match_audio_file_sample_spec */
static pa_sample_spec s_default_sample_spec;

static int is_litte_endian_system(void)
{
  int x = 1;
  return *(char*) &x == 1;
}

static void context_connect_callback(pa_context *context, void *userdata)
{
  (void) context;
  (void) userdata;
  enum pa_context_state state;

  state = pa_context_get_state(s_context);
  switch (state) {
  case PA_CONTEXT_READY:
    s_ok = 1;
    __attribute__((fallthrough));
  case PA_CONTEXT_FAILED:
    pa_threaded_mainloop_signal(s_mainloop, 0);
    break;
  default:
    break;
  }
}

static void cleanup(void)
{
  /* TODO: syncing */

  if (s_playback_stream) {
    pa_stream_disconnect(s_playback_stream);
  }

  pa_context_disconnect(s_context);
  pa_threaded_mainloop_stop(s_mainloop);
}

int init_audio(void)
{
  /* Start mainloop */
  s_mainloop = pa_threaded_mainloop_new();
  pa_mainloop_api *api = pa_threaded_mainloop_get_api(s_mainloop);
  s_context = pa_context_new(api, s_application_name);
  pa_threaded_mainloop_start(s_mainloop);

  /* Connect to PulseAudio server */
  s_ok = 0;
  pa_threaded_mainloop_lock(s_mainloop);
  pa_context_set_state_callback(s_context, context_connect_callback, NULL);
  pa_context_connect(s_context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
  pa_threaded_mainloop_wait(s_mainloop);
  pa_threaded_mainloop_unlock(s_mainloop);

  if (!s_ok) {
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Failed to connect to PulseAudio server: %s",
             pa_strerror(pa_context_errno(s_context)));
    pa_threaded_mainloop_stop(s_mainloop);
    return -1;
  }

  /* Match system byte order in sample format */
  if (is_litte_endian_system()) {
    s_default_sample_spec.format = PA_SAMPLE_FLOAT32LE;
  } else {
    s_default_sample_spec.format = PA_SAMPLE_FLOAT32BE;
  }

  atexit(cleanup);

  return 0;
}

static void sink_info_list_callback(pa_context *context, const pa_sink_info *info,
                                    int eol, void *userdata)
{
  (void) context;
  (void) userdata;

  const char *key;
  const char *description;

  if (eol) {
    pa_threaded_mainloop_signal(s_mainloop, 0);
    return;
  }

  key = PA_PROP_DEVICE_DESCRIPTION;
  description = pa_proplist_gets(info->proplist, key);

  struct list *l = xcalloc(1, sizeof(*l));
  l->index = info->index;
  l->name = strdup(info->name);
  l->description = strdup(description);
  l->next = s_list;
  s_list = l;
}

struct list *list_sinks(void)
{
  struct list *l;

  pa_threaded_mainloop_lock(s_mainloop);
  pa_context_get_sink_info_list(s_context, sink_info_list_callback, NULL);
  pa_threaded_mainloop_wait(s_mainloop);
  pa_threaded_mainloop_unlock(s_mainloop);

  l = s_list;
  s_list = NULL;
return l;
}

static void stream_connect_callback(pa_stream *s, void *userdata)
{
  (void) userdata;
  enum pa_stream_state state;

  state = pa_stream_get_state(s);
  switch (state)
  {
  case PA_STREAM_READY:
    s_ok = 1;
    __attribute__((fallthrough));
  case PA_STREAM_FAILED:
    pa_threaded_mainloop_signal(s_mainloop, 0);
    break;
  default:
    break;
  }
}

void match_audio_file_sample_spec(struct audio_file *af)
{
  s_default_sample_spec.rate = af->samplerate;
  s_default_sample_spec.channels = af->channels;
}

int connect_sink(const char *name)
{
  s_playback_stream = pa_stream_new(s_context, s_playback_stream_name, &s_default_sample_spec, NULL);
  if (s_playback_stream == NULL) {
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Failed to create playback stream: %s",
             pa_strerror(pa_context_errno(s_context)));
    return -1;
  }

  /* Overwrite some buffer attributes
   * so application is more responsive to
   * configuration updates */
  pa_buffer_attr attr = {
    .fragsize = (uint32_t)-1,
    .maxlength = (uint32_t)-1,
    .minreq = (uint32_t)-1,
    .prebuf = 0,
    .tlength = pa_usec_to_bytes(1000000, &s_default_sample_spec),
  };

  s_ok = 0;
  pa_threaded_mainloop_lock(s_mainloop);
  pa_stream_set_state_callback(s_playback_stream, stream_connect_callback, s_playback_stream);

  /* Start stream in paused mode.
   * Sync with record stream. */
  pa_stream_connect_playback(s_playback_stream, name, &attr, PA_STREAM_START_CORKED, NULL, NULL);
  pa_threaded_mainloop_wait(s_mainloop);
  pa_threaded_mainloop_unlock(s_mainloop);

  if (!s_ok) {
    name = name ? name : "default sink";
    snprintf(s_errorbuf, sizeof(s_errorbuf),
             "Failed to connect to %s: %s",
             name, pa_strerror(pa_context_errno(s_context)));
    return -1;
  }

  return 0;
}

const char *get_audio_error_string(void)
{
  return s_errorbuf;
}

static void stream_success_callback(pa_stream *s, int ok, void *userdata)
{
  (void) s;
  (void) userdata;
  s_ok = ok;
  s_op_done = 1;
  pa_threaded_mainloop_signal(s_mainloop, 1);
}

/* Pause stream if b == 1, start if b == 0 */
static int cork_stream(pa_stream *stream, int b)
{
  pa_operation *op;
  enum pa_error_code err;

  pa_threaded_mainloop_lock(s_mainloop);

  s_ok = 0;
  s_op_done = 0;
  op = pa_stream_cork(stream, b, stream_success_callback, NULL);

  while (!s_op_done) {
    pa_threaded_mainloop_wait(s_mainloop);
  }

  pa_operation_unref(op);
  pa_threaded_mainloop_accept(s_mainloop);
  err = pa_context_errno(s_context);
  pa_threaded_mainloop_unlock(s_mainloop);

  if (s_ok)
    return 0;

  snprintf(s_errorbuf, sizeof(s_errorbuf),
           "Failed to %s stream: %s",
           b ? "stop" : "start", pa_strerror(err));
  return -1;
}

static void stream_write_callback(pa_stream *s, size_t nbytes, void *userdata)
{
  struct synthesizer *syn = userdata;
  size_t nsamps = nbytes / sizeof(float);

  /* Synthesize enough samples to fill target buffer */
  lock_synthesizer(syn);
  synthesize(syn, nsamps);
  void *data = synthesizer_get_data_ptr(syn);
  write_to_output_file(data, nsamps);
  pa_stream_write(s, data, nbytes, NULL, 0, PA_SEEK_RELATIVE);
  unlock_synthesizer(syn);

  /* TODO: Should the pa_operation be handled somehow? */
  pa_stream_drain(s, NULL, NULL);
}

/* Generic stream notification callback that just prints
 * a messsage */
static void stream_notify_callback(pa_stream *p, void *userdata)
{
  (void) p;
  const char *msg = userdata;
  log_warn("Stream notify: %s", msg);
}

int start_stream(struct synthesizer *syn)
{
  /* Set write callback and start streams */

  pa_stream_set_write_callback(s_playback_stream, stream_write_callback, syn);

  /* Set some notification callbacks */
  pa_stream_set_overflow_callback(s_playback_stream, stream_notify_callback, "Overflow");
  pa_stream_set_underflow_callback(s_playback_stream, stream_notify_callback, "Underflow");
  pa_stream_set_suspended_callback(s_playback_stream, stream_notify_callback, "Suspended");

  if (cork_stream(s_playback_stream, 0) < 0)
    return -1;

  return 0;
}

void free_list(struct list *l)
{
  while (l) {
    struct list *tmp = l;
    free(l->name);
    free(l->description);
    l = l->next;
    free(tmp);
  }
}
