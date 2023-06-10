#include "hvme.h"
#include "err.h"
#include "prog.h"
#include "exec.h"

#include <string.h>
#include <stdio.h>

int run_hvme(int argc, const char* argv[]) {
  if (argc <= 1) {
    err("Can't execute 0 files!");
    return 1;
  } else {
    Program* prog = make_prog(argc - 1, argv + 1);
    if (prog == NULL)
      return 1;

    int ret = exec_prog(prog);
    del_prog(prog);

    /* If `ret != 0` we have an error and
     * the output will already be formatted
     * correctly. */
    if (ret == 0) clean_stdout();
    
    return ret;
  }
}
