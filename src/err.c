#include "err.h"

#include <stdio.h>

// Flush stdout and add a newline if it's missing.
void clean_stdout(void) {
  /* Print a trailing newline if there's none. */
  char last_stdout;
  fread(&last_stdout, sizeof(char), 1, stdout);
  if (last_stdout != '\n') {
    printf("\n");
  }
  fflush(stdout);
}
