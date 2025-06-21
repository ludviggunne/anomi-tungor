#include <math.h>
#include <stdarg.h>

#include "harmony.h"
#include "xmalloc.h"

#define STEP 1.0594630943592953f

void add_chord(struct chord_list **list, size_t size, ...)
{
  va_list ap;

  struct chord_list *head = xcalloc(1, sizeof(*head));
  float *pitches = xmalloc(sizeof(*pitches) * size);

  va_start(ap, size);
  for (size_t i = 0; i < size; ++i) {
    int pc = va_arg(ap, int);
    pitches[i] = powf(STEP, pc);
  }
  va_end(ap);

  head->size = size;
  head->pitches = pitches;
  head->next = *list;
  *list = head;
}

void free_chord_list(struct chord_list *list)
{
  struct chord_list *tmp;
  while (list) {
    tmp = list;
    list = list->next;
    free(list->pitches);
    free(list);
    list = tmp;
  }
}
