#ifndef XMALLOC_H
#define XMALLOC_H

#include <stdlib.h>
#include <stdio.h>

/* Some stdlib wrappers tht just crash
 * on OOM */

static inline void *xmalloc(size_t size)
{
  void *ptr = malloc(size);

  if (!ptr) {
    perror("malloc");
    abort();
  }

  return ptr;
}

static inline void *xrealloc(void *ptr, size_t size)
{
  void *new_ptr = realloc(ptr, size);

  if (!new_ptr) {
    perror("realloc");
    abort();
  }

  return new_ptr;
}

static inline void *xcalloc(size_t nmemb, size_t size)
{
  void *ptr = calloc(nmemb, size);

  if (!ptr) {
    perror("calloc");
    abort();
  }

  return ptr;
}
#endif
