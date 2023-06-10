#pragma once

#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdio.h>

#include "scan.h"
#include "st.h"

typedef enum {
  ARG=TK_ARG,
  LOC=TK_LOC,
  STAT=TK_STAT,
  CONST=TK_CONST,
  THIS=TK_THIS,
  THAT=TK_THAT,
  PTR=TK_PTR,
  TMP=TK_TMP,
} Segment;

// VM instruction
typedef struct {
  // Instruction code.
  enum InstCode {
    // Instruction code none.
    IC_NONE=0,
    // Memory: (PUSH | POP) <segment> <offset>
    PUSH=TK_PUSH, POP=TK_POP,
    // Arithmetic
    ADD=TK_ADD, SUB=TK_SUB, NEG=TK_NEG,
    AND=TK_AND, OR=TK_OR, NOT=TK_NOT,
    // Logic
    EQ=TK_EQ, GT=TK_GT, LT=TK_LT,
    // Control flow.
    GOTO=TK_GOTO, IF_GOTO=TK_IF_GOTO,
    // Function calling.
    CALL=TK_CALL, RET=TK_RET,
    // Builtin instructions. No way to directy access them.
    // `TK_IDENT` is the last token (must be since it has the
    // lowest precedence). Therefore it's used from here on out
    // to ensure that there is no overlap.
    BUILTIN_PRINT=(TK_IDENT + 1),
    BUILTIN_READ_CHAR,
    BUILTIN_READ_LINE,
  } code;

  union {
    // Memory operands (set for `PUSH` and `POP`).
    struct {
      Segment seg;
      uint16_t offset;
    } mem;
    // Identifier (set for `GOTO`, `IF_GOTO` and `CALL`).
    char ident[MAX_IDENT_LEN + 1];
  };

  /* This field is separate from the above
   * union so  that `ident` and `nargs` can
   * both be set for `CALL`.
   * Number of arguments (set for `CALL`):
   */
  uint16_t nargs;

  // Original position in source file.
  Pos pos;
} Inst;

typedef struct {
  size_t idx;
  size_t len;
  Inst* cell;
  char* filename;
} Insts;

// Initialize a new `Insts` instance.
Insts new_insts(const char* filename);

// Delete an `Insts` instance.
void del_insts(Insts insts);

#ifndef INST_STR_BUF
// Size of `char` buf to pass to `inst_str`.
#define INST_STR_BUF 40
#endif  // INST_STR_BUF

// Write a string describing `i` to `str`.
// `str` must be large enought to hold at
// least `INST_STR_BUF` characters.
void inst_str(const Inst* i, char* str);

// Macro to handle the fixed-size string
// buffer used to stringify an instruction.
// This macro should be the only way
// `token_str` is called.
#define INST_STR(id, inst) \
  char id[INST_STR_BUF]; \
  inst_str(inst, id);


#ifndef INST_BLOCK_SIZE
#define INST_BLOCK_SIZE 0x1000
#endif  // INST_BLOCK_SIZE

// Last element in instruction array.
#define NULL_INST (Inst) { .code=IC_NONE }

#define PARSE_ERR 0
#define PARSE_OK 1

// Parse the given array of token. Returns `NULL` on failure.
int parse(const Tokens* tokens, Insts* insts, SymbolTable* st);

#endif  // _PARSE_H_
