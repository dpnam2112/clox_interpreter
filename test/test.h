#ifndef TEST_H
#define TEST_H

#include "common.h"
#include "value.h"

#define TESTCASE_FAILED(testname, expected, out)\
do{\
        printf("Testcase: %-60s [\033[1;31mFAILED\033[0m]\n", testname);\
	printf("[REASON: Expected %f, but got %f]\n", expected, out);\
} while(false)

#define TESTCASE_PASSED(testname)\
        printf("Testcase: %-60s [\033[1;32mPASSED\033[0m]\n", testname)

int test(Value (*test_func)(), const char * testname, Value expected);
void test_chunk();
void test_arithmetic();
void test_stack();

#endif
