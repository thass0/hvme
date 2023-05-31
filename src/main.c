#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "scan.h"
#include "repl.h"
#include "parse.h"
#include "exec.h"
#include "st.h"

int main(int argc, const char* argv[]) {
  const char* filename = parse_args(argc, argv);
  if (filename == NULL) {
    return repl();
  } else {
    warn_file_ext(filename);

    Items* items = scan_blocks(filename);
    if (items == NULL)
      return 1;

    SymbolTable st = new_st();
    Insts* insts = parse(items, &st);
    if (insts == NULL) {
      del_st(st);
      return 1;
    }
    del_items(items);

    // Add startup code to `insts`.
    Insts* startup = gen_startup(&st);
    if (startup == NULL) {
      del_st(st);
      del_insts(insts);
      return 1;
    }
    // Insert the startup code before the instructions.
    cpy_insts(startup, insts);
    st.offset += 2;  /* Inserting the startup code before
                     the actual instructions offsets all
                     their addresses by 2. */
    del_insts(insts);

    Stack stack = new_stack();
    Heap heap = new_heap();
    int ret = exec(&stack, &heap, &st, startup);
    del_insts(startup);
    del_st(st);
    del_heap(heap);

    if (ret == 0)
      printf("%d\n", stack.sp > 0
        ? stack.ops[stack.sp - 1]
        : 0
      );
    del_stack(stack);

    return ret;
  }
}
