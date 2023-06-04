#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include "utils.h"
#include "../src/parse.h"
#include "../src/exec.h"

#include <stdio.h>
#include <string.h>

/* From `src/prog.c`. */
extern struct Memory new_mem(void);

#define TEST_PROG_NAME "test_internal"

static struct Program* setup_prog(Inst* arr, size_t len) {
  struct File* file = (struct File*) calloc (1, sizeof(struct File));
  file->filename =
    (char*) calloc (strlen(TEST_PROG_NAME) + 1, sizeof(char));
  assert(file->filename != NULL);
  strcpy(file->filename, TEST_PROG_NAME);
  file->st = new_st();
  file->insts = new_insts(TEST_PROG_NAME);
  file->insts->cell =
    (Inst*) realloc (file->insts->cell, len * sizeof(Inst));
  memcpy(file->insts->cell, arr, len * sizeof(Inst));
  file->insts->len = len;
  file->insts->idx = len;
  file->mem = new_mem();
  file->ei = 0;

  struct Program* prog = (struct Program*) calloc (1, sizeof(struct Program));
  assert(prog != NULL);
  prog->files = file;
  prog->nfiles = 1;
  prog->fi = 0;
  prog->heap = new_heap();
  prog->stack = new_stack();

  return prog;
}

TEST(correct_stack_errors) {
  {  // Stack underflow when popping of the empty stack.
    Inst inst_arr[] = {
      {.code=POP, .mem={.seg=TMP, .offset=0}},
    };
    struct Program* prog = setup_prog(inst_arr, 1);
    int res = exec(prog);
    del_prog(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("stack underflow", 20, stderr), ==, 1);
  }

  return MUNIT_OK;
}

TEST(correct_memory_errors) {
  {  // Raise error if invalid pointer segement is addressed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=PTR, .offset=2 }},
    };
    struct Program* prog = setup_prog(inst_arr, 1);
    int res = exec(prog);
    del_prog(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("can't access pointer segment at `2` (max. index is 1).", 20, stderr), ==, 1);
  }
  {  // Raise error if combined address exceeds bounds
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=0xFFFF }},  // Push max. on stack.
      { .code=POP, .mem={ .seg=PTR, .offset=0 }},  // Set `this` to offset 0xFFFF.
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},  // Push another value on stack.
      { .code=POP, .mem={ .seg=THIS, .offset=1 }},  // Pop this value to address 0xFFFF + 1 -> overflow
    };
    struct Program* prog = setup_prog(inst_arr, 4);
    int res = exec(prog);
    del_prog(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("address overflow: "
      "`pop this 1` tries to access RAM at 65536.", 20, stderr), ==, 1);
  }

  return MUNIT_OK;
}

TEST(arithmetic_errors) {
  {
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=ADD },
    };
    struct Program* prog = setup_prog(inst_arr, 3);
    int res = exec(prog);
    del_prog(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("addition overflow: 65535 + 1 = 65536 > 65535", 20, stderr), ==, 1);
  }
  {
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=SUB },
    };
    struct Program* prog = setup_prog(inst_arr, 3);
    int res = exec(prog);
    del_prog(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("subtraction underflow: 0 - 1 = -1 < 0", 20, stderr), ==, 1);
  }

  return MUNIT_OK;
}

TEST(arithmetic_instructions) {
  Inst inst_arr[] = {
    { .code=PUSH, .mem={ .seg=CONST, .offset=9 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=10723 }},
    { .code=ADD },
    { .code=PUSH, .mem={ .seg=CONST, .offset=10732 }},
    { .code=SUB },
    { .code=NOT },
    { .code=NEG },
    { .code=PUSH, .mem={ .seg=CONST, .offset=2 }},
    { .code=ADD },
  };
  struct Program* prog = setup_prog(inst_arr, 9);
  int res = exec(prog);
  assert_int(res, ==, 0);
  assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 3);
  del_prog(prog);
  return MUNIT_OK;
}

TEST(stack_buildup_works) {
  // Buildup
  Inst inst_arr1[] = {
    { .code=PUSH, .mem={ .seg=CONST, .offset=7 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=11 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=13 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=17 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=19 }},
    { .code=POP, .mem={ .seg=CONST, .offset=0 }},
    { .code=POP, .mem={ .seg=CONST, .offset=0 }},
    { .code=POP, .mem={ .seg=CONST, .offset=0 }},
    { .code=ADD },
  };
  struct Program* prog = setup_prog(inst_arr1, 9);
  int res = exec(prog);
  assert_int(res, ==, 0);
  assert_int(prog->stack.len, ==, 2);
  assert_int(prog->stack.sp, ==, 1);
  assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 18);

  del_prog(prog);

  return MUNIT_OK;
}

TEST(stack_doesnt_change_on_error) {
  {
    // The stack should not change if the
    // addition would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=9 }},
      { .code=ADD },
    };
    struct Program* prog = setup_prog(inst_arr, 3);
    int res = exec(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 9);
    assert_int(prog->stack.ops[prog->stack.sp - 2], ==, 65535);
    del_prog(prog);
  } {
    // Stack should not change if a pop's
    // address would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=POP, .mem={ .seg=PTR, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=POP, .mem={ .seg=THIS, .offset=1 }},
    };
    struct Program* prog = setup_prog(inst_arr, 4);
    int res = exec(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(prog->stack.sp, ==, 1);
    assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 1);
    del_prog(prog);
  } {
    // Stack should not change if a push's
    // address would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=POP, .mem={ .seg=PTR, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=PUSH, .mem={ .seg=THIS, .offset=1 }},
    };
    struct Program* prog = setup_prog(inst_arr, 4);
    int res = exec(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(prog->stack.sp, ==, 1);
    assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 1);
    del_prog(prog);
  } {
    // Stack should not change if a pop
    // tried to access an invald pointer
    // segment.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=POP, .mem={ .seg=PTR, .offset=2 }},
    };
    struct Program* prog = setup_prog(inst_arr, 2);
    int res = exec(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(prog->stack.sp, ==, 1);
    assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 1);
    del_prog(prog);
  } {
    // The stack should not change if the
    // subtraction would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=SUB },
    };
    struct Program* prog = setup_prog(inst_arr, 3);
    int res = exec(prog);
    assert_int(res, ==, EXEC_ERR);
    assert_int(prog->stack.ops[prog->stack.sp - 1], ==, 1);
    assert_int(prog->stack.ops[prog->stack.sp - 2], ==, 0);
    del_prog(prog);
  }

  return MUNIT_OK;
}

MunitTest exec_tests[] = {
  REG_TEST(correct_stack_errors),
  REG_TEST(correct_memory_errors),
  REG_TEST(arithmetic_errors),
  REG_TEST(arithmetic_instructions),
  REG_TEST(stack_doesnt_change_on_error),
  REG_TEST(stack_buildup_works),
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

