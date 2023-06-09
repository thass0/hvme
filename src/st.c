#include "st.h"

#include <math.h>
#include <string.h>

SymbolTable new_st(void) {
  SymbolTable st = {
    .len = ST_BLOCK_SIZE + 1,  // Adding `1` is required so that
    .used = 0,                 // `len` is a multiple of two in `delim_hash`.
    .offset = 0,               // Offset must only be set to a non-zero if
                               // `offset` other instructions were put infront
                               // of the instructions this symbol table points to.
  };
  st.cell = (Symbol*) calloc (sizeof(Symbol), st.len);
  assert(st.cell != NULL);
  return st;
}

void del_st(SymbolTable st) {
  free(st.cell);
}

SymKey mk_key(const char* ident, SymKeyType type) {
  SymKey key = { .type = type };
  strncpy(key.ident, ident, MAX_IDENT_LEN);
  key.ident[strlen(ident)] = '\0';
  return key;
}

SymVal mk_lbval(size_t inst_addr) {
  return (SymVal) {
    .inst_addr = inst_addr,
    .nlocals = 0,
  };
}

SymVal mk_fnval(size_t inst_addr, uint16_t nlocals) {
  return (SymVal) {
    .inst_addr = inst_addr,
    .nlocals = nlocals,
  };
}

static inline int keys_are_eq(const SymKey* a, const SymKey* b) {
  if (a == NULL || b == NULL)
    return 0;

  return (strcmp(a->ident, b->ident) == 0) &&
         (a->type == b->type);
}

static inline int vals_are_eq(const SymVal* a, const SymVal* b) {
  if (a == NULL || b == NULL)
    return 0;

  return a->inst_addr == b->inst_addr && a->nlocals == b->nlocals;
}

static inline unsigned long djb2hash_key(const SymKey* key) {
  assert(key != NULL);

  const char* identp = key->ident;

  unsigned long hash = 5381;
  int c;

  while ((c = *identp++))
    hash = ((hash << 5) + hash) + c;  // hash * 33 + c

  // Treat `type` as another character.
  hash = ((hash << 5) + hash) + (int) key->type;

  return hash;
}

// Returns an index in the interval [0;len].
// `len` must be a power of two.
static inline size_t delim_hash(unsigned long hash, size_t len) {
  // 2^64 / 𝜙 ≈ 11400714819323198485 (closest approximation which is not a multiple of 2)
  return (hash * 11400714819323198485llu) >> (64 - ((int)log2(len) - 1));  
}

InsertResult insert_st(
  SymbolTable* st,
  SymKey key,
  SymVal val
) {
  assert(st != NULL);

  if (st->used >= st->len) {
    // Realloc `cell` if the symbol table is full.
    st->len += ST_BLOCK_SIZE;
    st->cell = (Symbol*) realloc (st->cell, st->len * sizeof(Symbol));
    memset(st->cell + st->len - ST_BLOCK_SIZE, 0, ST_BLOCK_SIZE * sizeof(Symbol));
  }

  unsigned long hash = djb2hash_key(&key);
  size_t idx = delim_hash(hash, st->len - 1);

  // Use linear probing to find an unused entry in `cell`.
  size_t sd_len = idx;

  // Check entries [idx;len - 1]
  while (idx < st->len && st->cell[idx].key.type != SBT_UNUSED) {
    // Does a symbol with the same ident and type exist?
    if (keys_are_eq(&st->cell[idx].key, &key)) {
      // Does this symbol have the same content we want to enter?
      if (vals_are_eq(&st->cell[idx].val, &val)) {
        break;  // Yes, do nothing. The content exists.
      } else {
        return INRES_EXISTS;  // No, bad. There is different data for the same key.
      }
    } else {
      idx ++;
    }
  }

  // Check entries [0;idx - 1]
  if (idx >= st->len) {
    idx = 0;
    while (idx < sd_len && st->cell[idx].key.type != SBT_UNUSED) {
      // Does a symbol with the same ident and type exist?
      if (keys_are_eq(&st->cell[idx].key, &key)) {
        // Does this symbol have the same content we want to enter?
        if (vals_are_eq(&st->cell[idx].val, &val)) {
          break;  // Yes, do nothing. The content exists.
        } else {
          return INRES_EXISTS;  // No, bad. There is different data for the same key.
        }
      } else {
        idx ++;
      }
    }
  }

  /* This condition must always be true, because `insert_st`
   * allocated enough memory for the symbol table to have
   * at least a single free entry during the insertion.
   * The above two loops must find this entry, since they
   * check each entry in the cell.
   */
  assert(st->cell[idx].key.type == SBT_UNUSED);

  memcpy(&st->cell[idx].key, &key, sizeof(SymKey));
  memcpy(&st->cell[idx].val, &val, sizeof(SymVal));

  st->used ++;

  return INRES_OK;
}

GetResult get_st(SymbolTable st, const SymKey* key, SymVal* val) {
  assert(key != NULL);
  assert(val != NULL);

  unsigned long hash = djb2hash_key(key);
  size_t idx = delim_hash(hash, st.len - 1);
  size_t sd_len = idx;

  while (idx < st.len && !keys_are_eq(&st.cell[idx].key, key))
    idx ++;

  if (idx == st.len) {
    idx = 0;
    while (idx < sd_len && !keys_are_eq(&st.cell[idx].key, key))
      idx ++;

    if (idx == sd_len)
    // Every index was checked and none contains the given key.
      return GTRES_ERR;
  }

  *val = st.cell[idx].val;
  // Offset the retrieved address.
  val->inst_addr += st.offset;
  return GTRES_OK;
}
