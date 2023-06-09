#include "exec.h"
#include "prog.h"
#include "st.h"
#include "utils.h"
#include "parse.h"

#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#define BIT16_LIMIT 65535

/* Execution error return location. */
static jmp_buf exec_env;

/* Errors (some of them are used more than once so
 * they are defined here to avoid different spelling
 * of the same error or something similar). */

#define STACK_UNDERFLOW_ERROR(pos) { \
  perr((pos), "stack underflow");    \
  longjmp(exec_env, EXEC_ERR);       \
}
#define POINTER_SEGMENT_ERROR(addr, pos) {        \
  perrf((pos), "can't access pointer segment at " \
       "`%lu` (max. index is 1)", (addr));        \
  longjmp(exec_env, EXEC_ERR);                    \
}
#define ADDR_OVERFLOW_ERROR(instp, addr) { \
  INST_STR(inst_str_buf, (instp));         \
  perrf((instp)->pos, "address overflow: " \
        "`%s` tries to access RAM at %lu", \
        inst_str_buf, (addr));             \
  longjmp(exec_env, EXEC_ERR);             \
}
#define STACK_ADDR_OVERFLOW_ERROR(instp, addr, max_addr) { \
  INST_STR(inst_str_buf, (instp));                         \
  perrf((instp)->pos, "stack address overflow: "           \
        "`%s` tries to access stack "                      \
       "at %lu (limit is at %lu)",                         \
        inst_str_buf, (addr), (max_addr));                 \
  longjmp(exec_env, EXEC_ERR);                             \
}
#define SEG_OVERFLOW_ERROR(instp, offset) {                 \
  INST_STR(inst_str_buf, (instp));                          \
  perrf((instp)->pos, "address overflow in `%s`: "          \
        "segment has %lu entries", inst_str_buf, (offset)); \
  longjmp(exec_env, EXEC_ERR);                              \
}
#define ADD_OVERFLOW_ERROR(x, y, sum, pos) {              \
  perrf((pos), "addition overflow: %d + %d = %d > 65535", \
    (x), (y), (sum));                                     \
  longjmp(exec_env, EXEC_ERR);                            \
}
#define SUB_UNDERFLOW_ERROR(x, y, pos) {                  \
  int diff = (int) (x) - (int) (y);                       \
  perrf((pos), "subtraction underflow: %d - %d = %d < 0", \
    (x), (y), diff);                                      \
  longjmp(exec_env, EXEC_ERR);                            \
}
#define CTRL_FLOW_ERROR(ident, pos) {                  \
  if (strcmp((ident), "Sys.init") == 0) {              \
    perr((pos), "can't jump to function `Sys.init`.\n" \
      SOLUTION_ARROW "Write it");                      \
  } else {                                             \
    perrf((pos), "can't jump to %s",                   \
      (ident));                                        \
  }                                                    \
  longjmp(exec_env, EXEC_ERR);                         \
}
#define NARGS_ERROR(nargs, sp, pos) {                           \
  perrf((pos), "given number of stack arguments (%d) is wrong." \
    " There are only %lu elements on the stack!",               \
    (nargs), (sp));                                             \
  longjmp(exec_env, EXEC_ERR);                                  \
}

void exec_pop(Inst inst, Stack* stack, Heap* heap, struct Memory* mem) {
  assert(stack != NULL);
  assert(mem != NULL);

  size_t offset = inst.mem.offset;

  switch(inst.mem.seg) {
    case ARG:
      if (
        offset < stack->arg_len &&
        // `offset < stack->arg_len` implicitly
        // means that `offset + stack->arg < stack->sp`
        // (same with loc).
        offset + stack->arg < stack->sp
      ) {
        if (!spop(stack, &stack->ops[offset + stack->arg]))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else {
        if (offset >= stack->arg_len) {
          SEG_OVERFLOW_ERROR(&inst, stack->arg_len);
        } else {
          STACK_ADDR_OVERFLOW_ERROR(&inst, offset + stack->arg, stack->sp);
        }
      }
      break;
    case LOC:
      if (
        offset < stack->lcl_len &&
        offset + stack->lcl < stack->sp
      ) {
        if (!spop(stack, &stack->ops[offset + stack->lcl]))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else {
        if (offset >= stack->lcl_len) {
          SEG_OVERFLOW_ERROR(&inst, stack->lcl_len);
        } else {
          STACK_ADDR_OVERFLOW_ERROR(&inst, offset + stack->arg, stack->sp);
        }
      }
      break;
    case STAT:
      if (offset < MEM_STAT_SIZE) {
        if (!spop(stack, &mem->_static[offset]))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else {
        SEG_OVERFLOW_ERROR(&inst, MEM_STAT_SIZE);
      }
      break;
    case CONST: {
        // `pop`ping to constant deletes the value.
        Word val;
        if (!spop(stack, &val))
          STACK_UNDERFLOW_ERROR(inst.pos);
      }
      break;
    case THIS:
      if (offset + heap->_this <= MEM_HEAP_SIZE) {
        // If we land here, then `offset + heap->_this` fits
        // a `uint16_t`.
        Word val;
        if (!spop(stack, &val)) STACK_UNDERFLOW_ERROR(inst.pos);
        heap_set(*heap, (Addr)(offset + heap->_this), val);
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->_this);
      }
      break;
    case THAT:
      if (offset + heap->that <= MEM_HEAP_SIZE) {
        Word val;
        if (!spop(stack, &val)) STACK_UNDERFLOW_ERROR(inst.pos);
        heap_set(*heap, (Addr)(offset + heap->that), val);
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->that);
      }
      break;
    case PTR:
      if (offset == 0) {
        if (!spop(stack, (Word*) &heap->_this))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else if (offset == 1) {
        if (!spop(stack, (Word*) &heap->that))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else {
        POINTER_SEGMENT_ERROR(offset, inst.pos);
      }
      return;
    case TMP:
      if (offset < MEM_TEMP_SIZE) {
        if (!spop(stack, &mem->tmp[offset]))
          STACK_UNDERFLOW_ERROR(inst.pos);
      } else {
        SEG_OVERFLOW_ERROR(&inst, MEM_TEMP_SIZE)
      }
      break;
  }
}

void exec_push(Inst inst, Stack* stack, Heap* heap, struct Memory* mem) {
  assert(stack != NULL);
  assert(heap != NULL);
  assert(mem != NULL);
  
  size_t offset = inst.mem.offset;

  switch(inst.mem.seg) {
    case ARG:
      if (
        offset < stack->arg_len &&
        offset + stack->arg < stack->sp
      ) {
        spush(stack, stack->ops[offset + stack->arg]);
      } else {
        if (offset >= stack->arg_len) {
          SEG_OVERFLOW_ERROR(&inst, stack->arg_len);
        } else {
          STACK_ADDR_OVERFLOW_ERROR(&inst, offset + stack->arg, stack->sp);
        }
      }
      break;
    case LOC:
      if (
        offset < stack->lcl_len &&
        offset + stack->lcl < stack->sp
      ) {
        spush(stack, stack->ops[offset + stack->lcl]);
      } else {
        if (offset >= stack->lcl_len) {
          SEG_OVERFLOW_ERROR(&inst, stack->lcl_len);
        } else {
          STACK_ADDR_OVERFLOW_ERROR(&inst, offset + stack->arg, stack->sp);
        }
      }
      break;
    case STAT:
      if (offset < MEM_STAT_SIZE) {
        spush(stack, mem->_static[offset]);
      } else {
        SEG_OVERFLOW_ERROR(&inst, MEM_STAT_SIZE);
      }
      break;
    case CONST:
      // The `constant` segment is a pseudo segment
      // used to get the constant value of `offset`.
      spush(stack, (Word) inst.mem.offset);  // `Word` is `uint16_t`.
      return;
    case THIS:
      if (offset + heap->_this <= MEM_HEAP_SIZE) {
        spush(stack, heap_get(*heap, (Addr)(offset + heap->_this)));
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->_this);        
      }
      break;
    case THAT:
      if (offset + heap->that <= MEM_HEAP_SIZE) {
        spush(stack, heap_get(*heap, (Addr)(offset + heap->that)));
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->that);        
      }
      break;
    case PTR:
      // `pointer` isn't  really a segment but is instead
      // used to the the addresses of the `this` and `that`
      // segments.
      if (offset == 0) {
        assert(heap->_this <= MEM_HEAP_SIZE);
        spush(stack, (Word) heap->_this);
      } else if (offset == 1) {
        assert(heap->that <= MEM_HEAP_SIZE);
        spush(stack, (Word) heap->that);
      } else {
        POINTER_SEGMENT_ERROR(offset, inst.pos);
      }
      return;
    case TMP:
      if (offset < MEM_TEMP_SIZE) {
        spush(stack, mem->tmp[offset]);
      } else {
        SEG_OVERFLOW_ERROR(&inst, MEM_TEMP_SIZE)
      }
      break;
  }
}

// Extended word to allow buffering
// and checking if overflows occured
// on itermediate results.
typedef uint32_t Wordbuf;

static inline void exec_add(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);
  Wordbuf sum = (Wordbuf) x + (Wordbuf) y;

  if (sum <= BIT16_LIMIT) {
    spush(stack, (Word) sum);
  } else {
    // Since `spop` doesn't delete anything,
    // this resets the stack to the state
    // before attempting the add.
    stack->sp += 2;
    ADD_OVERFLOW_ERROR(x, y, sum, pos);
  }
}

static inline void exec_sub(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  if (x >= y) {
    spush(stack, x - y);
  } else {
    stack->sp += 2;  // Restore `x` and `y`.
    SUB_UNDERFLOW_ERROR(x, y, pos);
  }
}

static inline void exec_neg(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  // Two's complement negation.
  y = ~y;
  y += 1;
  spush(stack, y);
}

static inline void exec_and(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  spush(stack, x & y);
}

static inline void exec_or(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  spush(stack, x | y);
}

static inline void exec_not(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  spush(stack, ~y);
}

/* NOTE: Boolean operations return 0xFFFF (-1)
 * if the result is true. Otherwise they return
 * 0x0000 (false). Any value which is not 0x0000
 * is interpreted as being true.
 */

# define TRUE 0xFFFF
# define FALSE 0

static inline void exec_eq(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  spush(stack, x == y ? TRUE : FALSE);
}

static inline void exec_lt(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  spush(stack, x < y ? TRUE : FALSE);
}

static inline void exec_gt(Stack* stack, Pos pos) {
  assert(stack != NULL);

  Word y;
  if (!spop(stack, &y))
    STACK_UNDERFLOW_ERROR(pos);
  Word x;
  if (!spop(stack, &x))
    STACK_UNDERFLOW_ERROR(pos);

  spush(stack, x > y ? TRUE : FALSE);
}

/* Get the currently active file */
#define active_file(prog) (prog->files[prog->fi])

/* Get current instruction */
#define active_inst(prog) (prog->files[prog->fi].insts->cell[prog->files[prog->fi].ei])

#define JMP_OK 1
#define JMP_ERR 0

static int jump_to(struct Program* prog, struct SymKey key, struct SymVal* val) {
  assert(prog != NULL);
  assert(val != NULL);

  GetResult get_res = get_st(active_file(prog).st, &key, val);

  if (get_res == GTRES_OK) {
    /* Change instruction in active file. */
    active_file(prog).ei = val->inst_addr - 1;
    return JMP_OK;
  } else {
    unsigned int prev_fi = prog->fi;
    GetResult get_res;

    for (unsigned int next_fi = 0; next_fi < prog->nfiles; next_fi++) {
      /* Don't re-check the active file. */
      if (prev_fi != next_fi) {
        get_res = get_st(prog->files[next_fi].st, &key, val);
        if (get_res == GTRES_OK) {
          prog->fi = next_fi;  /* Change the file we are in. */
          active_file(prog).ei = val->inst_addr - 1;
          break;
        }
      }
    }

    return get_res == GTRES_ERR
      ? JMP_ERR
      : JMP_OK;
  }
}

static inline void exec_goto(struct Program* prog, Pos pos) {
  assert(prog != NULL);

  struct SymVal val;
  struct SymKey key = mk_key(
    active_file(prog).insts->cell[active_file(prog).ei].ident,
    SBT_LABEL
  );
  if (!jump_to(prog, key, &val))
    CTRL_FLOW_ERROR(key.ident, pos);
  /* Else everything went well. */
}

static inline void exec_if_goto(struct Program* prog, Pos pos) {
  assert(prog != NULL);

  Word val;
  if (!spop(&prog->stack, &val))
    STACK_UNDERFLOW_ERROR(pos);

  /* Jump if topmost value is true. */

  if (val != FALSE) {
    struct SymVal val;
    struct SymKey key = mk_key(
      active_file(prog).insts->cell[active_file(prog).ei].ident,
      SBT_LABEL
    );

    if (!jump_to(prog, key, &val)) {
      /* Restore poping `val` off the stack. */
      prog->stack.sp ++;
      CTRL_FLOW_ERROR(key.ident, pos);
    }
  }
}

static inline void exec_call(struct Program* prog, Pos pos) {
  assert(prog != NULL);

  const char* ident = active_file(prog).insts->cell[active_file(prog).ei].ident;
  Word nargs = active_file(prog).insts->cell[active_file(prog).ei].nargs;
  Stack* stack = &prog->stack;
  Heap* heap = &prog->heap;

  Addr ret_ei = active_file(prog).ei;
  Addr ret_fi = prog->fi;

  if (nargs > stack->sp)
    NARGS_ERROR(nargs, stack->sp, pos);

  struct SymVal val;
  struct SymKey key = mk_key(ident, SBT_FUNC);
  if (!jump_to(prog, key, &val))
    CTRL_FLOW_ERROR(ident, pos);

  // Push return execution index on the stack.
  spush(stack, (Word) ret_ei);
  // Push the return file index on the stack.
  spush(stack, (Word) ret_fi);
  // Push caller's `LCL` on stack.
  spush(stack, (Word) stack->lcl);
  spush(stack, (Word) stack->lcl_len);
  // Push caller's `ARG` on stack.
  spush(stack, (Word) stack->arg);
  spush(stack, (Word) stack->arg_len);
  // Push caller's `THIS` on stack.
  spush(stack, (Word) heap->_this);
  // Push caller's `THAT` on stack.
  spush(stack, (Word) heap->that);

  /* Set `ARG` for new function.
   * `8` accounts for the return address etc. on stack.
   * `nargs` is the number of arguments assumed are
   * on stack right now (according to the `call` invocation).
   * `nargs` is checked at the beginning of this function to
   * avoid underflows here.
   */
  stack->arg = stack->sp - 8 - nargs;
  stack->arg_len = nargs;

  // Set `LCL` for new function and allocate locals.
  stack->lcl = stack->sp;
  stack->lcl_len = val.nlocals;
  for (size_t i = 0; i < stack->lcl_len; i++)
    spush(stack, 0);

  // Now we're ready to jump to the start of the function.
  active_file(prog).ei = val.inst_addr - 1;
}

void exec_ret(struct Program* prog, Pos pos) {
  assert(prog != NULL);

  Stack* stack = &prog->stack;
  Heap* heap = &prog->heap;

  // `LCL` always points to the stack position
  // right after all of the caller's segments
  // etc. have been pushed.
  Addr frame = stack->lcl;
  // Execution index and file index were pushed
  // first in this sequence.
  Addr ret_ei = stack->ops[frame - 8];
  Addr ret_fi = stack->ops[frame - 7];

  // `ARG` always points to the first argument
  // pushed on the stack by the caller. This is
  // where the caller will expect the return value.
  if (!spop(stack, &stack->ops[stack->arg]))
    STACK_UNDERFLOW_ERROR(pos);

  stack->sp = stack->arg + 1;
  // Restore the rest of the registers which
  // have been pushed on stack.
  heap ->that    = stack->ops[frame - 1];
  heap ->_this   = stack->ops[frame - 2];
  stack->arg_len = stack->ops[frame - 3];
  stack->arg     = stack->ops[frame - 4];
  stack->lcl_len = stack->ops[frame - 5];
  stack->lcl     = stack->ops[frame - 6];

  /* Jump ! */

  prog->fi = ret_fi;
  // Don't subtract here (see `exec_call` and `exec_goto`)!
  prog->files[prog->fi].ei = ret_ei;
}

int exec(struct Program* prog) {
  assert(prog != NULL);

  int arrive = setjmp(exec_env);
  if (arrive == EXEC_ERR)
    return EXEC_ERR;

  /* Reaching the end of any file is enough to end execution.
   * `insts->idx` points to the next unused instruction field
   * in the instruction buffer from parsing. Thus it can be
   * used here as the number of instructions in the buffer. */

  for (; active_file(prog).ei < active_file(prog).insts->idx; active_file(prog).ei ++) {
    switch(active_inst(prog).code) {
      case POP:
        exec_pop(
          active_inst(prog),
          &prog->stack,
          &prog->heap,
          &active_file(prog).mem
        );
        break;
      case PUSH:
        exec_push(
          active_inst(prog),
          &prog->stack,
          &prog->heap,
          &active_file(prog).mem
        );
        break;
      case ADD:
        exec_add(&prog->stack, active_inst(prog).pos);
        break;
      case SUB:
        exec_sub(&prog->stack, active_inst(prog).pos);
        break;
      case NEG:
        exec_neg(&prog->stack, active_inst(prog).pos);
        break;
      case AND:
        exec_and(&prog->stack, active_inst(prog).pos);
        break;
      case OR:
        exec_or(&prog->stack, active_inst(prog).pos);
        break;
      case NOT:
        exec_not(&prog->stack, active_inst(prog).pos);
        break;
      case EQ:
        exec_eq(&prog->stack, active_inst(prog).pos);
        break;
      case LT:
        exec_lt(&prog->stack, active_inst(prog).pos);
        break;
      case GT:
        exec_gt(&prog->stack, active_inst(prog).pos);
        break;
      case GOTO:
        exec_goto(prog, active_inst(prog).pos);
        break;
      case IF_GOTO:
        exec_if_goto(prog, active_inst(prog).pos);
        break;
      case CALL:
        exec_call(prog, active_inst(prog).pos);
        break;
      case RET:
        exec_ret(prog, active_inst(prog).pos);
        break;
      default: {
        INST_STR(str, &active_inst(prog));
        errf("invalid inststruction `%s`; programmer mistake", str);
        return EXEC_ERR;
      }
    }
  }

  return 0;
}
