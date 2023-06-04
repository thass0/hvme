#include "repl.h"
#include "utils.h"
#include "scan.h"
#include "parse.h"
#include "exec.h"
#include "st.h"

#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <assert.h>

#define CTRL_D_MSG "Press `Ctrl+D` to Exit."

static sigjmp_buf repl_start;

static void sigint_msg_ignore(int signum) {
  (void) signum;
  csi("G");  // Move to beginning of current line.
  csi("K");  // Clear line from cursor to end.
  // Replace current line with `CTRL_D_MSG`.
  puts(CTRL_D_MSG);
  siglongjmp(repl_start, 1);
}

// Print a snapshot of the stack's top.
// `depth` must be larger than `0`.
static void print_stack_snapshot(Stack stack, unsigned int depth) {
  assert(depth > 0);
  size_t start = stack.sp > depth - 1 ? stack.sp - depth : 0;
  start > 0
    ? printf(" [{ %lu }, %d", stack.sp - depth, stack.ops[start])
    : start < stack.sp
      ? printf(" [ %d", stack.ops[start])
      : printf(" [");
  for (size_t i = start + 1; i < stack.sp; i++)
    printf(", %d", stack.ops[i]);
  printf(" ]\n");
}

int repl(void) {

  struct sigaction new_act;
  new_act.sa_handler = sigint_msg_ignore;
  sigemptyset(&new_act.sa_mask);
  new_act.sa_flags = SA_SIGINFO;
  if (sigaction(SIGINT, &new_act, NULL) == -1) {
    err("failed to register Ctrl+C handler");
    return 1;
  }
  
  puts("VME Version " VME_VERSION);
  puts(CTRL_D_MSG);

  SymbolTable st = new_st();
  Insts* ginsts = new_insts(NULL);
  /* If a `goto` or a `if-goto` instruction is
   * entered in the REPL, then nothing will happend
   * on it's initial execution because it's missing
   * the other instructions to jump to. Therefore
   * `eval_pending` is set to 1 end the user is shown
   * a message that the jump will be executed with the
   * next instruction.
   */
  int eval_pending = 0;
  /* `stack`, `heap` and `insts` are effectively local to each
   * iteration of the REPL. Hence, they are always deleted
   * and allocated again. They are only stored here outside
   * of the loop so that they don't leak on interrupts.
   */
  Stack stack = {0};
  Heap heap = {0};
  Insts* insts = NULL;

  /*
  TODO: Refactor the REPL
  - [ ] Always re-evaluate everything together, never along.
  - [ ] Decouple `Insts` from parse to enable startup code.
  */

  // Any non-zero `savemask` restores the signal mask.
  sigsetjmp(repl_start, 1);

  while (1) {
    if (eval_pending == 1) {
      puts(" (jump(s) pending)");
    }

    char* input = readline("(vme) ");
    if (input == NULL) {
      del_stack(stack);
      del_heap(heap);
      del_insts(insts);
      del_st(st);
      del_insts(ginsts);
      printf("\n");
      return 0;  // Ctrl+D
    }

    // Terminate the input with a newline.
    size_t len = strlen(input) + 2;
    input = (char*) realloc (input, len);
    assert(input != NULL);
    input[len - 2] = '\n';  // Overwrites original '\0'.
    input[len - 1] = '\0';

    add_history(input);

    // Scan new input.
    Items* items = new_items(NULL);
    ssize_t scan_res = scan(items, input, strlen(input));
    free(input);

    /* If the scanner detects trailing characters at the end of
     * the given block which it cannot scan successfully, then
     * it might usually return a value above 0, indicating the
     * number of characters which remained unscanned. However,
     * this will never happen here because we always add newline
     * character to the end of the input before passing it to
     * `scan`. Doing that enables the scanner to correctly detect
     * all errors.
     */
    assert(scan_res <= 0);

    if (scan_res == SCAN_ERR) {
      puts("Lexing failed: discarding input");
     continue;
    }

    // Parse new input.
    del_insts(insts);  // Allocate new instructions.
    insts = parse(items, &st);
    del_items(items);

    if (insts == NULL) {
      puts("Parsing failed: discarding input");
      continue;
    }

    // Re-evaluate the entire repl.
    del_stack(stack);  // Allocate new stack.
    del_heap(heap);
    stack = new_stack();
    heap = new_heap();
    // Evaluate all instructions up to this point first.
    int res_orig = exec(&stack, &heap, &st, ginsts);

    if (res_orig == EXEC_ERR) {
      // Program became invalid
      puts("Program became invalid  D :");
      break;
    }

    if (
      insts->cell[0].code == GOTO ||
      insts->cell[0].code == IF_GOTO ||
      insts->cell[0].code == CALL
    ) {
      eval_pending = 1;
    } else {
      if (eval_pending == 1)
        printf(" (evaluated jump)");
      eval_pending = 0;
    }

    // Evaluate new input.
    int res_new = exec(&stack, &heap, &st, insts);

    if  (res_new == EXEC_ERR) {
      // Discard new input since it failed to evaluate.
      puts("Evaluation failed: discarding input");
      eval_pending = 0;
      continue;
    } else {
      // Append new instructions to total.
      cpy_insts(ginsts, insts);
    }

    // Print stack after entire evaluation.
    print_stack_snapshot(stack, 4);
  }

  del_st(st);
  del_insts(ginsts);
  del_stack(stack);
  del_heap(heap);
  del_insts(insts);

  return 0;
}
