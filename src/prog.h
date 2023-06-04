#pragma once

#ifndef _PROG_H_
#define _PROG_H_

#include "st.h"
#include "parse.h"

// Single RAM word.
typedef uint16_t Word;

#define MEM_HEAP_SIZE 0x1000lu
#define MEM_STAT_SIZE 0x100lu
#define MEM_TEMP_SIZE 0x10lu


#ifndef STACK_BLOCK_SIZE
#  ifdef UNIT_TESTS
#    define STACK_BLOCK_SIZE 2
#  else
#    define STACK_BLOCK_SIZE 0x1000
#  endif  // UNIT_TESTS
#endif  // STACK_BLOCK_SIZE

// Operand stack.
typedef struct {
  Word* ops;
  size_t sp;  // Current stack pointer.
  size_t arg;  // Argument stack pointer.
  size_t arg_len;  // Length of argument segment.
  size_t lcl;  // Local stack pointer.
  size_t lcl_len;  // Length of local segment.
  size_t len;  // Amount of memory allocated for the stack.
} Stack;

// Allocate and initialize a new stack.
Stack new_stack(void);

// Push a value on the stack.
void spush(Stack* stack, Word val);

// Underflow
#define SPOP_UF 0
#define SPOP_OK 1

// Pop a value off the stack. Return
// value indicated outcome. On success
// `val` will be changed to the popped
// value.
int spop(Stack* stack, Word* val);

// Delete memory allocated by the given stack.
void del_stack(Stack s);

// Machine address in 16-bit RAM.
typedef uint16_t Addr;

// Heap. Addressable range [0;0xFFFF].
typedef struct {
  Word* mem;
  size_t _this;
  size_t that;
} Heap;

// Allocate and initialize a new heap.
Heap new_heap(void);

Word heap_get(const Heap h, Addr addr);

void heap_set(Heap h, Addr addr, Word val);

// Delete memory allocated by the given heap.
void del_heap(Heap h);

struct Memory {
  Word* _static;
  Word* tmp;
};

struct File {
  char* filename;  /* guess what. */
  SymbolTable st;  /* file's symbols. */
  Insts* insts;  /* files's instructions. */
  struct Memory mem;  /* file's local memory segments (static and temp). */
  unsigned int ei;  /* execution index into  `insts`. */
};

struct Program {
  struct File* files;  /* files for all sources. */
  unsigned int nfiles;  /* number of files in `files`. */
  unsigned int fi;  /* file index into `files`. */
  Heap heap;  /* Program heap memory. */
  Stack stack;  /* Program stack memory. */
};

/* Assemable the source code in all the given
 * files into an executable program. */
struct Program* make_prog(unsigned int nfn, const char** fn);

void del_prog(struct Program* prog);

#endif // _PROG_H_
