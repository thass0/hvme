#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "prog.h"
#include "exec.h"

int main(int argc, const char* argv[]) {
  if (argc <= 1) {
    err("Can't execute 0 files!");
    return 1;
  } else {
    struct Program* prog = make_prog(argc - 1, argv + 1);
    if (prog == NULL)
      return 1;

    int ret = exec(prog);

    if (ret == 0)
      printf("%d\n", prog->stack.sp > 0
        ? prog->stack.ops[prog->stack.sp - 1]
        : 0
      );

    del_prog(prog);

    return ret;
  }
}
