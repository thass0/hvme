#pragma once

#ifndef _WARN_H_
#define _WARN_H_

#include <stdlib.h>
#include "st.h"

// Print a warning if the file extension of the
// given file is not `.vm`.
void warn_file_ext(const char* filename);

// Print a warning if the file doens't end with
// a newline character.
void warn_eof_nl(void);

// Print a warning about lit exceeding the maximum
// allowed 16-bit number range.
void warn_sat_uilit(int lit);

// Print a warning that the identifer of
// length `len` exceeds the maximum identifer
// length `max`. It will therefore be truncated to
// the first `MAX_IDENT_LEN` characters.
void warn_trunc_ident(const char* blk, size_t len, size_t max);

// Warn the user that `parse` didn't receive a
// symbol table which means that any label-related
// instruction doesn't work.
void warn_no_st(const SymKey* key, const SymVal* val);

#endif  // _WARN_H_
