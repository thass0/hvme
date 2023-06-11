#include "msg.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define NO_COLOR "NO_COLOR"

void clean_stdout(void) {
  /* Print a trailing newline if there's none. */
  #ifndef UNIT_TESTS
  char last_stdout;
  fread(&last_stdout, sizeof(char), 1, stdout);
  if (last_stdout != '\n') {
    printf("\n");
  }
  fflush(stdout);
  #endif  // UNIT_TESTS
}

static inline void init_perr(Pos pos) {
  char* no_color = getenv(NO_COLOR);
  char* err_init = "\033[31mError\033[0m";
  if (no_color != NULL && no_color[0] != '\0') {
    err_init= "Error";
  }

  if (pos.filename == NULL) {
    fprintf(stderr,
      "%s (%d:%d):\033[0m ",
      err_init, pos.ln + 1, pos.cl + 1);
  } else {
    fprintf(stderr,
      "%s (%s:%d:%d):\033[0m ",
      err_init, pos.filename, pos.ln + 1, pos.cl + 1);
  }
}

void perrf(Pos pos, const char* fmt, ...) {  
  va_list args;
  va_start(args, fmt);
  clean_stdout();
  init_perr(pos);
  vfprintf(stderr,  fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
}

void perr(Pos pos, const char* msg) {
  clean_stdout();
  init_perr(pos);
  fputs(msg, stderr);
  fputs("\n", stderr);
}

void err(const char* msg) {
  clean_stdout();
  char* no_color = getenv(NO_COLOR);
  char* err_init = "\033[31mError\033[0m";
  if (no_color != NULL && no_color[0] != '\0') {
    err_init= "Error";
  }
  fprintf(stderr, "%s %s\n", err_init, msg);
}

static inline void init_warn(void) {
  char* no_color = getenv(NO_COLOR);
  if (no_color != NULL && no_color[0] != '\0') {
    fprintf(stderr, "Warn: ");
  } else {
    fprintf(stderr, "\033[33mWarn:\033[0m ");
  }
}

static inline void hint_indicator(void) {
  char* no_color = getenv(NO_COLOR);
  if (no_color != NULL && no_color[0] != '\0') {
    fprintf(stderr, "\t-> ");
  } else {
    fprintf(stderr, "\t\033[34;3m->\033[m ");
  }
}

void warn_file_ext(const char* filename) {
  // Compare includes only the last three characters.
  // For the warning not to go off they must be `.vm`.
  const char* compare = filename + (strlen(filename) - 3); 
  if (strcmp(compare, ".vm") != 0) {
    init_warn();
    fprintf(stderr, "file name `%s` doesn't end with `.vm`\n", filename);
  }
}

void warn_eof_nl(void) {
  init_warn();
  fprintf(stderr, "no trailing newline at end of file.\n");
  hint_indicator();
  fprintf(stderr, "Automatically adding newline after the last character\n");
}

void warn_sat_uilit(int lit) {
  init_warn();
  fprintf(stderr, "`%d` exceeds the range possible 16-bit numbers.\n", lit);
  hint_indicator();
  fprintf(stderr, "Saturating to maximum value 65535\n");
}

void warn_trunc_ident(const char* blk, size_t len, size_t max) {
  char ident_buf[len + 1];
  strncpy(ident_buf, blk, len);
  ident_buf[len] = '\0';

  char ident_trunc[max + 1];
  strncpy(ident_trunc, blk, max);
  ident_trunc[max] = '\0';

  init_warn();
  fprintf(stderr, "`%s` is too long to be an identifier.\n", ident_buf);
  hint_indicator();
  fprintf(stderr, "It's truncated to `%s` (%lu chars)\n", ident_trunc, max);
}

void warn_no_st(const SymKey* key, const SymVal* val) {
  assert(key != NULL);
  assert(val != NULL);

  const char* type_name;
  switch (key->type) {
    case SBT_LABEL: type_name = "label"; break;
    case SBT_FUNC: type_name = "function"; break;
    case SBT_UNUSED: type_name = "unused symbol"; break;
  }
  init_warn();
  fprintf(stderr, "symbol table doesn't exists.\n");
  hint_indicator();
  fprintf(stderr, "Can't enter %s `%s` pointing to instruction %lu\n",
    type_name, key->ident, val->inst_addr);
}
