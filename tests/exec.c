#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include "utils.h"
#include "../src/parse.h"
#include "../src/exec.h"

#include <stdio.h>
#include <string.h>

static inline Insts* setup_insts(Inst* cell, size_t len) {
  Insts* insts = new_insts(NULL);
  free(insts->cell);
  insts->cell = cell;
  insts->len = len;
  // The `idx` entry should point one index
  // past the last valid one.
  insts->idx = len;
  return insts;
}

static inline void cleanup_insts(Insts* insts) {
  if (insts != NULL) {
    // Don't free `cell` since it's a static array.
    free(insts->filename);
    free(insts);
  }
}

TEST(correct_stack_errors) {
  {  // Stack underflow when popping of the empty stack.
    Inst inst_arr[] = {
      {.code=POP, .mem={.seg=TMP, .offset=0}},
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 1);
    int res = exec(&s, &h, NULL, insts);
    del_stack(s);
    del_heap(h);
    cleanup_insts(insts);
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
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 1);
    int res = exec(&s, &h, NULL, insts);
    del_stack(s);
    del_heap(h);
    cleanup_insts(insts);
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
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 4);
    int res = exec(&s, &h, NULL, insts);
    del_stack(s);
    del_heap(h);
    cleanup_insts(insts);
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
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 3);
    int res = exec(&s, &h, NULL, insts);
    del_stack(s);
    del_heap(h);
    cleanup_insts(insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(check_stream("addition overflow: 65535 + 1 = 65536 > 65535", 20, stderr), ==, 1);
  }
  {
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=SUB },
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 3);
    int res = exec(&s, &h, NULL, insts);
    del_stack(s);
    del_heap(h);
    cleanup_insts(insts);
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
  Stack s = new_stack();
  Heap h = new_heap();
  Insts* insts = setup_insts(inst_arr, 9);
  int res = exec(&s, &h, NULL, insts);
  del_heap(h);
  assert_int(res, ==, 0);
  assert_int(s.ops[s.sp - 1], ==, 3);
  cleanup_insts(insts);
  del_stack(s);
  return MUNIT_OK;
}

TEST(stack_buildup_works) {
  Stack s = new_stack();
  Heap h = new_heap();
  // Buildup
  Inst inst_arr1[] = {
    { .code=PUSH, .mem={ .seg=CONST, .offset=7 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=11 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=13 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=17 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=19 }},
  };
  Insts* insts1 = setup_insts(inst_arr1, 5);
  int res1 = exec(&s, &h, NULL, insts1);
  cleanup_insts(insts1);
  assert_int(res1, ==, 0);
  assert_int(s.len, ==, 6);
  assert_int(s.sp, ==, 5);

  // Deletion
  Inst inst_arr2[] = {
    { .code=POP, .mem={ .seg=LOC, .offset=0 }},
    { .code=POP, .mem={ .seg=LOC, .offset=1 }},
    { .code=POP, .mem={ .seg=LOC, .offset=2 }},
    { .code=ADD },
  };
  Insts* insts2 =  setup_insts(inst_arr2, 4);
  int res2 = exec(&s, &h, NULL, insts2);
  del_heap(h);
  cleanup_insts(insts2);
  assert_int(res2, ==, 0);
  assert_int(s.len, ==, 2);
  assert_int(s.sp, ==, 1);
  assert_int(s.ops[s.sp - 1], ==, 18);

  del_stack(s);
  return MUNIT_OK;
}

TEST(segment_addressing_works) {
  Inst set_arr[] = {
    { .code=PUSH, .mem={ .seg=CONST, .offset=2207}},
    { .code=POP, .mem={ .seg=ARG, .offset=0 }},
  };
  Inst check_arr[] = {
    { .code=PUSH, .mem={ .seg=ARG, .offset=0 }},
  };

  Insts* set_insts = setup_insts(set_arr, 2);
  Insts* check_insts = setup_insts(check_arr, 1);

  for (int seg = ARG; seg <= TMP; seg++) {
    if (seg == CONST)  // Skip pseudo segment.
      continue;
    Stack s = new_stack();
    Heap h = new_heap();
    set_arr[1].mem.seg = (Segment) seg;
    check_arr[0].mem.seg = (Segment) seg;
    int set_res = exec(&s, &h, NULL, set_insts);
    assert_int(set_res, ==, 0);
    assert_int(s.sp, ==, 0);
    int check_res = exec(&s, &h, NULL, check_insts);
    assert_int(check_res, ==, 0);
    assert_int(s.ops[s.sp - 1], ==, 2207);
    del_stack(s);
    del_heap(h);
  }

  cleanup_insts(set_insts);
  cleanup_insts(check_insts);

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
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 3);
    int res = exec(&s, &h, NULL, insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(s.ops[s.sp - 1], ==, 9);
    assert_int(s.ops[s.sp - 2], ==, 65535);
    cleanup_insts(insts);
    del_stack(s);
    del_heap(h);
  } {
    // Stack should not change if a pop's
    // address would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=POP, .mem={ .seg=PTR, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=POP, .mem={ .seg=THIS, .offset=1 }},
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 4);
    int res = exec(&s, &h, NULL, insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(s.sp, ==, 1);
    assert_int(s.ops[s.sp - 1], ==, 1);
    cleanup_insts(insts);
    del_stack(s);
    del_heap(h);
  } {
    // Stack should not change if a push's
    // address would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=65535 }},
      { .code=POP, .mem={ .seg=PTR, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=PUSH, .mem={ .seg=THIS, .offset=1 }},
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 4);
    int res = exec(&s, &h, NULL, insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(s.sp, ==, 1);
    assert_int(s.ops[s.sp - 1], ==, 1);
    cleanup_insts(insts);
    del_stack(s);
    del_heap(h);
  } {
    // Stack should not change if a pop
    // tried to access an invald pointer
    // segment.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=POP, .mem={ .seg=PTR, .offset=2 }},
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 2);
    int res = exec(&s, &h, NULL, insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(s.sp, ==, 1);
    assert_int(s.ops[s.sp - 1], ==, 1);
    cleanup_insts(insts);
    del_stack(s);
    del_heap(h);
  } {
    // The stack should not change if the
    // subtraction would have overflowed.
    Inst inst_arr[] = {
      { .code=PUSH, .mem={ .seg=CONST, .offset=0 }},
      { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
      { .code=SUB },
    };
    Stack s = new_stack();
    Heap h = new_heap();
    Insts* insts = setup_insts(inst_arr, 3);
    int res = exec(&s, &h, NULL, insts);
    assert_int(res, ==, EXEC_ERR);
    assert_int(s.ops[s.sp - 1], ==, 1);
    assert_int(s.ops[s.sp - 2], ==, 0);
    cleanup_insts(insts);
    del_stack(s);
    del_heap(h);
  }

  return MUNIT_OK;
}

TEST(can_multiply_two_numbers) {
  // `floor(sqrt(65535))` is 255 so if neither operand
  // ever exceeds 255, then the result will never overflow.
  uint16_t a = munit_rand_int_range(0, 255);
  uint16_t b = munit_rand_int_range(0, 255);
  Inst inst_arr[] = {
    // Initialize the counter.
    { .code=PUSH, .mem={ .seg=CONST, .offset=a }},
    { .code = POP, .mem={ .seg=LOC, .offset=2 }},
    // Add `b` to the accumulator in `local 1`.
    { .code=PUSH, .mem={ .seg=LOC, .offset=1 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=b }},
    { .code=ADD },
    { .code=POP, .mem={ .seg=LOC, .offset=1 }},
    // Decrease the counter in `local 2`
    { .code=PUSH, .mem={ .seg=LOC, .offset=2 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=1 }},
    { .code=SUB },
    { .code=POP, .mem={ .seg=LOC, .offset=2 }},
    // Jump if the counter is 0.
    { .code=PUSH, .mem={ .seg=LOC, .offset=2 }},
    { .code=PUSH, .mem={ .seg=CONST, .offset=0 }},
    { .code=EQ },
    { .code=IF_GOTO, .ident="end" },
    // Repeat the loop
    { .code=GOTO, .ident="mult_loop_start" },
    // Push the result
    { .code=PUSH, .mem={ .seg=LOC, .offset=1 }},
  };
  SymbolTable st = new_st();
  assert_int(insert_st(&st, mk_key("mult_loop_start", SBT_LABEL), mk_lbval(2)), ==, INRES_OK);
  assert_int(insert_st(&st, mk_key("end", SBT_LABEL), mk_lbval(15)), ==, INRES_OK);

  Insts* insts = setup_insts(inst_arr, 16);
  Stack s = new_stack();
  Heap h = new_heap();
  assert_int(exec(&s, &h, &st, insts), ==, 0);
  assert_int(s.ops[s.sp - 1], ==, a * b);

  del_st(st);
  cleanup_insts(insts);
  del_stack(s);
  del_heap(h);

  return MUNIT_OK;
}

MunitTest exec_tests[] = {
  REG_TEST(correct_stack_errors),
  REG_TEST(correct_memory_errors),
  REG_TEST(arithmetic_errors),
  REG_TEST(arithmetic_instructions),
  REG_TEST(stack_doesnt_change_on_error),
  REG_TEST(stack_buildup_works),
  REG_TEST(segment_addressing_works),
  REG_TEST(can_multiply_two_numbers),
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

