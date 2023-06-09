#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include <stdio.h>

#include "../src/parse.h"
#include "utils.h"

static inline Tokens setup_tokens(Token* token_arr, size_t len) {
  Tokens tokens = new_tokens(NULL);
  free(tokens.cell);
  tokens.cell = token_arr;
  tokens.idx = len, tokens.len = len;
  return tokens;
}

TEST(correct_parse_errors) {
  {
    Token it[] = {{.t=TK_PUSH}, {.t=TK_CONST, .pos={.ln=0, .cl=5}}};
    Tokens tokens = setup_tokens(it, 2);
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:15 | push constant ???\n"
 " 2:15 |               ^^^",  16, stderr), ==, 1);
  } {
    Token it[] = {{.t=TK_PUSH}, {.t=TK_LOC}, {.t=TK_ARG, .pos={.ln=0, .cl=11}}};
    Tokens tokens = setup_tokens(it, 3);
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:12 | push local argument\n"
 " 2:12 |            ^^^^^^^^",  16, stderr), ==, 1);
  } {
    Token it[] = {{.t=TK_PUSH}, {.t=TK_LOC}, {.t=TK_ARG, .pos={.ln=0, .cl=11}}, {.t=TK_CONST}, {.t=TK_LOC}};
    Tokens tokens = setup_tokens(it, 3);
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected an offset\n"
 " 1:12 | push local argument\n"
 " 2:12 |            ^^^^^^^^",  16, stderr), ==, 1);
  } {
    Token it[] = {{.t=TK_PUSH}};
    Tokens tokens = setup_tokens(it, 1);
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
    del_insts(insts);
    assert_int(check_stream(
 "wrong token, expected a segment\n"
 " 1:6 | push ??? ???\n"
 " 2:6 |      ^^^",  16, stderr), ==, 1);
  } {
    Token it[] = {{.t=TK_PUSH}, {.t=TK_POP, .pos={.ln=0, .cl=5}}, {.t=TK_PUSH}};
    Tokens tokens = setup_tokens(it, 3);
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
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
  Token input[] = {
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
  Tokens tokens = setup_tokens(input, 33);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_OK);
  assert_int(insts.cell[0].code, ==, PUSH);
  assert_int(insts.cell[0].mem.seg, ==, CONST);
  assert_int(insts.cell[0].mem.offset, ==, 6);
  assert_int(insts.cell[1].code, ==, POP);
  assert_int(insts.cell[1].mem.seg, ==, LOC);
  assert_int(insts.cell[1].mem.offset, ==, 4109);
  assert_int(insts.cell[2].code, ==, ADD);
  assert_int(insts.cell[3].code, ==, SUB);
  assert_int(insts.cell[4].code, ==, NEG);
  assert_int(insts.cell[5].code, ==, EQ);
  assert_int(insts.cell[6].code, ==, GT);
  assert_int(insts.cell[7].code, ==, LT);
  assert_int(insts.cell[8].code, ==, AND);
  assert_int(insts.cell[9].code, ==, OR);
  assert_int(insts.cell[10].code, ==, NOT);
  assert_int(insts.cell[11].code, ==, PUSH);
  assert_int(insts.cell[11].mem.seg, ==, ARG);
  assert_int(insts.cell[11].mem.offset, ==, 9);
  assert_int(insts.cell[12].code, ==, POP);
  assert_int(insts.cell[12].mem.seg, ==, STAT);
  assert_int(insts.cell[12].mem.offset, ==, 65535);
  assert_int(insts.cell[13].code, ==, PUSH);
  assert_int(insts.cell[13].mem.seg, ==, THIS);
  assert_int(insts.cell[13].mem.offset, ==, 859);
  assert_int(insts.cell[14].code, ==, POP);
  assert_int(insts.cell[14].mem.seg, ==, THAT);
  assert_int(insts.cell[14].mem.offset, ==, 47999);
  assert_int(insts.cell[15].code, ==, PUSH);
  assert_int(insts.cell[15].mem.seg, ==, PTR);
  assert_int(insts.cell[15].mem.offset, ==, 9385);
  assert_int(insts.cell[16].code, ==, POP);
  assert_int(insts.cell[16].mem.seg, ==, TMP);
  assert_int(insts.cell[16].mem.offset, ==, 31);
  del_insts(insts);
  return MUNIT_OK;
}

TEST(reject_segments_start) {
{
  Token it[] = {{ .t=TK_ARG }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_LOC }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_STAT }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_CONST }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_THIS }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_THAT }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_PTR }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {
  Token it[] = {{ .t=TK_TMP }};
  Tokens tokens = setup_tokens(it, 1);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
}
  return MUNIT_OK;
}

TEST(reject_offsets_start) {
  Token it[1];
  Tokens tokens = setup_tokens(it, 1);
  for (int i = 0; i < 20; i++) {
    tokens.cell[0] = (Token) { .t=TK_UINT, .uilit=RAND_OFFSET() };
    Insts insts = new_insts(NULL);
    int parse_res = parse(&tokens, &insts, NULL);
    assert_int(parse_res, ==, PARSE_ERR);
    del_insts(insts);
  }

  return MUNIT_OK;
}

TEST(reject_incomplete_mem)  {
{  // Mem and segment followed by another instruction.
  Token it[] = {{ .t=TK_PUSH }, { .t=TK_CONST }, { .t=TK_ADD }};
  Tokens tokens = setup_tokens(it, 3);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {  // Mem followed by an offset withtout a segment.
  Token it[] = {{ .t=TK_POP }, { .t=TK_UINT, .uilit=RAND_OFFSET()}};
  Tokens tokens = setup_tokens(it, 2);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
} {  // Mem followed by another instruction
  Token it[] = {{ .t=TK_POP }, { .t=TK_EQ }};
  Tokens tokens = setup_tokens(it, 2);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
}
  return MUNIT_OK;
}

TEST(reject_premature_end) {
  Token it[] = {{ .t=TK_POP }, { .t=TK_LOC }};
  Tokens tokens = setup_tokens(it, 2);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, NULL);
  assert_int(parse_res, ==, PARSE_ERR);
  del_insts(insts);
  return MUNIT_OK;
}

TEST(parse_fills_st) {
  SymbolTable st = new_st();
  Token it[] = {
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
  Tokens tokens = setup_tokens(it, 10);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, &st);
  assert_int(parse_res, ==, PARSE_OK);
  {
    SymKey key = mk_key("random_ident", SBT_LABEL);
    SymVal val;
    assert_int(get_st(st, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, 1);
  }
  {
    SymKey key = mk_key("another_ident", SBT_LABEL);
    SymVal val;
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
  Token token_arr[] = {
    { .t=TK_PUSH },
    { .t=TK_CONST },
    { .t=TK_UINT, .uilit=RAND_OFFSET() },
    { .t=TK_FUNC },
    { .t=TK_IDENT, .ident="blah_function" },
    { .t=TK_UINT, .uilit=nlocals },
  };
  Tokens tokens = setup_tokens(token_arr, 6);
  Insts insts = new_insts(NULL);
  int parse_res = parse(&tokens, &insts, &st);
  assert_int(parse_res, ==, PARSE_OK);
  {
    SymKey key = mk_key("blah_function", SBT_FUNC);
    SymVal val;
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
