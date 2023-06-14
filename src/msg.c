#include "msg.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#define NO_COLOR "NO_COLOR"

static char last_stdout = '\0';

#define PRINT_BUF_SIZE 1024

int hvme_fputs(const char *restrict s, FILE *restrict stream) {
  int len = strlen(s);
  if (stream == stdout) {
    last_stdout = s[len > 0 ? len - 1 : '\0'];
  }
  return fputs(s, stream);  // <- Only time `fputs` is allowed.
}

int hvme_fprintf(FILE *restrict stream, const char *restrict fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char* print_buf = (char*) calloc (PRINT_BUF_SIZE, sizeof(char));
  int nprinted = vsnprintf(print_buf, PRINT_BUF_SIZE, fmt, ap);
  va_end(ap);

  if (nprinted < 0) {
    /* Return error */
    free(print_buf);
    return nprinted;
  } else if (nprinted > PRINT_BUF_SIZE - 1) {
    /* Output was truncated. This is an error, too.
     * However, we just assume that this won't happen.
     * The maximum length of any user-defined content
     * (source code snippets in errors) is much lower
     * than the buffer so this should be OK. */
    free(print_buf);
    return -1;
  } else {
    print_buf[nprinted] = '\0';
    int ret = hvme_fputs(print_buf, stream);
    free(print_buf);
    return ret;
  }
}

void clean_stdout(void) {
  /* Print a trailing newline if there's none. */
  #ifndef UNIT_TESTS
  /* Print a newline if the last character wasn't
   * already a newline and stdout isn't empty. */
  if (last_stdout != '\n' && last_stdout != '\0')
    hvme_fprintf(stdout, "\n");
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
    hvme_fprintf(stderr,
      "%s (%d:%d):\033[0m ",
      err_init, pos.ln + 1, pos.cl + 1);
  } else {
    hvme_fprintf(stderr,
      "%s (%s:%d:%d):\033[0m ",
      err_init, pos.filename, pos.ln + 1, pos.cl + 1);
  }
}

void perrf(Pos pos, const char* fmt, ...) {  
  va_list args;
  va_start(args, fmt);
  clean_stdout();
  init_perr(pos);
  /* This is ok because it is internal; `clean_stdout`
   * will never be called before another `hvme_fprintf` 
   * is called to correctly set `last_stdout`. */
  vfprintf(stderr,  fmt, args);
  hvme_fprintf(stderr, "\n");
  va_end(args);
}

void perr(Pos pos, const char* msg) {
  clean_stdout();
  init_perr(pos);
  hvme_fputs(msg, stderr);
  hvme_fputs("\n", stderr);
}

void err(const char* msg) {
  clean_stdout();
  char* no_color = getenv(NO_COLOR);
  char* err_init = "\033[31mError\033[0m";
  if (no_color != NULL && no_color[0] != '\0') {
    err_init= "Error";
  }
  hvme_fprintf(stderr, "%s %s\n", err_init, msg);
}

static inline void init_warn(void) {
  char* no_color = getenv(NO_COLOR);
  if (no_color != NULL && no_color[0] != '\0') {
    hvme_fprintf(stderr, "Warn: ");
  } else {
    hvme_fprintf(stderr, "\033[33mWarn:\033[0m ");
  }
}

static inline void hint_indicator(void) {
  char* no_color = getenv(NO_COLOR);
  if (no_color != NULL && no_color[0] != '\0') {
    hvme_fprintf(stderr, "\t-> ");
  } else {
    hvme_fprintf(stderr, "\t\033[34;3m->\033[m ");
  }
}

void warn_file_ext(const char* filename) {
  // Compare includes only the last three characters.
  // For the warning not to go off they must be `.vm`.
  const char* compare = filename + (strlen(filename) - 3); 
  if (strcmp(compare, ".vm") != 0) {
    init_warn();
    hvme_fprintf(stderr, "file name `%s` doesn't end with `.vm`\n", filename);
  }
}

void warn_eof_nl(void) {
  init_warn();
  hvme_fprintf(stderr, "no trailing newline at end of file.\n");
  hint_indicator();
  hvme_fprintf(stderr, "Automatically adding newline after the last character\n");
}

void warn_sat_uilit(int lit) {
  init_warn();
  hvme_fprintf(stderr, "`%d` exceeds the range possible 16-bit numbers.\n", lit);
  hint_indicator();
  hvme_fprintf(stderr, "Saturating to maximum value 65535\n");
}

void warn_trunc_ident(const char* blk, size_t len, size_t max) {
  char ident_buf[len + 1];
  strncpy(ident_buf, blk, len);
  ident_buf[len] = '\0';

  char ident_trunc[max + 1];
  strncpy(ident_trunc, blk, max);
  ident_trunc[max] = '\0';

  init_warn();
  hvme_fprintf(stderr, "`%s` is too long to be an identifier.\n", ident_buf);
  hint_indicator();
  hvme_fprintf(stderr, "It's truncated to `%s` (%lu chars)\n", ident_trunc, max);
}

void warn_no_st(const SymKey* key, const SymVal* val) {
  assert(key != NULL);
  assert(val != NULL);

  init_warn();
  hvme_fprintf(stderr, "symbol table doesn't exists.\n");
  hint_indicator();
  hvme_fprintf(stderr, "Can't enter %s `%s` starting at instruction %lu\n",
    key_type_name(key->type), key->ident, val->inst_addr + 1);
}
