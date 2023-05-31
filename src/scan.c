#include "scan.h"
#include "utils.h"

#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

// Used by `T_KUINT` to store the scanned number.
static Uint uilit = 0;
// Used by `TK_IDENT` to store the scanned identifier.
// The maximum identifier length is 24.
static char ident_buf[MAX_IDENT_LEN + 1];

#define TOKEN_COMPLETED -1

Items* new_items(const char* filename) {
  Items* items = (Items*) malloc (sizeof(Items));
  assert(items != NULL);
  items->cur.ln = 0, items->cur.cl = 0, items->idx = 0;
  items->len = ITEM_BLOCK_SIZE;

  if (filename != NULL) {
    items->filename = (char*) calloc (strlen(filename) + 1, sizeof(char));
    assert(items->filename != NULL);
    strcpy(items->filename, filename);
  } else {
    items->filename = NULL;
  }

  items->cell = (Item*) calloc (items->len, sizeof(Item));
  assert(items->cell != NULL);

  return items;
}

void del_items(Items* items) {
  // NULL-check to avoid segfaults when
  // trying to free the members.
  if (items != NULL) {
    free(items->cell);
    free(items->filename);
    free(items);
  }
}

static inline void inc(size_t *restrict offset, Pos *restrict pos) {
  *offset += 1;
  pos->cl += 1;
}

static inline void incby(
  size_t inc,
  size_t *restrict offset,
  Pos *restrict pos
) {
  *offset += inc;
  pos->cl += inc;
}

static inline void incl(size_t *restrict offset, Pos *restrict pos) {
  *offset += 1;
  pos->ln += 1;
  pos->cl = 0;
}

void item_str(const Item* it, char* str) {
  assert(it != NULL);
  assert(str != NULL);
  assert(TK_NONE <= it->t && it->t <= TK_IDENT);

  static char* strs[] = {
    "???",
    "push",
    "pop",
    "argument",
    "local",
    "static",
    "constant",
    "this",
    "that",
    "pointer",
    "temp",
    "num",
    "label",
    "goto",
    "if-goto",
    "function",
    "call",
    "return",
    "add",
    "sub",
    "neg",
    "eq",
    "gt",
    "lt",
    "and",
    "or",
    "not",
    "ident"
  };

  if (it->t == TK_UINT) {
    // `uilit` is a 16-bit unsigned integer which
    // means it will never be more that 5 digits long.
    snprintf(str, ITEM_STR_BUF, "%d", it->uilit);
  } else {
    snprintf(str, ITEM_STR_BUF, "%s", strs[it->t]);
  }
}

void scan_err(const char* blk, const char* filename, Pos pos) {
  if (filename == NULL) {
    errf(
      "couldn't scan input\n"
      " %d:%d | %s",
      pos.ln + 1, pos.cl + 1, blk
    );
  } else {
    errf(
      "couldn't scan input (%s)\n"
      " %d:%d | %s",
      filename, pos.ln + 1, pos.cl + 1, blk
    );
  }
}


static inline int static_scan(const char* search, const char* blk, size_t len, size_t* offset, Pos* pos) {
  assert(search != NULL);
  assert(blk != NULL);
  assert(offset != NULL);

  size_t slen = strlen(search);
  if ((*offset + slen) > len) {
    // The rest of the block isn't long  enough.
    return 0;
  }

  if (strncmp(blk + *offset, search, slen) == 0) {
    incby(slen, offset, pos);
    return TOKEN_COMPLETED;
  } else {
    // TODO: Temporaty until finding the exact character is implemented.
    return 0;  
  }
}

static inline int push(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("push", blk, len, offset, pos);
}

static inline int pop(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("pop", blk, len, offset, pos);
}

static inline int argument(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("argument", blk, len, offset, pos);
}

static inline int local(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("local", blk, len, offset, pos);
}

static inline int _static(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("static", blk, len, offset, pos);
}

static inline int constant(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("constant", blk, len, offset, pos);
}

static inline int this(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("this", blk, len, offset, pos);
}

static inline int that(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("that", blk, len, offset, pos);
}

static inline int pointer(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("pointer", blk, len, offset, pos);
}

static inline int temp(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("temp", blk, len, offset, pos);
}

static inline int _uint(const char* blk, size_t len, size_t* offset, Pos* pos) {
  assert(blk != NULL);
  assert(offset != NULL);
  
  size_t digit_offset = *offset;
  size_t ndigits = 0;
  uint16_t nums[5] = { 0, 0, 0, 0, 0 };
  while (
    digit_offset < len &&
    ndigits < 5 &&
    isdigit(blk[digit_offset])
  ) {
    nums[ndigits] = blk[digit_offset] - '0';
    digit_offset++;
    ndigits++;
  }

  // The numbers doesn't just have to contian enought digits
  // but it also has to end  with another space character.
  // This conditional rejects something like `87935234` too,
  // because it's too long.
  // If `digit_offset >= len` is true this means that there
  // are no more characters left to check. This obviously
  // means that there is no trailing whitespace either.
  if (digit_offset >= len || !isspace(blk[digit_offset]))
    return ndigits;

  int uilit_buf = 0;

  switch (ndigits) {
    case 0:
      return 0;
    case 1:
      uilit_buf =
        nums[0];
      break;
    case 2:
      uilit_buf =
        nums[1] + nums[0] * 10;
      break;
    case 3:
      uilit_buf =
        nums[2] + nums[1] * 10 + nums[0] * 100;
      break;
    case 4:
      uilit_buf =
        nums[3] + nums[2] * 10 + nums[1] * 100 + nums[0] * 1000;
      break;
    case 5:
      uilit_buf =
        nums[4] + nums[3] * 10 + nums[2] * 100 + nums[1] * 1000 + nums[0] * 10000;
      break;
  }

  // Using five decimal digits, we could represent numbers
  // larger than the 16-bit limit (65535) up to 99999.
  // Therefore, we buffer to an `int` which will not overflow
  // from any number between 65535 and  99999 and then limit
  // the number we store at 65536. This means that any input
  // above 65535 is simply saturated to 65535. A warning is
  // emitted.
  if (uilit_buf > 65535) {
    warn_sat_uilit(uilit_buf);
    uilit = 65535;
  } else {
    // `uilit` fits the scanned number.
    uilit = uilit_buf;
  }

  incby(ndigits, offset, pos);

  return TOKEN_COMPLETED;
}

static inline int label(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("label", blk, len, offset, pos);
}

static inline int _goto(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("goto", blk, len, offset, pos);
}

static inline int if_goto(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("if-goto", blk, len, offset, pos);
}

static inline int function(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("function", blk, len, offset, pos);
}

static inline int call(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("call", blk, len, offset, pos);
}

static inline int _return(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("return", blk, len, offset, pos);
}

static inline int add(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("add", blk, len, offset, pos);
}

static inline int sub(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("sub", blk, len, offset, pos);
}

static inline int neg(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("neg", blk, len, offset, pos);
}

static inline int eq(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("eq", blk, len, offset, pos);
}

static inline int gt(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("gt", blk, len, offset, pos);
}

static inline int lt(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("lt", blk, len, offset, pos);
}

static inline int and(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("and", blk, len, offset, pos);
}

static inline int or(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("or", blk, len, offset, pos);
}

static inline int not(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("not", blk, len, offset, pos);
}

static inline int ident(const char* blk, size_t len, size_t* offset, Pos* pos) {
  assert(blk != NULL);
  assert(offset != NULL);

  size_t ident_offset = *offset;
  size_t nchars = 0;

  // [A-Za-z_\.:]
  if (ident_offset < len &&
    (isalpha(blk[ident_offset]) ||
    blk[ident_offset] == '_' ||
    blk[ident_offset] == '.' ||
    blk[ident_offset] == ':')
  ) {
    nchars ++;
    ident_offset ++;
  } else {
    return 0;
  }

  // [0-9A-Za-z_\.:]*
  char next;
  while(ident_offset < len) {
    next = blk[ident_offset];
    if (isalnum(next) || next == '_' || next == '.' || next == ':') {
      // Go to the next character.
      nchars ++;
      ident_offset ++;
    } else {
      // We ended up at a character which isn't
      // part of the identifier anymore.
      break;
    }
  }

  // `ident_offset >= len` is the unhappy path
  // of the while loop above: it's true if the
  // loop ends because the block's length is
  // exceeded before the ident ends.
  // The character after the end of the ident
  // must be a whitespace.
  if (ident_offset >= len || !isspace(blk[ident_offset]))
    return nchars;

  if (nchars > MAX_IDENT_LEN)
    warn_trunc_ident(blk + *offset, nchars, MAX_IDENT_LEN);

  strncpy(ident_buf, blk + *offset, MAX_IDENT_LEN);
  ident_buf[nchars >= MAX_IDENT_LEN ? MAX_IDENT_LEN : nchars] = '\0';

  incby(nchars, offset, pos);

  return TOKEN_COMPLETED;
}

static inline int comment(const char* blk, size_t len, size_t* offset, Pos* pos) {
  return static_scan("//", blk, len, offset, pos);
}

typedef int(*ScanFnPtr)(const char*, size_t, size_t*, Pos*);
static ScanFnPtr match_fns[] = {
  push,
  pop,
  argument,
  local,
  _static,
  constant,
  this,
  that,
  pointer,
  temp,
  _uint,
  label,
  _goto,
  if_goto,
  function,
  call,
  _return,
  add,
  sub,
  neg,
  eq,
  gt,
  lt,
  and,
  or,
  not,
  ident,
  comment,
  NULL,
};

Item item_from_fn(size_t fn_idx, Pos pos) {
  // IMPORTANT: It has to be ensured, that
  // `Token(fn_idx) = fn_idx + 1` remains true.
  Item item = {
    .t=fn_idx + 1,
    .uilit=uilit,
    .pos=pos,
  };
  strcpy(item.ident, ident_buf);
  memset(ident_buf, ' ', MAX_IDENT_LEN);
  uilit = 0;
  return item;
}

static inline int eat_ws(const char* blk, size_t len, size_t* offset, Pos* pos) {
  assert(blk != NULL);
  assert(offset != NULL);
  
  int found_nl = 0;

  while (*offset < len && isspace(blk[*offset])) {
    if (blk[*offset] == '\n') {
      found_nl = 1;
      incl(offset, pos);
    } else {
      inc(offset, pos);
    }
  }

  return found_nl;
}

static inline ssize_t num_trailing(const char* blk, size_t len) {
  assert(blk != NULL);
  
  size_t offset = 0;

  while (offset < len) {
    if (isspace(blk[offset])) {
      // Error: there is whitespace left in this block
      // which means that the scanner failed to scan
      // all fully available tokens.
      return SCAN_ERR;
    }

    offset++;
  }

  // Number of trailing non-whitespace characters to copy.
  return offset;
}

ssize_t scan(Items* items, const char* blk, size_t len) {
  assert(items != NULL);
  assert(blk != NULL);
  
  size_t offset = 0;
  // Remember whether we left inside a comment.
  static int inside_comment = 0;

  // Eat up initial whitespace.
  eat_ws(blk, len, &offset, &items->cur);

  while (offset < len) {
    // `offset` now points to the first
    // character in a new line of content.

    // Eat comments.
    if (inside_comment) {
      if (blk[offset] == '\n') {
        inside_comment = 0;
        incl(&offset, &items->cur);
        // Now that we've set `inside_comment` false we
        // are going to begin a new iteration where we
        // scan actual content. This requires that initial
        // whitespace is scanned first.
        eat_ws(blk, len, &offset, &items->cur);
      } else {
        inc(&offset, &items->cur);
      }

      continue;
    }

    int num_matched = 0;
    size_t fn_idx = 0;
    Pos cur_start = items->cur;
    while (
      num_matched != TOKEN_COMPLETED
      && match_fns[fn_idx] != NULL
    ) {
      num_matched = match_fns[fn_idx](blk, len, &offset, &items->cur);
      if (num_matched != TOKEN_COMPLETED)
        fn_idx++;
    }

    if (num_matched == TOKEN_COMPLETED && match_fns[fn_idx] == comment) {
      // Comment scan function as completed successfully.
      // This means that we set `inside_comment` to true and
      // continue until the comment is terminated by a newline.
      inside_comment = 1;
    } else if (num_matched == TOKEN_COMPLETED && match_fns[fn_idx] != NULL) {
      // The scan function `fn_idx`
      // completed successfully.
      if (items->idx >= items->len) {
        // Increase size and reallocate in case the array is full.
        items->len += ITEM_BLOCK_SIZE;
        items->cell = realloc(items->cell, items->len * sizeof(Item));
        assert(items->cell != NULL);
      }

      items->cell[items->idx] = item_from_fn(fn_idx, cur_start);
      items->idx ++;
    } else {
      // No scan function completed. This must
      // raise and error only if there are
      // whitespace characters or the EOF after
      // the current set of characters. Otherwise
      // if the rest of the current block only
      // contains trailing characters blk no whitespace
      // then those characters could still be part of
      // another match and should thus be copied to the
      // start of the next block.
      ssize_t ret = num_trailing(blk + offset, len - offset);

      if (ret == SCAN_ERR) {
        // Remove the trailing newline character
        // from the string to print it.
        char* pblk = (char*) calloc (len - offset, sizeof(char));
        assert(pblk != NULL);
        strncpy(pblk, blk + offset, len - offset - 1);
        // pblk[len - 1] is NULL already because calloc
        // was used to allocate it. No need to add it.

        scan_err(pblk, items->filename, cur_start);
        free(pblk);
      }

      return ret;
    }

    // Eat up trailing whitespace.
    eat_ws(blk, len, &offset, &items->cur);
  }

  // All characters in `blk` were scanned successfully.
  // Zero of them have to be copied to the next block.
  return 0;
}

Items* scan_blocks(const char* filename) {
  assert(filename != NULL);
  assert(SCAN_BLOCK_SIZE >= MAX_TOKEN_LEN);
  
  int fd = open(filename, O_RDONLY);
  if (fd == -1)  {
    return NULL;
  }

  Items* items = new_items(filename);
  
  char* blk = (char*) malloc (SCAN_BLOCK_SIZE * sizeof(char));
  assert(blk != NULL);

  ssize_t bytes_read;
  size_t bytes_copied = 0;
  size_t len;
  size_t orig_len; // Length of original content in buffer.
                   // Always equal to `len` unless an automatic
                   // newline is inserted. Then `len` will be
                   // increased by one but `orig_len` can be used
                   // to check how much original data the block contains.

  do {
    bytes_read = read(fd, blk + bytes_copied, SCAN_BLOCK_SIZE - bytes_copied);
    if (bytes_read == -1) {
      // Error: `-1` means read error and if
      // `bytes_read > SIZE_MAX` we cannot continue
      // since we cannot cast it into a `size_t`.
      free(blk);
      close(fd);
      del_items(items);
      return NULL;
    }

    len = (size_t) bytes_read;
    len += bytes_copied;
    orig_len = len;

    // Check if this block is the end of the file.
    if (0 < len && len < SCAN_BLOCK_SIZE && blk[len - 1] != '\n') {
      // The user should provide the newline by themselves.
      warn_eof_nl();
      // The newline (or any other whitespace) is required so
      // that all tokens, including the one at the end of the
      // file, are delimited to the rear. Here a newline is
      // added automatically to still finish scanning successfully.
      blk[len] = '\n';
      len++;
    }

    // On success `res` represents the number of bytes
    // which could not be processed anymore, because
    // there were no whitespace characters between
    // them and the end of the block.
    ssize_t res = scan(items, blk, len);

    // `bytes_copied` is not used again if `res < 0` so
    // setting it to a potentially negative value is ok.
    // If `res == 0` `bytes_copied` should be set without
    // doing any copying. Otherwise it's set for copying.
    bytes_copied = (size_t) res;
    if (res == SCAN_ERR) {
      free(blk);
      close(fd);
      del_items(items);
      return NULL;
    } else if (res > 0) {
      memcpy(blk, blk + SCAN_BLOCK_SIZE - bytes_copied, bytes_copied);
    }
  } while (orig_len == SCAN_BLOCK_SIZE);
  
  free(blk);
  close(fd);

  return items;
}
