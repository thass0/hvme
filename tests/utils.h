#pragma once

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdio.h>
#include <stdlib.h>

#define TEST(name) \
  static MunitResult name(MUNIT_UNUSED const MunitParameter p[], MUNIT_UNUSED void* fixture)

#define REG_TEST(name) \
  { "/"#name, name, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }  

#define RAND_OFFSET() \
  munit_rand_int_range(0, 65535)

int check_stream(const char* expect, size_t nnoise, FILE* stream);

const char* setup_tmp(char* fn, const char* cnt);

#endif  // _UTILS_H_
