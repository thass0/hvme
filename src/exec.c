#include "exec.h"
#include "utils.h"
#include "parse.h"

#include <stdlib.h>
#include <assert.h>
#include <setjmp.h>

#define MEM_SIZE 0xFFFF

// Extended word to allow buffering
// and checking if overflows occured
// on itermediate results.
typedef uint32_t Wordbuf;

// `temp` and `static` segments
# define STAT_SIZE 0xFFul
static Word static_seg[STAT_SIZE];

# define TMP_SIZE 0xFul
static  Word temp_seg[TMP_SIZE];

// Execution error return location.
static jmp_buf exec_env;

// Errors (some of them are used more than once so
// they are defined here to avoid different spelling
// of the same error or something similar).

#define MISSING_ST_ERROR(inst) { \
    INST_STR(str, inst); \
    errf("missing symbol table: can't execute control flow instruction `%s`", str); \
    longjmp(exec_env, EXEC_ERR); \
}
#define STACK_UNDERFLOW_ERROR err("stack underflow"); longjmp(exec_env, EXEC_ERR)
#define POINTER_SEGMENT_ERROR(addr) \
  errf("can't access pointer segment at `%lu` (max. index is 1).", (addr)); \
  longjmp(exec_env, EXEC_ERR)
#define ADDR_OVERFLOW_ERROR(inst, addr) { \
  INST_STR(inst_str_buf, inst); \
  errf("address overflow: `%s` tries to access RAM at %lu.", inst_str_buf, (addr)); \
  longjmp(exec_env, EXEC_ERR); \
}
# define STACK_ADDR_OVERFLOW_ERROR(inst, addr, max_addr) { \
  INST_STR(inst_str_buf, inst); \
  errf("stack address overflow: `%s` tries to access stack at %lu (limit is at %lu)", \
    inst_str_buf, (addr), (max_addr)); \
  longjmp(exec_env, EXEC_ERR); \
}
# define SEG_OVERFLOW_ERROR(inst, offset) { \
  INST_STR(inst_str_buf, inst); \
  errf("address overflow in `%s`: segment has %lu entries", \
    inst_str_buf, offset); \
  longjmp(exec_env, EXEC_ERR); \
}
#define ADD_OVERFLOW_ERROR(x, y, sum) \
  errf("addition overflow: %d + %d = %d > 65535", x, y, sum); \
  longjmp(exec_env, EXEC_ERR);
#define SUB_UNDERFLOW_ERROR(x, y) {\
  int diff = (int) x - (int) y; \
  errf("subtraction underflow: %d - %d = %d < 0", x, y, diff); \
  longjmp(exec_env, EXEC_ERR); \
}
#define CTRL_FLOW_ERROR(ident) { \
  errf("can't jump to %s", ident); \
  longjmp(exec_env, EXEC_ERR); \
}
# define NARGS_ERROR(nargs, sp) { \
  errf("given number of stack arguments (%d) is wrong." \
    " There are only %lu elements on the stack!", \
    (nargs), (sp)); \
  longjmp(exec_env, EXEC_ERR); \
}

Stack new_stack(void) {
  Stack s = {
    .sp=0,
    .len=STACK_BLOCK_SIZE,
    .arg=0,
    .arg_len=0,
    .lcl=0,
    .lcl_len=0,
  };
  s.ops = (Word*) calloc (s.len, sizeof(Word));
  assert(s.ops != NULL);
  return s;
}

void del_stack(Stack s) {
  free(s.ops);
}

static inline void spush(Stack* stack, Word val) {
  assert(stack != NULL);
  
  if (stack->sp == stack->len) {
    stack->len += STACK_BLOCK_SIZE;
    stack->ops = (Word*) realloc (stack->ops, stack->len * sizeof(Word));
    assert(stack->ops != NULL);
  }

  stack->ops[stack->sp] = val;
  stack->sp ++;
}

static inline Word spop(Stack* stack) {
  assert(stack != NULL);

  if (stack->sp > 0) {
    stack->sp --;
    if (stack->sp < stack->len - STACK_BLOCK_SIZE) {
      stack->len -= STACK_BLOCK_SIZE;
      stack->ops = (Word*) realloc (stack->ops, stack->len * sizeof(Word));
      assert(stack->ops != NULL);
    }
    return stack->ops[stack->sp];
  } else {
    STACK_UNDERFLOW_ERROR;
  }
}

Heap new_heap(void) {
  Heap h = {
    ._this = 0,
    .that = 0,
  };
  h.mem = (Word*) calloc (MEM_SIZE, sizeof(Word));
  assert(h.mem != NULL);
  return h;
}

void del_heap(Heap h) {
  free(h.mem);
}

static inline Word heap_get(const Heap h, Addr addr) {
  assert(h.mem != NULL);
  // No need to check out of range errors
  // since `heap.mem` has 0xFFFF entries
  // and `Addr` can't be larger than that.
  // Rare case of C type-safety ...
  return h.mem[addr];
}

static inline void heap_set(Heap h, Addr addr, Word val) {
  assert(h.mem != NULL);
  // See `heap_get` about further range checks.
  h.mem[addr] = val;
}

void exec_pop(Stack* stack, Heap* heap, Inst inst) {
  assert(stack != NULL);
  assert(stack->ops != NULL);
  assert(heap != NULL);
  assert(heap->mem != NULL);

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
        stack->ops[offset + stack->arg] = spop(stack);
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
        stack->ops[offset + stack->lcl] = spop(stack);
      } else {
        if (offset >= stack->lcl_len) {
          SEG_OVERFLOW_ERROR(&inst, stack->lcl_len);
        } else {
          STACK_ADDR_OVERFLOW_ERROR(&inst, offset + stack->arg, stack->sp);
        }
      }
      break;
    case STAT:
      if (offset < STAT_SIZE) {
        static_seg[offset] = spop(stack);
      } else {
        SEG_OVERFLOW_ERROR(&inst, STAT_SIZE);
      }
      break;
    case CONST:
      // `pop`ping to constant deletes the value.
      (void) spop(stack);
      break;
    case THIS:
      if (offset + heap->_this <= MEM_SIZE) {
        // If we land here, then `offset + heap->_this` fits
        // a `uint16_t`.
        heap_set(*heap, (Addr)(offset + heap->_this), spop(stack));
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->_this);
      }
      break;
    case THAT:
      if (offset + heap->that <= MEM_SIZE) {
        heap_set(*heap, (Addr)(offset + heap->that), spop(stack));
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->that);
      }
      break;
    case PTR:
      if (offset == 0) {
        heap->_this = spop(stack);
      } else if (offset == 1) {
        heap->that = spop(stack);
      } else {
        POINTER_SEGMENT_ERROR(offset);
      }
      return;
    case TMP:
      if (offset < TMP_SIZE) {
        temp_seg[offset] = spop(stack);
      } else {
        SEG_OVERFLOW_ERROR(&inst, TMP_SIZE)
      }
      break;
  }
}

void exec_push(Stack* stack, const Heap* heap, Inst inst) {
  assert(stack != NULL);
  assert(stack->ops != NULL);
  assert(heap != NULL);
  assert(heap->mem != NULL);
  
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
      if (offset < STAT_SIZE) {
        spush(stack, static_seg[offset]);
      } else {
        SEG_OVERFLOW_ERROR(&inst, STAT_SIZE);
      }
      break;
    case CONST:
      // The `constant` segment is a pseudo segment
      // used to get the constant value of `offset`.
      spush(stack, (Word) inst.mem.offset);  // `Word` is `uint16_t`.
      return;
    case THIS:
      if (offset + heap->_this <= MEM_SIZE) {
        spush(stack, heap_get(*heap, (Addr)(offset + heap->_this)));
      } else {
        ADDR_OVERFLOW_ERROR(&inst, offset + heap->_this);        
      }
      break;
    case THAT:
      if (offset + heap->that <= MEM_SIZE) {
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
        assert(heap->_this <= MEM_SIZE);
        spush(stack, (Word) heap->_this);
      } else if (offset == 1) {
        assert(heap->that <= MEM_SIZE);
        spush(stack, (Word) heap->that);
      } else {
        POINTER_SEGMENT_ERROR(offset);
      }
      return;
    case TMP:
      if (offset < TMP_SIZE) {
        spush(stack, temp_seg[offset]);
      } else {
        SEG_OVERFLOW_ERROR(&inst, TMP_SIZE)
      }
      break;
  }
}

static inline void exec_add(Stack* stack) {
  assert(stack != NULL);

  Wordbuf y = (Wordbuf) spop(stack);
  Wordbuf x = (Wordbuf) spop(stack);
  Wordbuf sum = x + y;

  if (sum <= MEM_SIZE) {
    spush(stack, (Word) sum);
  } else {
    // Since `spop` doesn't delete anything,
    // this resets the stack to the state
    // before attempting the add.
    stack->sp += 2;
    ADD_OVERFLOW_ERROR(x, y, sum);
  }
}

static inline void exec_sub(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  if (x >= y) {
    spush(stack, x - y);
  } else {
    stack->sp += 2;  // Restore `x` and `y`.
    SUB_UNDERFLOW_ERROR(x, y);
  }
}

static inline void exec_neg(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  // Two's complement negation.
  y = ~y;
  y += 1;
  spush(stack, y);
}

static inline void exec_and(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  spush(stack, x & y);
}

static inline void exec_or(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  spush(stack, x | y);
}

static inline void exec_not(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  spush(stack, ~y);
}

/* NOTE: Boolean operations return 0xFFFF (-1)
 * if the result is true. Otherwise they return
 * 0x0000 (false). Any value which is not 0x0000
 * is interpreted as being true.
 */

# define TRUE 0xFFFF
# define FALSE 0

static inline void exec_eq(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  spush(stack, x == y ? TRUE : FALSE);
}

static inline void exec_lt(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  spush(stack, x < y ? TRUE : FALSE);
}

static inline void exec_gt(Stack* stack) {
  assert(stack != NULL);

  Word y = spop(stack);
  Word x = spop(stack);

  spush(stack, x > y ? TRUE : FALSE);
}

static inline void exec_goto(SymbolTable st, const char* ident, size_t* num_inst) {
  assert(num_inst != NULL);
  assert(ident != NULL);

  struct SymVal val;
  struct SymKey key = mk_key(ident, SBT_LABEL);
  GetResult get_res = get_st(st, &key, &val);

  if (get_res == GTRES_OK) {
    /* `- 1` is required here because the execution loop
     * will increase `num_inst` by one after this function
     * is called. However, `jmp_inst` already points to the
     * *next* instruction and not to the one of the goto itself.
     * This means that we subtract one to make up for adding
     * one at the end of the loop.
     */
    *num_inst = val.inst_addr - 1;
  } else {
    CTRL_FLOW_ERROR(ident);
  }
}

static inline void exec_if_goto(Stack* stack, SymbolTable st, const char* ident, size_t* num_inst) {
  assert(stack != NULL);
  assert(num_inst != NULL);
  assert(ident != NULL);

  Word val = spop(stack);
  // Jump if topmost value is true.
  if (val != FALSE) {
    struct SymVal val;
    struct SymKey key = mk_key(ident, SBT_LABEL);
    GetResult get_res = get_st(st, &key, &val);

    if (get_res == GTRES_OK) {
      // See `exec_goto` for reason for subtraction.
      *num_inst = val.inst_addr - 1;
    } else {
      // Restore poping `val` off the stack.
      stack->sp ++;
      CTRL_FLOW_ERROR(ident);
    }
  }
}

static inline void exec_call(
  Stack* stack,
  Heap* heap,
  SymbolTable st,
  const char* ident,
  uint16_t nargs,
  size_t* num_inst
) {
  assert(stack != NULL);
  assert(heap != NULL);
  assert(ident != NULL);
  assert(num_inst != NULL);

  if (nargs > stack->sp)
    NARGS_ERROR(nargs, stack->sp);

  struct SymVal val;
  struct SymKey key = mk_key(ident, SBT_FUNC);
  GetResult get_res = get_st(st, &key, &val);

  if (get_res == GTRES_ERR)
    CTRL_FLOW_ERROR(ident);

  // Push return address on the stack.
  spush(stack, (Word) *num_inst);
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
   * `7` accounts for the return address etc. on stack.
   * `nargs` is the number of arguments assumed are
   * on stack right now (according to the `call` invocation).
   * `nargs` is checked at the beginning of this function to
   * avoid underflows here.
   */
  stack->arg = stack->sp - 7 - nargs;
  stack->arg_len = nargs;

  // Set `LCL` for new function and allocate locals.
  stack->lcl = stack->sp;
  stack->lcl_len = val.nlocals;
  for (size_t i = 0; i < stack->lcl_len; i++)
    spush(stack, 0);

  // Now we're ready to jump to the start of the function.
  *num_inst = val.inst_addr - 1;
}

void exec_ret(Stack* stack, Heap* heap, size_t* num_inst) {
  assert(stack != NULL);
  assert(heap != NULL);
  assert(num_inst != NULL);

  // `LCL` always points to the stack position
  // right after all of the caller's segments
  // etc. have been pushed.
  Addr frame = stack->lcl;
  // The return address was pushed first.
  Addr ret_addr = stack->ops[frame - 7];
  // `ARG` always points to the first argument
  // pushed on the stack by the caller. This is
  // where the caller will expect the return value.
  stack->ops[stack->arg] = spop(stack);
  stack->sp = stack->arg + 1;
  // Restore the rest of the registers which
  // have been pushed on stack.
  heap ->that    = stack->ops[frame - 1];
  heap ->_this   = stack->ops[frame - 2];
  stack->arg_len = stack->ops[frame - 3];
  stack->arg     = stack->ops[frame - 4];
  stack->lcl_len = stack->ops[frame - 5];
  stack->lcl     = stack->ops[frame - 6];

  // Don't subtract here (see `exec_call` and `exec_goto`)!
  *num_inst = ret_addr;
}

/*
 * TODO: Handle special call-loop REPL case if the
 * function which will be called has the instruction
 * address 0 and the call is only executed in the
 * initial isolated test. In this case it will jump
 * endlessly because 0 is actually a valid address
 * in this setting
 */

int exec(Stack* stack, Heap* heap, SymbolTable* st, const Insts* insts) {
  assert(stack != NULL);
  assert(insts != NULL);

  int arrive = setjmp(exec_env);
  if (arrive == EXEC_ERR)
    return EXEC_ERR;

  for (size_t i = 0; i < insts->idx; i++) {
    switch(insts->cell[i].code) {
      case POP:
        exec_pop(
          stack,
          heap,
          insts->cell[i]
        );
        break;
      case PUSH:
        exec_push(
          stack,
          heap,
          insts->cell[i]
        );
        break;
      case ADD:
        exec_add(stack); break;
      case SUB:
        exec_sub(stack); break;
      case NEG:
        exec_neg(stack); break;
      case AND:
        exec_and(stack); break;
      case OR:
        exec_or(stack); break;
      case NOT:
        exec_not(stack); break;
      case EQ:
        exec_eq(stack); break;
      case LT:
        exec_lt(stack); break;
      case GT:
        exec_gt(stack); break;
      case GOTO:
        if (st != NULL)
          exec_goto(*st, insts->cell[i].ident, &i);
        else
          MISSING_ST_ERROR(&insts->cell[i]);
        break;
      case IF_GOTO:
        if (st != NULL)
          exec_if_goto(stack, *st, insts->cell[i].ident, &i);
        else
          MISSING_ST_ERROR(&insts->cell[i]);
        break;
      case CALL:
        if (st != NULL)
          exec_call(
            stack,
            heap,
            *st,
            insts->cell[i].ident,
            insts->cell[i].nargs,
            &i
          );
        else
          MISSING_ST_ERROR(&insts->cell[i]);
        break;
      case RET:
        exec_ret(stack, heap, &i);
        break;
      default: {
        INST_STR(str, &insts->cell[i]);
        errf("invalid inststruction `%s`; programmer mistake", str);
        return EXEC_ERR;
      }
    }
  }

  return 0;
}
