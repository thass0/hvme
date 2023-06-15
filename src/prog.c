#include "prog.h"

#include "scan.h"
#include "msg.h"

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

/* NOTE: It is forbidden to use a pointer into `stack->ops`
 * as `val` because this might cause a use after free error
 * if `realloc` changes the location of `stack->ops`. */
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

/* Add builtin instruction. */
void add_bii(Insts* insts, Inst add) {
  assert(insts != NULL);

  /* Realloc if full. */
  if (insts->idx == insts->len) { 
    insts->len += INST_BLOCK_SIZE;
    insts->cell = (Inst*) realloc (insts->cell,
      insts->len * sizeof(Inst));
    assert(insts->cell != NULL);
  }

  insts->cell[insts->idx] = add;

  /* Give the instruction an automatic position.
   * Without special formatting each instruction
   * spans one line. Hence, we can translate the
   * instruction index into the line. The column
   * number can stay unchanged (0). */
  insts->cell[insts->idx].pos.ln = insts->idx;
  insts->cell[insts->idx].pos.filename = insts->filename;

  insts->idx ++;
}

/* NOTE: On adding another builtin.
 * Builtins work by defining an internal instructions which
 * implements the desired function and exposing it wrapped
 * in a regular function. To add a new one changes have to
 * be made in the following files.
 *
 *   1. Add the wrapping function to the
 *      system functions here (`src/prog.c`).
 *   2. Add the new internal instruction to `InstCode`
 *      in `src/parse.h`.
 *   3. Add the builtin's name to `inst_str` in `src/parse.c`.
 *   4. Add a `case NEW_BUILTIN` to `exec_prog` which
 *      executes the builtin's implementation in `src/exec.c`.
 *
 */

void builtin_print_char(File* file) {
  assert(file != NULL);

  /*
   * `Sys.print_char (c) -> 0`
   * prints the given character.
   */

  insert_st(&file->st,
    mk_key("Sys.print_char", SBT_FUNC),
    mk_fnval(file->insts.idx, 0));

  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=ARG, .offset=0 }});
  add_bii(&file->insts, (Inst) { .code=BUILTIN_PRINT_CHAR });
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=CONST, .offset=0 }});
  add_bii(&file->insts, (Inst) { .code=RET });
}

void builtin_print_num(File* file) {
  assert(file != NULL);

  /*
   * `Sys.print_num (num) -> 0`
   * prints the given number as an unsigned integer.
   */

  insert_st(&file->st,
    mk_key("Sys.print_num", SBT_FUNC),
    mk_fnval(file->insts.idx, 0));

  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=ARG, .offset=0 }});
  add_bii(&file->insts, (Inst) { .code=BUILTIN_PRINT_NUM });
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=CONST, .offset=0 }});
  add_bii(&file->insts, (Inst) { .code=RET });
}

void builtin_print_str(File* file) {
  assert(file != NULL);

  /*
   * `Sys.print_str (nchars, addr) -> 0`
   * prints a character array of length `nchars`.
   * `nchars` is placed on stack below `addr`.
   */

  insert_st(&file->st,
    mk_key("Sys.print_str", SBT_FUNC),
    mk_fnval(file->insts.idx, 0));

  // Push `nchars` from the arguments onto stack.
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=ARG, .offset=0 }});
  // Push `addr` from the arguments onto the stack.
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=ARG, .offset=1 }});
  add_bii(&file->insts, (Inst) { .code=BUILTIN_PRINT_STR });
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=CONST, .offset=0 }});
  add_bii(&file->insts, (Inst) { .code=RET });
}

void builtin_read_char(File* file) {
  assert(file != NULL);

  /*
   * `Sys.read_char() -> char`
   * reads a single character and returns it.
   */

  insert_st(&file->st,
    mk_key("Sys.read_char", SBT_FUNC),
    mk_fnval(file->insts.idx, 0));

  // Read a character.
  add_bii(&file->insts, (Inst) { .code=BUILTIN_READ_CHAR });
  // Return the character.
  add_bii(&file->insts, (Inst) { .code=RET });
}

void builtin_read_num(File* file) {
  assert(file != NULL);

  /*
   * `Sys.read_num() -> num`
   * reads a single unsigned integer and returns it.
   */

  insert_st(&file->st,
    mk_key("Sys.read_num", SBT_FUNC),
    mk_fnval(file->insts.idx, 0));

  add_bii(&file->insts, (Inst) { .code=BUILTIN_READ_NUM });
  add_bii(&file->insts, (Inst) { .code=RET });
}

void builtin_read_str(File* file) {
  assert(file != NULL);

  /*
   * `Sys.read_str(addr) -> nchars`
   * reads a line and stores it on heap starting at `addr`.
   * The number of characters stored is returned.
   * The stored string doesn't include the terminating
   * newline character.
   */

  insert_st(&file->st,
    mk_key("Sys.read_str", SBT_FUNC),
    mk_fnval(file->insts.idx, 1));

  // Push heap address to store the read string.
  add_bii(&file->insts, (Inst) { .code=PUSH, .mem={ .seg=ARG, .offset=0 }});
  // Read a line and store the string at this heap address.
  // `BUILTIN_READ_STR` pushes the number of chars read on stack.
  add_bii(&file->insts, (Inst) { .code=BUILTIN_READ_STR });
  // Return the number of read characters (pushed by `BUILTIN_READ_STR`).
  add_bii(&file->insts, (Inst) { .code=RET });
}

void init_system_file(File* file) {
  assert(file != NULL);

  const char sc[] = "<system>";
  file->filename = (char*) calloc (strlen(sc) + 1, sizeof(char));
  assert(file->filename != NULL);
  strcpy(file->filename, sc);

  file->st = new_st();
  file->insts = new_insts(sc);
  file->mem = new_mem();

  /* Store builtin functions in system file. */
  
  builtin_print_char(file);
  builtin_print_num(file);
  builtin_print_str(file);
  builtin_read_char(file);
  builtin_read_num(file);
  builtin_read_str(file);

  /* Add startup code (must be at the very end).
   * This first pushed the number of arguments `Sys.init`
   * will receive on stack and then calls `Sys.init`. */
  file->ei = file->insts.idx;
  add_bii(&file->insts, (Inst) {.code=PUSH, .mem={ .seg=CONST, .offset=0 }});
  add_bii(&file->insts, (Inst) {.code=CALL, .ident="Sys.init", .nargs=1 });
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

  /* Store the system code (startup code, builtins etc.)
   * the first file. `fi` starts in this file. */
  init_system_file(&prog->files[prog->nfiles ++]);
  
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
