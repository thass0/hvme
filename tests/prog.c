#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include <stdio.h>

#include "../src/prog.h"
#include "utils.h"

TEST(empty_prog_is_correct) {
  const char* argv[] = {""};
  Program* prog = make_prog(0, argv);
  assert_int(prog->nfiles, ==, 1);
  assert_int(prog->fi, ==, 0);
  assert_string_equal(prog->files[0].filename, "<system>");
  assert_int(prog->files[0].insts.cell[0].code, ==, PUSH);
  assert_int(prog->files[0].insts.cell[0].mem.seg, ==, CONST);
  assert_int(prog->files[0].insts.cell[0].mem.offset, ==, 0);
  assert_int(prog->files[0].insts.cell[1].code, ==, CALL);
  assert_string_equal(prog->files[0].insts.cell[1].ident, "Sys.init");
  assert_int(prog->files[0].insts.cell[1].nargs, ==, 1);
  del_prog(prog);

  return MUNIT_OK;
}

TEST(single_file_prog_is_correct) {
  char fn[] = "/tmp/XXXXXX";
  setup_tmp(fn,
       "function Sys.init 0\n"
      "push constant 2\n"
      "push argument 0\n"
      "add\n"
      "return\n");
  const char* argv[] = {fn};
  Program* prog = make_prog(1, argv);
  assert_ptr_not_null(prog);
  assert_int(prog->nfiles, ==, 2);
  assert_int(prog->fi, ==, 0);
  assert_string_equal(prog->files[0].filename, "<system>");
  /* Check instructions of the given file */
  assert_int(prog->files[1].insts.cell[0].code, ==, PUSH);
  assert_int(prog->files[1].insts.cell[0].mem.seg, ==, CONST);
  assert_int(prog->files[1].insts.cell[0].mem.offset, ==, 2);
  assert_int(prog->files[1].insts.cell[1].code, ==, PUSH);
  assert_int(prog->files[1].insts.cell[1].mem.seg, ==, ARG);
  assert_int(prog->files[1].insts.cell[1].mem.offset, ==, 0);
  assert_int(prog->files[1].insts.cell[2].code, ==, ADD);
  assert_int(prog->files[1].insts.cell[3].code, ==, RET);
  /* Check symbol table content */
  SymVal val;
  SymKey key = mk_key("Sys.init", SBT_FUNC);
  assert_int(get_st(prog->files[1].st, &key, &val), ==, GTRES_OK);
  /* Check filename */
  assert_string_equal(prog->files[1].filename, fn);
  del_prog(prog);
  
  return MUNIT_OK;
}

TEST(multi_file_prog_is_correct) {
  char fn1[] = "/tmp/XXXXXX";
  setup_tmp(fn1,"label wow\npush constant 0\n");
  char fn2[] = "/tmp/XXXXXX";
  setup_tmp(fn2,"push constant 1\n");
  char fn3[] = "/tmp/XXXXXX";
  setup_tmp(fn3,"push constant 2\nlabel cool\n");
  const char* argv[] = { fn1, fn2, fn3 };
  Program* prog = make_prog(3, argv);
  assert_ptr_not_null(prog);
  assert_int(prog->nfiles, ==, 4);
  assert_int(prog->fi, ==, 0);
  assert_string_equal(prog->files[0].filename, "<system>");
  /* First file */
  assert_int(prog->files[1].insts.cell[0].code, ==, PUSH);
  assert_int(prog->files[1].insts.cell[0].mem.seg, ==, CONST);
  assert_int(prog->files[1].insts.cell[0].mem.offset, ==, 0);
  SymVal val1;
  SymKey key1 = mk_key("wow", SBT_LABEL);
  assert_int(get_st(prog->files[1].st, &key1, &val1), ==, GTRES_OK);
  assert_string_equal(prog->files[1].filename, fn1);
  /* Second file */
  assert_int(prog->files[2].insts.cell[0].code, ==, PUSH);
  assert_int(prog->files[2].insts.cell[0].mem.seg, ==, CONST);
  assert_int(prog->files[2].insts.cell[0].mem.offset, ==, 1);
  assert_string_equal(prog->files[2].filename, fn2);
  /* Third file */
  assert_int(prog->files[3].insts.cell[0].code, ==, PUSH);
  assert_int(prog->files[3].insts.cell[0].mem.seg, ==, CONST);
  assert_int(prog->files[3].insts.cell[0].mem.offset, ==, 2);
  SymVal val3;
  SymKey key3 = mk_key("cool", SBT_LABEL);
  assert_int(get_st(prog->files[3].st, &key3, &val3), ==, GTRES_OK);
  assert_string_equal(prog->files[3].filename, fn3);

  del_prog(prog);

  return MUNIT_OK;
}

TEST(abort_all_on_error) {
  char fn1[] = "/tmp/XXXXXX";
  setup_tmp(fn1,"label wow\npush constant 0\n");
  char fn2[] = "/tmp/XXXXXX";
  setup_tmp(fn2,"push constant 1\n");
  char fn3[] = "/tmp/XXXXXX"; // Syntax error:
  setup_tmp(fn3,"push constant 2\nlabll cool\n");
  const char* argv[] = { fn1, fn2, fn3 };
  Program* prog = make_prog(3, argv);
  assert_ptr_equal(prog, NULL);
  /* Use Asan here to verify there
   * isn't a memory leak happening. */
  return MUNIT_OK;
}

MunitTest prog_tests[] = {
  REG_TEST(empty_prog_is_correct),
  REG_TEST(single_file_prog_is_correct),
  REG_TEST(multi_file_prog_is_correct),
  REG_TEST(abort_all_on_error),
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
