#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "log.h"
#include "select.h"

static const char s_chars[] = "0123456789abcdefghijklmnopqrstuvwxyz";

struct list *list_select(struct list *l)
{
  struct list *it = l;
  const char *c = s_chars;

  for (; it && c; it = it->next, c++) {
    log_info("    \x1b[34m%c\x1b[0m: %s: %s (%d)", *c, it->description, it->name, it->index);
  }

  const char *sel;
  do {
    int s = fgetc(stdin);
    sel = strchr(s_chars, s);
  } while (sel == NULL || sel >= c);

  it = l;
  c = s_chars;

  for (; it && c && c != sel; it = it->next, c++) {
  }

  assert(it);

  return it;
}
