#ifndef HARMONY_H
#define HARMONY_H

#include <stddef.h>

#define CHORD_LIST_INIT NULL

enum pitch_class {
  PC_C                = 0,
  PC_CSHARP           = 1,
  PC_DFLAT            = 1,
  PC_D                = 2,
  PC_DSHARP           = 3,
  PC_EFLAT            = 3,
  PC_E                = 4,
  PC_F                = 5,
  PC_FSHARP           = 6,
  PC_GFLAT            = 6,
  PC_G                = 7,
  PC_GSHARP           = 8,
  PC_AFLAT            = 8,
  PC_A                = 9,
  PC_ASHARP           = 10,
  PC_BFLAT            = 10,
  PC_B                = 11,
};

struct chord_list {
  size_t size;
  float *pitches;
  struct chord_list *next;
};

void add_chord(struct chord_list **list, size_t size, ...);
void free_chord_list(struct chord_list *list);

#endif
