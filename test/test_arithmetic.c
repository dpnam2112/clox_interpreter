#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "test.h"


double ONE_PLUS_ONE()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 1, 1);
    chunk_write_load_const(&chunk, 1, 1);
    chunk_append(&chunk, OP_ADD, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double TWO_TIMES_FOUR()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 2, 1);
    chunk_write_load_const(&chunk, 4, 1);
    chunk_append(&chunk, OP_MUL, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double FOUR_DIV_TWO()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 4, 1);
    chunk_write_load_const(&chunk, 2, 1);
    chunk_append(&chunk, OP_DIV, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double FOUR_SUB_TWO()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 4, 1);
    chunk_write_load_const(&chunk, 2, 1);
    chunk_append(&chunk, OP_SUBTRACT, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double NEGATE_TWO()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 2, 1);
    chunk_append(&chunk, OP_NEGATE, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double NEGATE_1_POINT_1()
{
    Chunk chunk;
    chunk_init(&chunk);
    vm_init();

    chunk_write_load_const(&chunk, 1.1, 1);
    chunk_append(&chunk, OP_NEGATE, 1);
    chunk_append(&chunk, OP_RETURN, 2);

    vm_interpret(&chunk);
    double result = vm_stack_pop();

    vm_free();
    return result;
}

double COMBINATION_0()
{
	//1 * 2 + 3
	
	Chunk chunk;
	chunk_init(&chunk);
	vm_init();

	chunk_write_load_const(&chunk, 2, 1);
	chunk_write_load_const(&chunk, 1, 1);
	chunk_append(&chunk, OP_MUL, 1);
	chunk_write_load_const(&chunk, 3, 1);
	chunk_append(&chunk, OP_ADD, 1);
	chunk_append(&chunk, OP_RETURN, 2);

	vm_interpret(&chunk);
	double result = vm_stack_pop();

	vm_free();
	return result;
}

double COMBINATION_1()
{
	//1 * 2 + 3 / 5, expect 2.6
	
	Chunk chunk;
	chunk_init(&chunk);
	vm_init();

	chunk_write_load_const(&chunk, 2, 1);
	chunk_write_load_const(&chunk, 1, 1);
	chunk_append(&chunk, OP_MUL, 1);
	chunk_write_load_const(&chunk, 3, 1);
	chunk_write_load_const(&chunk, 5, 1);
	chunk_append(&chunk, OP_DIV, 1);
    chunk_append(&chunk, OP_ADD, 1);
	chunk_append(&chunk, OP_RETURN, 2);

	vm_interpret(&chunk);
	double result = vm_stack_pop();

	vm_free();
	return result;
}

double COMBINATION_2()
{
	//-4 / 5 + 5 * 2 - 7, expect 2.2
	
	Chunk chunk;
	chunk_init(&chunk);
	vm_init();

	chunk_write_load_const(&chunk, 4, 1);
	chunk_append(&chunk, OP_NEGATE, 1);
	chunk_write_load_const(&chunk, 5, 1);
	chunk_append(&chunk, OP_DIV, 1);
	chunk_write_load_const(&chunk, 5, 1);
	chunk_write_load_const(&chunk, 2, 1);
	chunk_append(&chunk, OP_MUL, 1);
	chunk_write_load_const(&chunk, 7, 1);
	chunk_append(&chunk, OP_SUBTRACT, 1);
    chunk_append(&chunk, OP_ADD, 1);
	chunk_append(&chunk, OP_RETURN, 2);

	vm_interpret(&chunk);
	Value result = vm_stack_pop();

	vm_free();
	return result;
}

void test_arithmetic()
{
	printf("== begin testing simple arithmetic ==\n");
	test(ONE_PLUS_ONE, "1 + 1", 2);
	test(TWO_TIMES_FOUR, "2 * 4", 8);
	test(FOUR_SUB_TWO, "4 - 2", 2);
	test(FOUR_DIV_TWO, "4 / 2", 2);
	test(NEGATE_TWO, "-2", -2);
	test(NEGATE_1_POINT_1, "-1.1", -1.1);
	test(COMBINATION_0, "1 * 2 + 3", 5);
	test(COMBINATION_1, "1 * 2 + 3 / 5", 2.6);
	test(COMBINATION_2, "-4 / 5 + 5 * 2 - 7", 
            ((double) -4.0 / 5 + 5 * 2 - 7));
	printf("== end of test ==\n");
}
