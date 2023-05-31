#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

#include "../src/st.h"
#include "utils.h"

TEST(st_io_works) {
  SymbolTable s = new_st();
  char* labels[] = {"This", "is", "a", "significant", "label.", NULL};
  char* functions[] = {"This", "function", "is", "very", "important", NULL};

  for (int i = 0; labels[i] != NULL; i++)
    insert_st(&s, mk_key(labels[i], SBT_LABEL), mk_lbval(i));

  for (int j = 0; functions[j] != NULL; j++)
    insert_st(&s, mk_key(functions[4 - j], SBT_FUNC), mk_lbval(j));
  
  struct SymVal val;
  struct SymKey key;
  for (int i = 0; labels[i] != NULL; i++) {
    key = mk_key(labels[i], SBT_LABEL);
    assert_int(get_st(s, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, i);
  }
  
  for (int j = 0; functions[j] != NULL; j++) {
    key = mk_key(functions[4 - j], SBT_FUNC);
    assert_int(get_st(s, &key, &val), ==, GTRES_OK);
    assert_int(val.inst_addr, ==, j);
  }

  del_st(s);
  
  return MUNIT_OK;
}

TEST(data_collisions_are_rejected) {
  SymbolTable s = new_st();
  size_t num_inst_a =  324;  // Any number.
  size_t num_inst_b =  7806;  // Any number not equal to `num_inst_a`.

  struct SymKey key = mk_key("someIdent", SBT_LABEL);
  assert_int(insert_st(&s, key, mk_lbval(num_inst_a)), ==, INRES_OK);
  assert_int(insert_st(&s, key, mk_lbval(num_inst_b)), ==, INRES_EXISTS);
  struct SymVal val;
  assert_int(get_st(s, &key, &val), ==, GTRES_OK);
  assert_int(val.inst_addr, ==, num_inst_a);

  del_st(s);

  return MUNIT_OK;
}

MunitTest st_tests[] = {
  REG_TEST(st_io_works),
  REG_TEST(data_collisions_are_rejected),
  { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }  
};
