#include "prog.h"

#include "scan.h"
#include "warn.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

Stack new_stack(void) {
  Stack s = {
    .sp=0,
    .len=STACK_BLOCK_SIZE,
    .arg=0,
    .arg_len=0,
    .lcl=0,
    .lcl_len=0,
  };
  s.ops = (Word*) calloc (s.len, sizeof(Word));
  assert(s.ops != NULL);
  return s;
}

void del_stack(Stack s) {
  free(s.ops);
}

void spush(Stack* stack, Word val) {
  assert(stack != NULL);
  
  if (stack->sp == stack->len) {
    stack->len += STACK_BLOCK_SIZE;
    stack->ops = (Word*) realloc (stack->ops, stack->len * sizeof(Word));
    assert(stack->ops != NULL);
  }

  stack->ops[stack->sp] = val;
  stack->sp ++;
}

int spop(Stack* stack, Word* val) {
  assert(stack != NULL);
  assert(val != NULL);

  /* Underflowing into the caller's stack is forbidden.
   * This part of the stack starts at
   * `stack->lcl + stack->lcl_len`. */

  if (stack->sp > (stack->lcl + stack->lcl_len)) {
    stack->sp --;
    if (stack->sp < stack->len - STACK_BLOCK_SIZE) {
      stack->len -= STACK_BLOCK_SIZE;
      stack->ops = (Word*) realloc (stack->ops, stack->len * sizeof(Word));
      assert(stack->ops != NULL);
    }

    *val = stack->ops[stack->sp];
    return 1;
  } else {
    return 0;
  }
}

Heap new_heap(void) {
  Heap h = {
    ._this = 0,
    .that = 0,
  };
  h.mem = (Word*) calloc (MEM_HEAP_SIZE, sizeof(Word));
  assert(h.mem != NULL);
  return h;
}

void del_heap(Heap h) {
  free(h.mem);
}

Word heap_get(const Heap h, Addr addr) {
  assert(h.mem != NULL);
  // No need to check out of range errors
  // since `heap.mem` has 0xFFFF entries
  // and `Addr` can't be larger than that.
  // Rare case of C type-safety ...
  return h.mem[addr];
}

void heap_set(Heap h, Addr addr, Word val) {
  assert(h.mem != NULL);
  // See `heap_get` about further range checks.
  h.mem[addr] = val;
}

Memory new_mem(void) {
  Memory mem = {
    ._static = (Word*) calloc (MEM_STAT_SIZE, sizeof(Word)),
    .tmp = (Word*) calloc (MEM_TEMP_SIZE, sizeof(Word)),
  };
  assert(mem._static != NULL);
  assert(mem.tmp != NULL);
  return mem;
}

void init_startup_file(File* file) {
  assert(file != NULL);

  const char sc[] = "<startup code>";
  file->filename = (char*) calloc (strlen(sc) + 1, sizeof(char));
  assert(file->filename != NULL);
  strcpy(file->filename, sc);

  /* All of this wastes some memory. */

  file->st = new_st();

  Insts insts = new_insts(sc);
  assert(2 <= INST_BLOCK_SIZE);
  insts.cell[insts.idx ++] = (Inst) {.code=PUSH, .mem={ .seg=CONST, .offset=0 }};
  insts.cell[insts.idx ++] = (Inst) {.code=CALL, .ident="Sys.init", .nargs=1 };
  file->insts = insts;

  file->mem = new_mem();
}

#define PROC_ERR 0
#define PROC_OK 1

int proc_file(File* file, const char* fn) {
  assert(file != NULL);
  assert(fn != NULL);

  /* 1. Scan */
  Tokens tokens = new_tokens(fn);
  int scan_res = scan(&tokens);
  if (scan_res == SCAN_ERR) { 
    del_tokens(tokens);
    return PROC_ERR;
  }

  /* 2. Parse */
  file->st = new_st();
  file->insts = new_insts(fn);
  int parse_res = parse(&tokens, &file->insts, &file->st);
  del_tokens(tokens);
  if (parse_res == PARSE_ERR) {
    del_st(file->st);
    del_insts(file->insts);
    return PROC_ERR;
  }

  /* Now we know the source code in `fn` is a
   * valid source file. Next all other members
   * of the file instance are initialized. */

  file->mem = new_mem();

  file->filename =
    (char*) calloc (strlen(fn) + 1, sizeof(char));
  assert(file->filename != NULL);
  strcpy(file->filename, fn);

  /* Should already be `0`. */
  file->ei = 0;

  return PROC_OK;
}

Program* make_prog(unsigned int nfn, const char* fn[]) {
  assert(fn != NULL);

  Program* prog =
    (Program*) calloc (1, sizeof(Program));
  assert(prog != NULL);

  prog->heap = new_heap();
  prog->stack = new_stack();

  /* Allocate `nfn + 1` for the startup code. */
  prog->files = (File*) calloc (nfn + 1, sizeof(File));
  assert(prog->files != NULL);

  /* Store the startup code the first file.
   * `fi` starts in this file. */
  init_startup_file(&prog->files[prog->nfiles ++]);
  
  for (; prog->nfiles <= nfn; prog->nfiles++) {
    warn_file_ext(fn[prog->nfiles - 1]);
    if (proc_file(
      &prog->files[prog->nfiles],
      fn[prog->nfiles - 1]
    ) == PROC_ERR) {
      del_prog(prog);
      return NULL;
    }
  }

  return prog;
}

void del_mem(Memory mem) {
  free(mem._static);
  free(mem.tmp);
}

void del_file(File* file) {
  if (file != NULL) {
    del_st(file->st);
    del_insts(file->insts);
    del_mem(file->mem);
    free(file->filename);
  }
}

void del_prog(Program* prog) {
  if (prog != NULL) {
    for (unsigned int i = 0; i < prog->nfiles; i++) {
      del_file(&prog->files[i]);
    }
    del_heap(prog->heap);
    del_stack(prog->stack);
    free(prog->files);
    free(prog);
  }
}
