#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "term.h"

static struct termios s_old_termios;

static void cleanup(void)
{
  tcsetattr(STDIN_FILENO, TCSANOW, &s_old_termios);
}

void term_set_raw(void)
{
  struct termios new_termios;

  tcgetattr(STDIN_FILENO, &s_old_termios);
  memcpy(&new_termios, &s_old_termios, sizeof(struct termios));

  cfmakeraw(&new_termios);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  atexit(cleanup);
}

