// [µnit test framework](https://github.com/nemequ/munit) 
#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit.h"

extern MunitTest scan_tests[];
extern MunitTest parse_tests[];
extern MunitTest exec_tests[];
extern MunitTest st_tests[];

static MunitSuite suites[] = {
  {
    "/scan",
    scan_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
  },
  {
    "/parse",
    parse_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
  },
  {
    "/exec",
    exec_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
  },
  {
    "/st",
    st_tests,
    NULL,
    1,
    MUNIT_SUITE_OPTION_NONE
  },
  { NULL, NULL, NULL, 0, MUNIT_SUITE_OPTION_NONE }
};

static const MunitSuite suite = {
  "/vm",
  NULL,
  suites,
  1,
  MUNIT_SUITE_OPTION_NONE,
};

int main(int argc, char* const* argv) {
  return munit_suite_main(&suite, NULL, argc, argv);
}
