#include "warn.h"

#include <stdio.h>
#include <string.h>

#define SOLUTION_ARROW "\t\033[34;3m->\033[m "

#define warnf(fmt, ...) {   \
  fprintf(stderr,           \
    "\033[33mWarn:\033[0m " \
    fmt                     \
    "\n",                   \
    __VA_ARGS__);           \
}

#define warn(str) \
  { fputs("\033[33mWarn:\033[0m " str "\n", stderr); }


void warn_file_ext(const char* filename) {
  // Compare includes only the last three characters.
  // For the warning not to go off they must be `.vm`.
  const char* compare = filename + (strlen(filename) - 3); 
  if (strcmp(compare, ".vm") != 0) {
    warnf("file name `%s` doesn't end with `.vm`", filename);
  }
}

void warn_eof_nl(void) {
  warn("no trailing newline at end of file.\n"
      SOLUTION_ARROW "Automatically adding newline after the last character");
}

void warn_sat_uilit(int lit) {
  warnf("`%d` exceeds the range possible 16-bit numbers.\n"
        SOLUTION_ARROW "Saturating to maximum value 65535", lit);
}

void warn_trunc_ident(const char* blk, size_t len, size_t max) {
  char ident_buf[len + 1];
  strncpy(ident_buf, blk, len);
  ident_buf[len] = '\0';

  char ident_trunc[max + 1];
  strncpy(ident_trunc, blk, max);
  ident_trunc[max] = '\0';

  warnf("`%s` is too long to be an identifier.\n"
    SOLUTION_ARROW "It's truncated to `%s` (%lu chars)", ident_buf, ident_trunc, max);
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
  warnf("symbol table doesn't exists.\n"
    SOLUTION_ARROW "Can't enter %s `%s` pointing to instruction %lu",
    type_name, key->ident, val->inst_addr
  );
}