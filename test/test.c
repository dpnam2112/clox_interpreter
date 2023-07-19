#include "test.h"

int test(Value (*test_func)(), const char * testname, Value expected)
{
    Value res = test_func();
    if (expected - res == 0)
        TESTCASE_PASSED(testname);
    else
        TESTCASE_FAILED(testname, expected, res);
    return (res == expected);
}
