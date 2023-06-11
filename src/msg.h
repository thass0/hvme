#pragma once

#ifndef _MSG_H_
#define _MSG_H_

#include "scan.h"
#include "st.h"
#include <stdio.h>

/* Print a format string. Using this function makes
 * `clean_stdout` work. HVME internals should not use
 * any other function to print something. */
int hvme_fprintf(FILE *restrict stream, const char *restrict fmt, ...);

/* `hvme_fprintf` counter part for non-literal strings. */
int hvme_fputs(const char *restrict s, FILE *restrict stream);

/* Flush stdout and add a newline if it's missing. */
void clean_stdout(void);

/* Formatted error message with source position. */
void perrf(Pos pos, const char* fmt, ...);

/* Unformatted error message. */
void err(const char* msg);

/* Unformatted error message with source position. */
void perr(Pos pos, const char* msg);

/* Print a warning if the file extension of the
 * given file is not `.vm`. */
void warn_file_ext(const char* filename);

/* Print a warning if the file doens't end with
 * a newline character. */
void warn_eof_nl(void);

/* Print a warning about lit exceeding the maximum
 * allowed 16-bit number range. */
void warn_sat_uilit(int lit);

/* Print a warning that the identifer of
 * length `len` exceeds the maximum identifer
 * length `max`. It will therefore be truncated to
 * the first `MAX_IDENT_LEN` characters. */
void warn_trunc_ident(const char* blk, size_t len, size_t max);

/* Warn the user that `parse` didn't receive a
 * symbol table which means that any label-related
 * instruction doesn't work. */
void warn_no_st(const SymKey* key, const SymVal* val);

#endif  // _MSG_H_
