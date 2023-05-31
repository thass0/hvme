# pragma once

# ifndef _ST_H_
# define _ST_H_

#include <stdlib.h>
#include <assert.h>

#include "scan.h"

enum SymKeyType {
    SBT_UNUSED = 0,
    SBT_FUNC,
    SBT_LABEL,
};

struct SymKey {
  enum SymKeyType type;
  char ident[MAX_IDENT_LEN + 1];
};

struct SymVal {
  // Instruction address in `Insts`.
  size_t inst_addr;
  uint16_t nlocals;  // Used only in functions.
};

struct Symbol {
  struct SymKey key;
  struct SymVal val;
};

// Open-addressed symbol table.
typedef struct {
  struct Symbol* cell;
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
struct SymKey mk_key(const char* ident, enum SymKeyType type);
struct SymVal mk_lbval(size_t inst_addr);
struct SymVal mk_fnval(size_t inst_addr, uint16_t nlocals);

typedef enum {
  INRES_EXISTS = 0,  // The given symbol already exists with different data.
  INRES_OK = 1,
} InsertResult;

InsertResult insert_st(
  SymbolTable* st,
  struct SymKey key,
  struct SymVal val
);

typedef enum {
  GTRES_ERR = 0,
  GTRES_OK = 1,
} GetResult;

// `val` is set to the retrieved info if `GTRES_OK` is returned.
// Otherwise `val` isn't changed.
GetResult get_st(SymbolTable st, const struct SymKey* key, struct SymVal* val);

# endif  // _ST_H_
