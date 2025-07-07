#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <portmidi.h>

#include "xmalloc.h"
#include "midi.h"
#include "audio.h" // TODO: move struct list to its own header
#include "select.h"
#include "log.h"
#include "event.h"

#define EVENT_BUF_SIZE 1

static PmStream *s_stream = NULL;
static PmEvent s_event_buf[EVENT_BUF_SIZE];
static int s_freeze = 0;
static int s_middle_pedal_held = 0;

static void cleanup(void)
{
  if (s_stream)
    Pm_Close(s_stream);

  Pm_Terminate();
}

static void *midi_proc(void *args)
{
  (void) args;

  const int note_on = 144;
  const int note_off = 128;
  const int middle_pedal = 176;

  for (;;) {
    int num_events_read = Pm_Read(s_stream, s_event_buf, EVENT_BUF_SIZE);
    for (int i = 0; i < num_events_read; ++i) {
      struct event ev;
      int status = Pm_MessageStatus(s_event_buf[i].message);
      int data1 = Pm_MessageData1(s_event_buf[i].message);
      int data2 = Pm_MessageData2(s_event_buf[i].message);

      log_info("midi %d:%d:%d", status, data1, data2);

      if (status == middle_pedal) {
        ev.type = EVENT_FREEZE;
        if (data2 && !s_middle_pedal_held) {
          s_freeze = !s_freeze;
          ev.freeze = s_freeze;
        } else {
          s_middle_pedal_held = !!data2;
          continue;
        }
      } else if (status == note_on) {
        ev.type = EVENT_MIDI;
        ev.on = 1;
        ev.pitch = data1 % 12;
      } else if (status == note_off) {
        ev.type = EVENT_MIDI;
        ev.on = 0;
        ev.pitch = data1 % 12;
      } else continue;

      queue_event(&ev);
    }
  }

  return NULL;
}

void midi_init(void)
{
  Pm_Initialize();
  atexit(cleanup);

  /* Select device */
  struct list *device_list = NULL;
  int device_count = Pm_CountDevices();
  for (int i = 0; i < device_count; ++i) {

    const PmDeviceInfo *info = Pm_GetDeviceInfo(i);
    if (!info->input)
      continue;

    struct list *l = xcalloc(1, sizeof(*l));
    l->index = i;
    l->name = strdup(info->name);
    l->description = strdup("");
    l->next = device_list;
    device_list = l;
  }

  struct list *selected = list_select(device_list);
  if (selected == NULL) {
    log_info("Abort");
    exit(1);
  }
  int selected_id = selected->index;
  free_list(device_list);
  
  PmError error;
  error = Pm_OpenInput(&s_stream, selected_id, NULL, EVENT_BUF_SIZE, NULL, NULL);
  if (error != pmNoError) {
    log_err("Unable to connect to MIDI device: %s", Pm_GetErrorText(error));
    exit(1);
  }

  /* TODO: clean exit */

  pthread_attr_t attr;
  pthread_t thread;

  pthread_attr_init(&attr);
  pthread_create(&thread, &attr, &midi_proc, NULL);
}
