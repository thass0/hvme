#pragma once

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>

void clean_stdout(void);

/* Formatted error message. */
#define errf(fmt, ...) {        \
  clean_stdout();               \
  fprintf(stderr,               \
    "\033[31mError:\033[0m "    \
    fmt                         \
    "\n",                       \
    __VA_ARGS__);               \
}

/* Formatted error message with source position. */
#define perrf(pos, fmt, ...) {                   \
  clean_stdout();                                \
  if ((pos).filename == NULL) {                  \
    fprintf(stderr,                              \
      "\033[31mError\033[0m (%d:%d):\033[0m "    \
      fmt "\n",                                  \
      (pos).ln, (pos).cl,                        \
      __VA_ARGS__);                              \
  } else {                                       \
    fprintf(stderr,                              \
      "\033[31mError\033[0m (%s:%d:%d):\033[0m " \
      fmt "\n",                                  \
      (pos).filename, (pos).ln, (pos).cl,        \
      __VA_ARGS__);                              \
 }                                               \
}

/* Unformatted error message. */
#define err(str) {               \
  clean_stdout();                \
  fputs("\033[31mError:\033[0m " \
    str                          \
    "\n",                        \
    stderr);                     \
}

/* Unformatted error message with source position. */
#define perr(pos, str) {                         \
  clean_stdout();                                \
  if ((pos).filename == NULL) {                  \
    fprintf(stderr,                              \
      "\033[31mError\033[0m (%d:%d):\033[0m "    \
      str "\n",                                  \
      (pos).ln, (pos).cl);                       \
  } else {                                       \
    fprintf(stderr,                              \
      "\033[31mError\033[0m (%s:%d:%d):\033[0m " \
      str "\n",                                  \
      (pos).filename, (pos).ln, (pos).cl);       \
 }                                               \
}

#endif  // _UTILS_H_

