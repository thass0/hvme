#include "utils.h"

#include <stdio.h>
#include <string.h>

#ifndef EXEC_NAME
#define EXEC_NAME "vme"
#endif

char** parse_args(int argc, const char* argv[]) {
  if (argc > 1) {
    char** arr = (char**) calloc (argc, sizeof(char));
    /* `calloc` implicityly null-termiantes `arr`
     * because we allocate one entry more than we
     * need. */
    for (int i = 1; i < argc; i++) {
      arr[i - 1] = (char*) calloc (strlen(argv[i]) + 1, sizeof(char));
      strcpy(arr[i - 1], argv[i]);
    }
    return arr;
  } else {
    return NULL;
  }
}

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

void warn_no_st(const struct SymKey* key, const struct SymVal* val) {
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

Insts* gen_startup(const SymbolTable* st) {
  assert(st != NULL);

  struct SymVal val;
  struct SymKey si_key = mk_key("Sys.init", SBT_FUNC);
  if (get_st(*st, &si_key, &val) != GTRES_OK) {
    
    return NULL;
  }

  Insts* si = new_insts(NULL);

  // Number of arguments passed to `Sys.init`.
  si->cell[si->idx++] = (Inst) { .code=PUSH, .mem={ .seg=CONST, .offset=0 }};
  // Call `Sys.init`
  si->cell[si->idx++] = (Inst) { .code=CALL, .ident="Sys.init", .nargs=1 };

  return si;
}
