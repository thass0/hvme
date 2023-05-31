#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include <stdio.h>

#include "../src/parse.h"
#include "utils.h"

static inline Items* setup_items(Item* item_arr, size_t len) {
  Items* items = new_items(NULL);
  free(items->cell);
  items->cell = item_arr;
  items->idx = len, items->len = len;
  return items;
}

static inline void cleanup_items(Items* items) {
  // `filename` remains `NULL` in the current
  // tests and `cell` is a static array and
  // cannot be freed.
  free(items);
}

TEST(correct_parse_errors) {
  {
    Item it[] = {{.t=TK_PUSH}, {.t=TK_CONST, .pos={.ln=0, .cl=5}}};
    Items* items = setup_items(it, 2);
    Insts* insts = parse(items, NULL);
    cleanup_items(items);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:15 | push constant ???\n"
 " 2:15 |               ^^^",  16, stderr), ==, 1);
  } {
    Item it[] = {{.t=TK_PUSH}, {.t=TK_LOC}, {.t=TK_ARG, .pos={.ln=0, .cl=11}}};
    Items* items = setup_items(it, 3);
    Insts* insts = parse(items, NULL);
    cleanup_items(items);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:12 | push local argument\n"
 " 2:12 |            ^^^^^^^^",  16, stderr), ==, 1);
  } {
    Item it[] = {{.t=TK_PUSH}, {.t=TK_LOC}, {.t=TK_ARG, .pos={.ln=0, .cl=11}}, {.t=TK_CONST}, {.t=TK_LOC}};
    Items* items = setup_items(it, 3);
    Insts* insts = parse(items, NULL);
    cleanup_items(items);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:12 | push local argument\n"
 " 2:12 |            ^^^^^^^^",  16, stderr), ==, 1);
  } {
    Item it[] = {{.t=TK_PUSH}};
    Items* items = setup_items(it, 1);
    Insts* insts = parse(items, NULL);
    cleanup_items(items);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected a segment\n"
 " 1:6 | push ??? ???\n"
 " 2:6 |      ^^^",  16, stderr), ==, 1);
  } {
    Item it[] = {{.t=TK_PUSH}, {.t=TK_POP, .pos={.ln=0, .cl=5}}, {.t=TK_PUSH}};
    Items* items = setup_items(it, 3);
    Insts* insts = parse(items, NULL);
    cleanup_items(items);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected a segment\n"
 " 1:6 | push pop ???\n"
 " 2:6 |      ^^^",  16, stderr), ==, 1);
  }
  return MUNIT_OK;
}

TEST(parse_valid_insts) {
  // This list of instructions should test each instruction
  // and each segement at least once. Additionally some offsets
  // are tested, too.
  Item input[] = {
    { .t=TK_PUSH }, { .t=TK_CONST }, { .t=TK_UINT, .uilit=6 },
    { .t=TK_POP }, { .t=TK_LOC }, { .t=TK_UINT, .uilit=4109 },
    { .t=TK_ADD }, { .t=TK_SUB }, { .t=TK_NEG },
    { .t=TK_EQ }, { .t=TK_GT }, { .t=TK_LT },
    { .t=TK_AND }, { .t=TK_OR }, { .t=TK_NOT },
    { .t=TK_PUSH }, { .t=TK_ARG }, { .t=TK_UINT, .uilit=9 },
    { .t=TK_POP }, { .t=TK_STAT }, { .t=TK_UINT, .uilit=65535 },
    { .t=TK_PUSH }, { .t=TK_THIS }, { .t=TK_UINT, .uilit=859 },
    { .t=TK_POP }, { .t=TK_THAT }, { .t=TK_UINT, .uilit=47999 },
    { .t=TK_PUSH }, { .t=TK_PTR }, { .t=TK_UINT, .uilit=9385 },
    { .t=TK_POP }, { .t=TK_TMP }, { .t=TK_UINT, .uilit=31 },
  };
  Items* items = setup_items(input, 33);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_not_null(insts);
  assert_int(insts->cell[0].code, ==, PUSH);
  assert_int(insts->cell[0].mem.seg, ==, CONST);
  assert_int(insts->cell[0].mem.offset, ==, 6);
  assert_int(insts->cell[1].code, ==, POP);
  assert_int(insts->cell[1].mem.seg, ==, LOC);
  assert_int(insts->cell[1].mem.offset, ==, 4109);
  assert_int(insts->cell[2].code, ==, ADD);
  assert_int(insts->cell[3].code, ==, SUB);
  assert_int(insts->cell[4].code, ==, NEG);
  assert_int(insts->cell[5].code, ==, EQ);
  assert_int(insts->cell[6].code, ==, GT);
  assert_int(insts->cell[7].code, ==, LT);
  assert_int(insts->cell[8].code, ==, AND);
  assert_int(insts->cell[9].code, ==, OR);
  assert_int(insts->cell[10].code, ==, NOT);
  assert_int(insts->cell[11].code, ==, PUSH);
  assert_int(insts->cell[11].mem.seg, ==, ARG);
  assert_int(insts->cell[11].mem.offset, ==, 9);
  assert_int(insts->cell[12].code, ==, POP);
  assert_int(insts->cell[12].mem.seg, ==, STAT);
  assert_int(insts->cell[12].mem.offset, ==, 65535);
  assert_int(insts->cell[13].code, ==, PUSH);
  assert_int(insts->cell[13].mem.seg, ==, THIS);
  assert_int(insts->cell[13].mem.offset, ==, 859);
  assert_int(insts->cell[14].code, ==, POP);
  assert_int(insts->cell[14].mem.seg, ==, THAT);
  assert_int(insts->cell[14].mem.offset, ==, 47999);
  assert_int(insts->cell[15].code, ==, PUSH);
  assert_int(insts->cell[15].mem.seg, ==, PTR);
  assert_int(insts->cell[15].mem.offset, ==, 9385);
  assert_int(insts->cell[16].code, ==, POP);
  assert_int(insts->cell[16].mem.seg, ==, TMP);
  assert_int(insts->cell[16].mem.offset, ==, 31);
  del_insts(insts);
  return MUNIT_OK;
}

TEST(reject_segments_start) {
{
  Item it[] = {{ .t=TK_ARG }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_LOC }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_STAT }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_CONST }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_THIS }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_THAT }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_PTR }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {
  Item it[] = {{ .t=TK_TMP }};
  Items* items = setup_items(it, 1);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
}
  return MUNIT_OK;
}

TEST(reject_offsets_start) {
  Item it[1];
  Items* items = setup_items(it, 1);
  for (int i = 0; i < 20; i++) {
    items->cell[0] = (Item) { .t=TK_UINT, .uilit=RAND_OFFSET() };
    Insts* insts = parse(items, NULL);
    assert_ptr_equal(NULL, insts);
    del_insts(insts);
  }
  cleanup_items(items);

  return MUNIT_OK;
}

TEST(reject_incomplete_mem)  {
{  // Mem and segment followed by another instruction.
  Item it[] = {{ .t=TK_PUSH }, { .t=TK_CONST }, { .t=TK_ADD }};
  Items* items = setup_items(it, 3);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {  // Mem followed by an offset withtout a segment.
  Item it[] = {{ .t=TK_POP }, { .t=TK_UINT, .uilit=RAND_OFFSET()}};
  Items* items = setup_items(it, 2);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
} {  // Mem followed by another instruction
  Item it[] = {{ .t=TK_POP }, { .t=TK_EQ }};
  Items* items = setup_items(it, 2);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
}
  return MUNIT_OK;
}

TEST(reject_premature_end) {
  Item it[] = {{ .t=TK_POP }, { .t=TK_LOC }};
  Items* items = setup_items(it, 2);
  Insts* insts = parse(items, NULL);
  cleanup_items(items);
  assert_ptr_equal(NULL, insts);
  del_insts(insts);
  return MUNIT_OK;
}

TEST(parse_fills_st) {
  SymbolTable st = new_st();
  Item it[] = {
    {.t=TK_PUSH},
    {.t=TK_CONST},
    {.t=TK_UINT, .uilit=RAND_OFFSET()},
    {.t=TK_LABEL},
    {.t=TK_IDENT, .ident="random_ident"},
    {.t=TK_POP},
    {.t=TK_LOC},
    {.t=TK_UINT, .uilit=0},
    {.t=TK_LABEL},
    {.t=TK_IDENT, .ident="another_ident"},
  };
  Items* items = setup_items(it, 10);
  Insts* insts = parse(items, &st);
  cleanup_items(items);
  assert_ptr_not_null(insts);
  {
    struct SymKey key = mk_key("random_ident", SBT_LABEL);
    struct SymVal val;
    assert_int(get_st(st, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, 1);
  }
  {
    struct SymKey key = mk_key("another_ident", SBT_LABEL);
    struct SymVal val;
    assert_int(get_st(st, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, 2);
  }
  del_st(st);
  del_insts(insts);
  return MUNIT_OK;
}

TEST(parse_function) {
  SymbolTable st = new_st();
  int nlocals = RAND_OFFSET();
  Item item_arr[] = {
    { .t=TK_PUSH },
    { .t=TK_CONST },
    { .t=TK_UINT, .uilit=RAND_OFFSET() },
    { .t=TK_FUNC },
    { .t=TK_IDENT, .ident="blah_function" },
    { .t=TK_UINT, .uilit=nlocals },
  };
  Items* items = setup_items(item_arr, 6);
  Insts* insts = parse(items, &st);
  cleanup_items(items);
  assert_ptr_not_null(insts);
  // assert_int(insts->cell[1].code, ==, INIT_FUNC);
  // assert_int(insts->cell[1].nlocals, ==, nlocals);
  {
    struct SymKey key = mk_key("blah_function", SBT_FUNC);
    struct SymVal val;
    assert_int(get_st(st, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, 1);
    assert_int(val.nlocals, ==, nlocals);
  }

  del_st(st);
  del_insts(insts);
  return MUNIT_OK;
}

MunitTest parse_tests[] = {
  REG_TEST(parse_valid_insts),
  REG_TEST(reject_segments_start),
  REG_TEST(reject_offsets_start),
  REG_TEST(reject_incomplete_mem),
  REG_TEST(reject_premature_end),
  REG_TEST(correct_parse_errors),
  REG_TEST(parse_fills_st),
  REG_TEST(parse_function),
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};
