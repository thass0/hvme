#include "hvme.h"
#include "err.h"
#include "prog.h"
#include "exec.h"

#include <string.h>
#include <argp.h>

int run_hvme(int argc, const char* argv[]) {
  if (argc <= 1) {
    err("Can't execute 0 files!");
    return 1;
  } else {
    Program* prog = make_prog(argc - 1, argv + 1);
    if (prog == NULL)
      return 1;

    int ret = exec_prog(prog);

    if (ret == 0)
      printf("%d\n", prog->stack.sp > 0
        ? prog->stack.ops[prog->stack.sp - 1]
        : 0
      );

    del_prog(prog);

    return ret;
  }
}
