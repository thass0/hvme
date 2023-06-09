#pragma once

#ifndef _EXEC_H_
#define _EXEC_H_

#include "prog.h"

#define EXEC_ERR -1

// Execute the program. Returns
// `0` on success and `EXEC_ERR` if
// an error arises during execution.
int exec_prog(Program* program);

#endif  // _EXEC_H_
