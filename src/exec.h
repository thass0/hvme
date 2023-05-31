#pragma once

#ifndef _EXEC_H_
#define _EXEC_H_

#include "parse.h"

#define EXEC_ERR -1

// Single RAM word.
typedef uint16_t Word;

# ifndef STACK_BLOCK_SIZE
#   ifdef UNIT_TESTS
#     define STACK_BLOCK_SIZE 2
#   else
#     define STACK_BLOCK_SIZE 0x1000
#   endif  // UNIT_TESTS
# endif  // STACK_BLOCK_SIZE

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

// Delete memory allocated by the given heap.
void del_heap(Heap h);

// Execute the given instructions
// until `IC_NONE` is found. Returns
// `0` on success and `EXEC_ERR` if
// an error arises during execution.
int exec(Stack* stack, Heap* heap, SymbolTable* st, const Insts* insts);

#endif  // _EXEC_H_
