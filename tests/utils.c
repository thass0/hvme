#include "utils.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int check_stream(const char* expect, size_t nnoise, FILE* stream) {
  assert(expect != NULL);
  assert(stream != NULL);

  // `nnoise` is the number of bytes buffered for optional noise
  // contained in the stream around the expected content.

  size_t buf_len = strlen(expect) + 1 + nnoise;
  char* buf = (char*) malloc (buf_len);  // `sizeof(char)` is implicit

  rewind(stream);
  fread(buf, sizeof(char), buf_len - 1, stream);
  buf[buf_len - 1] = '\0';  // Wow! This solved the heap overflow in strstr.
  char* found = strstr(buf, expect);
  rewind(stream);

  free(buf);

  return found == NULL
    ? 0
    : 1;
}

const char* setup_tmp(char* fn, const char* cnt) {
  int fd = mkstemp(fn);
  assert(fd != -1);
  FILE* f = fdopen(fd, "w");
  assert(f != NULL);
  fputs(cnt, f);
  fclose(f);
  close(fd);
  return fn;
}
