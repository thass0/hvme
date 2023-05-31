#include "parse.h"
#include "utils.h"
#include "st.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

Insts* new_insts(const char* filename) {
  Insts* insts = (Insts*) malloc (sizeof(Insts));
  assert(insts != NULL);
  insts->idx = 0;
  insts->len = INST_BLOCK_SIZE;

  if (filename != NULL) {
    insts->filename = (char*) calloc (strlen(filename) + 1, sizeof(char));
    assert(filename != NULL);
    strcpy(insts->filename, filename);
  } else {
    insts->filename = NULL;
  }

  insts->cell = (Inst*) calloc (insts->len, sizeof(Inst));
  assert(insts->cell != NULL);

  return insts;
}

void del_insts(Insts* insts) {
  if (insts != NULL) {
    free(insts->cell);
    free(insts->filename);
    free(insts);
  }
}

void cpy_insts(Insts* dest, Insts* src) {
  assert(dest != NULL);
  assert(src != NULL);

  dest->len += src->len;
  dest->cell = (Inst*) realloc (dest->cell, dest->len * sizeof(Inst));
  memcpy(dest->cell + dest->idx, src->cell, src->idx * sizeof(Inst));
  dest->idx += src->idx;
}

void inst_str(const Inst* i, char* str) {
  assert(i != NULL);
  assert(str != NULL);

  if (i->code == PUSH || i->code == POP) {
    static char* segs[] = {
      [ARG]="argument", [LOC]="local", [STAT]="static",
      [CONST]="constant", [THIS]="this", [THAT]="that",
      [PTR]="pointer", [TMP]="temp",
    };
    static char* meminsts[] = { [POP]="pop", [PUSH]="push" };
    snprintf(str, INST_STR_BUF,  "%s %s %d",
      meminsts[i->code], segs[i->mem.seg], i->mem.offset);
  } else if (i->code == GOTO || i->code == IF_GOTO) {
    static char* ctrlflow_insts[] = { [TK_GOTO]="goto", [TK_IF_GOTO]="if-goto" };
    snprintf(str, INST_STR_BUF, "%s %s",
      ctrlflow_insts[i->code], i->ident);
  } else if (i->code == CALL) {
    snprintf(str, INST_STR_BUF, "call %s %d",
      i->ident, i->nargs);
  } else {
    static char* insts[] = {
      [0]="IC_NONE",
      [TK_ADD]="add", [TK_SUB]="sub", [TK_NEG]="neg",
      [TK_AND]="and", [TK_OR]="or", [TK_NOT]="not",
      [TK_EQ]="eq", [TK_GT]="gt", [TK_LT]="lt",
      [TK_RET]="return",
    };
    strncpy(str, insts[i->code], INST_STR_BUF);
  }
}

// Internal item stream interface.
typedef struct {
  const Item* items;
  size_t idx;
  size_t len;
} ItemStream;


const static Item none_item = (Item) {
  .t=TK_NONE,
  .uilit=0,
  .pos={.ln=0, .cl=0},
};

// Return the current item and go to the next.
const Item* its_next(ItemStream* its) {
  assert(its != NULL);
  
  return its->idx < its->len
    // Return the current entry and go to the next.
    ? &its->items[its->idx ++]
    // Return the none entry, signaling that `idx`
    // has reached the end of the stream.
    : &none_item;
}

// Return the current item.
const Item* its_lh(const ItemStream* its) {
  assert(its != NULL);

  return its->idx < its->len
    ? &its->items[its->idx]
    : &none_item;
}

// Create a slice into the given stream which starts
// at the left offset of `idx` `ol` and ends at the
// right offset of `idx` `or`.
ItemStream its_slice(ItemStream* its, size_t ol, size_t or) {
  assert(its != NULL);

  return (ItemStream) {
    .items=its->items,
    // `idx` is `idx - offset left` if
    // this doesn't go below zero.
    .idx = ol >= its->idx
      ? 0
      : its->idx - ol,
    // `len` is `idx + offset right` if
    // this doesn't exceed the current `len`.
    .len = or + its->idx <= its->len
      ? or + its->idx
      : its->len,
  };
}

void print_expect3_err(ItemStream its, const char* expectation, const char* filename) {
  assert(expectation != NULL);

  // `calloc` will indirectly add the null-terminator
  // to both `spacer` and `pointer`.
  ITEM_STR(first, its_next(&its));
  char* first_spacer = (char*) calloc (strlen(first) + 1, sizeof(char));
  assert(first_spacer != NULL);
  memset(first_spacer, ' ', strlen(first));

  const Item* second_it = its_next(&its);
  ITEM_STR(second, second_it);
  char* second_spacer = (char*) calloc (strlen(second) + 1, sizeof(char));
  assert(second_spacer != NULL);
  memset(second_spacer, ' ', strlen(second));

  const Item* it = its_next(&its);
  ITEM_STR(self, it);
  char* pointer = (char*) calloc(strlen(self) + 1, sizeof(char));
  assert(pointer != NULL);
  memset(pointer, '^', strlen(self));

  Item display_it = *it;
  if (display_it.t == TK_NONE) {
    // Correct the position in none items.
    display_it.pos.cl = strlen(second) + second_it->pos.cl + 1;
    display_it.pos.ln = second_it->pos.ln;
  }

  if (filename == NULL) {
    errf(
      "wrong token, expected %s\n"
      " %d:%d | %s %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s %s",  // <- Error marker.
      expectation,
      display_it.pos.ln + 1, display_it.pos.cl + 1, first, second, self,
      display_it.pos.ln + 2, display_it.pos.cl + 1, first_spacer, second_spacer, pointer
    );
  } else {
    errf(
      "wrong token, expected %s (%s)\n"
      " %d:%d | %s %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s %s",  // <- Error marker.
      expectation, filename,
      display_it.pos.ln + 1, display_it.pos.cl + 1, first, second, self,
      display_it.pos.ln + 2, display_it.pos.cl + 1, first_spacer, second_spacer, pointer
    );
  }

  free(first_spacer);
  free(second_spacer);
  free(pointer);
}

void print_expect2_err(ItemStream its, const char* expectation, const char* filename) {
  assert(expectation != NULL);
  
  // `calloc` will indirectly add the null-terminator
  // to both `spacer` and `pointer`.
  const Item* prev_it = its_next(&its);
  ITEM_STR(prev, prev_it);
  char* spacer = (char*) calloc (strlen(prev) + 1, sizeof(char));
  assert(spacer != NULL);
  memset(spacer, ' ', strlen(prev));

  const Item* it = its_next(&its);
  ITEM_STR(self, it);
  char* pointer = (char*) calloc (strlen(self) + 1, sizeof(char));
  assert(pointer != NULL);
  memset(pointer, '^', strlen(self));

  Item display_it = *it;
  if (display_it.t == TK_NONE) {
    // Correct the position in none items.
    display_it.pos.cl = prev_it->pos.cl + strlen(prev) + 1;
    display_it.pos.ln = prev_it->pos.ln;
  }

  ITEM_STR(next, its_next(&its));

  if (filename == NULL) {
    errf(
      "wrong token, expected %s\n"
      " %d:%d | %s %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s",  // <- Error marker.
      expectation,
      display_it.pos.ln + 1, display_it.pos.cl + 1, prev, self, next,
      display_it.pos.ln + 2, display_it.pos.cl + 1, spacer, pointer
    );
  } else {
    errf(
      "wrong token, expected %s (%s)\n"
      " %d:%d | %s %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s",  // <- Error marker.
      expectation, filename,
      display_it.pos.ln + 1, display_it.pos.cl + 1, prev, self, next,
      display_it.pos.ln + 2, display_it.pos.cl + 1, spacer, pointer
    );
  }

  free(spacer);
  free(pointer);
}

void print_item_err(const Item* it, const char* filename) {
  ITEM_STR(it_str, it);
  /*
  TODO:
  ```
  (vme) clear
  Error: wrong start of instruction
   1:1 | ident
  ```
  Change the above to look like this:
  ```
  (vme) clear
  Error: wrong start of instruction
   1:1 | clear (identifier)
  ```
  */
  if (filename == NULL) {
    errf(
      "wrong start of instruction\n"
      " %d:%d | %s",
      it->pos.ln + 1, it->pos.cl + 1, it_str
    );
  } else {
    errf(
      "wrong start of instruction (%s)\n"
      " %d:%d | %s",
      filename, it->pos.ln + 1, it->pos.cl + 1, it_str
    );
  }
}

void print_ident_err(ItemStream its, const char* filename) {
  const Item* ctrlflow_it = its_next(&its);
  ITEM_STR(ctrlflow_str, ctrlflow_it);
  char* ctrlflow_spacer = (char*) calloc (strlen(ctrlflow_str) + 1, sizeof(char));
  assert(ctrlflow_spacer != NULL);
  memset(ctrlflow_spacer, ' ', strlen(ctrlflow_str));  // Implicit `* sizeof(char)`.

  const Item* no_ident = its_next(&its);
  ITEM_STR(no_id_str, no_ident);
  char* pointer = (char*) calloc (strlen(no_id_str) + 1, sizeof(char));
  assert(pointer != NULL);
  memset(pointer, '^', strlen(no_id_str));

  Item display_it = *no_ident;
  if (display_it.t == TK_NONE) {
    // Correct the position in none items.
    display_it.pos.cl = strlen(ctrlflow_str) + ctrlflow_it->pos.cl + 1;
    display_it.pos.ln = ctrlflow_it->pos.ln;
  }

  if (filename == NULL) {
    errf(
      "wrong token, expected an identifier\n"
      " %d:%d | %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s",  // <- Error marker.
      display_it.pos.ln + 1, display_it.pos.cl + 1, ctrlflow_str, no_id_str,
      display_it.pos.ln + 2, display_it.pos.cl + 1, ctrlflow_spacer, pointer
    );
  } else {
    errf(
      "wrong token, expected an identifier (%s)\n"
      " %d:%d | %s %s\n"  // <- Scanned instruction.
      " %d:%d | %s %s",  // <- Error marker.
      filename,
      display_it.pos.ln + 1, display_it.pos.cl + 1, ctrlflow_str, no_id_str,
      display_it.pos.ln + 2, display_it.pos.cl + 1, ctrlflow_spacer, pointer
    );
  }

  free(ctrlflow_spacer);
  free(pointer);
}

#define PARSE_OK 0
#define PARSE_ERR -1

static inline int map_inst(ItemStream* its, Inst* inst, const char* filename) {
  (void) filename;

  assert(its != NULL);
  assert(inst != NULL);

  const Item* it = its_next(its);
  inst->pos = it->pos;

  // Any token which points to this function
  // maps 1:1 to its instruction code.
  inst->code = (enum InstCode) it->t;

  return PARSE_OK;
}

static inline int memory_inst(ItemStream* its, Inst* inst, const char* filename) {
  assert(its != NULL);
  assert(inst != NULL);

  const Item* mem_it = its_next(its);
  inst->pos = mem_it->pos;
  // The tokens `TK_PUSH` and `TK_POP` map
  // to their instruction codes, too.
  inst->code = (enum InstCode) mem_it->t;

  // Segment. All segment tokens are in a single range from
  // `TK_ARG` to `TK_TMP`. Therefore this check is sufficient.
  if (TK_ARG <= its_lh(its)->t && its_lh(its)->t <= TK_TMP) {
      // Segment tokens map 1:1 to their codes, too!
      inst->mem.seg = (Segment) its_next(its)->t;
  } else {
    print_expect2_err(its_slice(its, 1, 1), "a segment", filename);
    return PARSE_ERR;
  }

  // Offset.
  if (its_lh(its)->t == TK_UINT) {
    inst->mem.offset = its_next(its)->uilit;
  } else {
    print_expect3_err(its_slice(its, 2, 1), "an offset", filename);
    return PARSE_ERR;
  }
  return PARSE_OK;
}

static inline int goto_inst(ItemStream* its, Inst* inst, const char* filename) {
  assert(its != NULL);
  assert(inst != NULL);

  const Item* goto_it = its_next(its);  // Consume `goto`.
  inst->pos = goto_it->pos;
  inst->code = (enum InstCode) goto_it->t;

  if (its_lh(its)->t == TK_IDENT) {
    strcpy(inst->ident, its_next(its)->ident);
  } else {
    print_ident_err(its_slice(its, 1, 1), filename);
    return PARSE_ERR;
  }

  return PARSE_OK;
}

static inline int if_goto_inst(ItemStream* its, Inst* inst, const char* filename) {
  assert(its != NULL);
  assert(inst != NULL);

  const Item* if_goto_it = its_next(its);  // Consume `if-goto`.
  inst->pos = if_goto_it->pos;
  inst->code = (enum InstCode) if_goto_it->t;

  if (its_lh(its)->t == TK_IDENT) {
    strcpy(inst->ident, its_next(its)->ident);
  } else {
    print_ident_err(its_slice(its, 1, 1), filename);
    return PARSE_ERR;
  }

  return PARSE_OK;
}

static inline int label_meta(ItemStream* its, SymbolTable* st, size_t num_inst, const char* filename) {
  assert(its != NULL);
  assert(st != NULL);
  
  // This function is only called if the return value
  // of this call is `TK_LABEL`.
  its_next(its);

  if (its_lh(its)->t == TK_IDENT) {
    const char* ident = its_next(its)->ident;
    struct SymKey key = mk_key(ident, SBT_LABEL);
    struct SymVal val = mk_lbval(num_inst);
    if (st != NULL)
      insert_st(st, key, val);
    else
      warn_no_st(&key, &val);
  } else {
    print_ident_err(its_slice(its, 1, 1), filename);
    return PARSE_ERR;
  }

  return PARSE_OK;
}

static inline int function_inst(
  ItemStream* its,
  SymbolTable* st,
  size_t num_inst,
  const char* filename
) {
  assert(its != NULL);

  its_next(its);  // Consume `TK_FUNC`

  const char* ident = NULL;

  if (its_lh(its)->t == TK_IDENT) {
    ident = its_next(its)->ident;
  } else {
    print_ident_err(its_slice(its, 1, 1), filename);
    return PARSE_ERR;
  }

  uint16_t nlocals = 0;

  if (its_lh (its)->t == TK_UINT) {
    nlocals = its_next(its)->uilit;
  } else {
    print_expect3_err(
      its_slice(its, 2, 1),
      "the number of locals",
      filename
    );
  }

  struct SymKey key = mk_key(ident, SBT_FUNC);
  struct SymVal val = mk_fnval(num_inst, nlocals);

  if (st != NULL)
    insert_st(st, key, val);
  else
    warn_no_st(&key, &val);

  return PARSE_OK;
}

static inline int call_inst(ItemStream* its, Inst* inst, const char* filename) {
  assert(its != NULL);
  assert(inst != NULL);

  const Item* call_it = its_next(its);
  inst->pos = call_it->pos;
  inst->code = (enum InstCode) call_it->t;

  if (its_lh(its)->t == TK_IDENT) {
    strcpy(inst->ident, its_next(its)->ident);
  } else {
    print_ident_err(its_slice(its, 1, 1), filename);
    return PARSE_ERR;
  }

  if (its_lh(its)->t == TK_UINT) {
    inst->nargs = its_next(its)->uilit;
  } else {
    print_expect3_err(its_slice(its, 2, 1), "the number of arguments", filename);
  }
  
  return PARSE_OK;
}

typedef int(*ParseFnPtr)(ItemStream*, Inst*, const char*);
static ParseFnPtr parse_fns[] = {
  [TK_ADD]=map_inst,
  [TK_SUB]=map_inst,
  [TK_NEG]=map_inst,
  [TK_AND]=map_inst,
  [TK_OR]=map_inst,
  [TK_NOT]=map_inst,
  [TK_EQ]=map_inst,
  [TK_GT]=map_inst,
  [TK_LT]=map_inst,
  [TK_CALL]=call_inst,
  [TK_RET]=map_inst,
  [TK_GOTO]=goto_inst,
  [TK_IF_GOTO]=if_goto_inst,
  [TK_POP]=memory_inst,
  [TK_PUSH]=memory_inst,
};

static inline int is_inst_token(const Token t) {
  return 
    (TK_PUSH <= t && t <= TK_POP) ||  // <- range of all memory instructions
    (TK_ADD <= t && t <= TK_NOT)  ||  // <- range of all arithmetic and logic instructions
    (TK_LABEL <= t && t <= TK_IF_GOTO) ||  // <- range of all control flow instructions
    (TK_FUNC <= t && t <= TK_RET);  // <- range of all function calling instructions
}

Insts* parse(const Items* items, SymbolTable* st) {
  assert(items != NULL);
  
  ItemStream its = (ItemStream) {
    .items=items->cell,
    .idx=0,
    .len=items->idx,
  };

  Insts* insts = new_insts(items->filename);
  int res;
  /* Each time this parse function is called, a new `Insts`
   * instance is created with its own local instruction count
   * which starts at 0 every time `parse` runs. To keep track
   * of the instruction count between `parse` calls, the global
   * symbol table has a field which stores the number of the next
   * instruction between calls (`num_inst`). It's used as an offset
   * when calling `label` and it's updated at the end of each `parse` call.
   */
  size_t st_num_inst = st == NULL ? 0 : st->num_inst;
  
  for (
    const Item* it = its_lh(&its);
    its.idx < its.len;
    it = its_lh(&its)
  ) {
    // Error if the token can't be the beginning
    // of an instruction.
    if (!is_inst_token(it->t)) {
      del_insts(insts);
      print_item_err(it, items->filename);
      return NULL;
    }

    // Allocate more memory if current limit was reached.
    if (insts->idx == insts->len) {
      insts->len += INST_BLOCK_SIZE;
      insts->cell = (Inst*) realloc (insts->cell, insts->len * sizeof(Inst));
      assert(insts->cell != NULL);
    }

    switch (it->t) {
      case TK_LABEL:
        res = label_meta(
          &its,
          st,
          insts->idx + st_num_inst,
          items->filename
        );
        break;
      case TK_FUNC:
        res = function_inst(
          &its,
          st,
          insts->idx + st_num_inst,
          items->filename
        );
        break;
      default:
        // Construct the current instruction from
        // the item stream. A parse function might
        // advance by any number but has to stop
        // if it reaches the end of the input.
        res =
          parse_fns[it->t](&its, &insts->cell[insts->idx], items->filename);
        insts->idx ++;
        break;
    }

    if (res == PARSE_ERR) {
      del_insts(insts);
      return NULL;
    }
  }

  if (st != NULL)
    st->num_inst +=  insts->idx;

  return insts;
}
