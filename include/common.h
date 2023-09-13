#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

///* Configurations */
//#define DBG_TRACE_EXECUTION
#define DBG_DISASSEMBLE
#define BYTECODE_DUMP

#define SET_BIT(x, n) (1 << n) | x

#endif
