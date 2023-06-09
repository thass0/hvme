#pragma once

#ifndef _SCAN_H_
#define _SCAN_H

#include <stdint.h>
#include <unistd.h>

// NOTE: The marked beginnings and end of
// the ranges of different token types must
// remain unchanged so that all range check
// throught the scanner and the parser
// continue to work correctly.

// Possible scan tokens.
typedef enum {
  TK_NONE = 0,  // Meta token. Represents a none value. Start of tokens.
  TK_PUSH = 1,  // Literal `push`: start of memory instruction range.
  TK_POP,       // Literal `pop`: end of memory instruction range.
  TK_ARG,       // Literal `argument`: start of segment range.
  TK_LOC,       // Literal `local`
  TK_STAT,      // Literal `static`
  TK_CONST,     // Literal `constant`
  TK_THIS,      // Literal `this`
  TK_THAT,      // Literal `that`
  TK_PTR,       // Literal `pointer`
  TK_TMP,       // Literal `temp`: end of segment range.
  TK_UINT,      // Unsigned integer (0, 1, 2 ...)
  TK_LABEL,     // Literal `label`
  TK_GOTO,      // Literal `goto`
  TK_IF_GOTO,   // Literal `if-goto`
  TK_FUNC,      // Literal `function`
  TK_CALL,      // Literal `call`
  TK_RET,       // Literal `return`
  TK_ADD,       // Literal `add`: start of operation range.
  TK_SUB,       // Literal `sub`
  TK_NEG,       // Literal `neg`
  TK_EQ,        // Literal `eq`
  TK_GT,        // Literal `gt`
  TK_LT,        // Literal `lt`
  TK_AND,       // Literal `and`
  TK_OR,        // Literal `or`
  TK_NOT,       // Literal `not`: end of operation range.
  TK_IDENT,     // Identifier (recognizes `[A-Za-z_\.:][0-9A-Za-z_\.:]*`)
                // Must be at the end so it has to lowest precedence when scaning.
} Token;

# ifndef MAX_IDENT_LEN
# define MAX_IDENT_LEN 24
# endif  // MAX_IDENT_LEN

# ifndef MAX_TOKEN_LEN
// Length of longest possible token is 8 (`argument`/`constant`).
// The largest possible 16-bit number only has
// floor(log10(65535)) + 1 = 5 digits.
// This length is therefore dicated by `MAX_IDENTLEN`
# define MAX_TOKEN_LEN MAX_IDENT_LEN
# endif

# ifndef ITEM_STR_BUF
// Size of char buffer for `item_str`.
# define ITEM_STR_BUF (MAX_TOKEN_LEN + 1)
# endif

typedef uint16_t Uint;

typedef struct {
  unsigned int ln;
  unsigned int cl;
  /* Not used by the scanner because items
   * doesn't contain a filename which will
   * last long enought. */
  const char* filename;
} Pos;

// Scanned item
typedef struct {
  Token t;
  Pos pos;
  Uint uilit;
  char ident[MAX_IDENT_LEN + 1];
} Item;

typedef struct {
  size_t idx;
  size_t len;
  Item* cell;
  char* filename;
  Pos cur;   // Used only while scanning to track where we are.
} Items;

# ifndef ITEM_BLOCK_SIZE
# define ITEM_BLOCK_SIZE 0x1000
# endif

// Initialize a new `Items` instance.
Items* new_items(const char* filename);
// Delete an `Items` instance.
void del_items(Items* items);

// Writes `it` to `str`. `str` must be able
// to hold  at least `ITEM_STR_BUF` characters
// or else this function might segfault.
void item_str(const Item* it, char* str);

// Macro to handle the fixed-size string
// buffer used to stringify an item. This
// macro should be the only way `item_str`
// is called.
# define ITEM_STR(id, item) \
  char id[ITEM_STR_BUF]; \
  item_str(item, id);


# ifndef SCAN_BLOCK_SIZE
#  ifdef UNIT_TESTS
#    define SCAN_BLOCK_SIZE MAX_TOKEN_LEN
#  else
#    define SCAN_BLOCK_SIZE 0x10000
#  endif
# endif

// Scans the input and returns a
// heap-allocated array of tokens.
Items* scan_blocks(const char* filename);

#define SCAN_ERR -1

// Scans `blk` for `len` characters and appends the
// scanned items in `items`.
ssize_t scan(Items* items, const char* blk, size_t len);

#endif  // _SCAN_H_
