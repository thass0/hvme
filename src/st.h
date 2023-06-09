# pragma once

# ifndef _ST_H_
# define _ST_H_

#include <stdlib.h>
#include <assert.h>

#include "scan.h"

typedef enum {
    SBT_UNUSED = 0,
    SBT_FUNC,
    SBT_LABEL,
} SymKeyType;

typedef struct {
  SymKeyType type;
  char ident[MAX_IDENT_LEN + 1];
} SymKey;

typedef struct {
  // Instruction address in `Insts`.
  size_t inst_addr;
  uint16_t nlocals;  // Used only in functions.
} SymVal;

typedef struct {
  SymKey key;
  SymVal val;
} Symbol;

// Open-addressed symbol table.
typedef struct {
  Symbol* cell;
  size_t len;  // Number of entries in `cell`.
  size_t used;  // Number of used entries.
  size_t num_inst;  // Number of next instruction.
  size_t offset;  // Address offset for retrieval.
} SymbolTable;

# ifndef ST_BLOCK_SIZE
#   ifdef UNIT_TESTS
// Make `ST_BLOCK_SIZE` so small that allocation
// edge cases will be tested. It can't be smaller
// than four, however, because this breaks the
// function to delimit the hashes.
#     define ST_BLOCK_SIZE 4
#   else
#     define ST_BLOCK_SIZE 0x1000
#   endif  // UNIT_TESTS
# endif  // ST_BLOCK_SIZE

SymbolTable new_st(void);

void del_st(SymbolTable st);

// Instantiate keys and values.
SymKey mk_key(const char* ident, SymKeyType type);
SymVal mk_lbval(size_t inst_addr);
SymVal mk_fnval(size_t inst_addr, uint16_t nlocals);

typedef enum {
  INRES_EXISTS = 0,  // The given symbol already exists with different data.
  INRES_OK = 1,
} InsertResult;

InsertResult insert_st(
  SymbolTable* st,
  SymKey key,
  SymVal val
);

typedef enum {
  GTRES_ERR = 0,
  GTRES_OK = 1,
} GetResult;

// `val` is set to the retrieved info if `GTRES_OK` is returned.
// Otherwise `val` isn't changed.
GetResult get_st(SymbolTable st, const SymKey* key, SymVal* val);

# endif  // _ST_H_
