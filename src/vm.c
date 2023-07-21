#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "compiler.h"

VM vm;

static void stack_reset()
{
	vm.stack_top = vm.stack;
}

void vm_init()
{
	stack_reset();
}

void vm_free()
{
	stack_reset();
}

void vm_stack_push(Value value)
{
	*(vm.stack_top) = value;
	vm.stack_top++;
}

Value vm_stack_pop()
{
	return *(--vm.stack_top);
}

int vm_stack_size()
{
	return vm.stack_top - vm.stack;
}

static InterpretResult run()
{
#define READ_BYTE() *(vm.pc++)
#define READ_CONST() vm.chunk->constants.values[READ_BYTE()];
#define READ_BYTES(dest, n)\
	memcpy(dest, vm.pc, n);\
	vm.pc += n
#define READ_CONST_AT(offset) vm.chunk->constants.values[offset];
#define BINARY_OP(op)\
do {\
	Value a = vm_stack_pop();\
	Value b = vm_stack_pop();\
	vm_stack_push(b op a);\
} while (false)

	for (;;)
	{
#ifdef DBG_TRACE_EXECUTION
		/* Print stack values */
		printf("	");
		for (Value * ptr = vm.stack; ptr < vm.stack_top; ++ptr)
	{
			printf("[ ");
			print_value(*ptr);
			printf(" ]\n");
		}

		/* Print the instruction to be executed */
		disassemble_inst(vm.chunk, vm.pc - vm.chunk->bytecodes);
#endif
		uint8_t inst;
		switch (inst = READ_BYTE())
		{
		case OP_RETURN:
			{
				return INTERPRET_OK;
			}
		case OP_CONST:
			{
				Value constant = READ_CONST();
				vm_stack_push(constant);
				break;
			}
		case OP_CONST_LONG:
			{
				uint32_t const_offset = 0;
				READ_BYTES(&const_offset, 3);
				Value constant = READ_CONST_AT(const_offset);
				vm_stack_push(constant);
				break;
			}
		case OP_NEGATE:
			{
				Value value_param = vm_stack_pop();
				vm_stack_push(-value_param);
				break;
			}
		case OP_ADD:
			BINARY_OP(+);
			break;
		case OP_SUBTRACT:
			BINARY_OP(-);
			break;
		case OP_MUL:
			BINARY_OP(*);
			break;
		case OP_DIV:
			BINARY_OP(/);
			break;
		case META_LINE_NUM:
			vm.pc += 2;
			break;
		}
	}

#undef READ_BYTE
#undef READ_BYTES
#undef READ_CONST
#undef READ_CONST_AT
}
InterpretResult vm_interpret(Chunk * chunk)
{
	vm.chunk = chunk;
	vm.pc = chunk->bytecodes;
	return run();
}

InterpretResult interpret(const char * source)
{
	Chunk chunk;
	chunk_init(&chunk);

	if (!compile(source, &chunk))
	{
		chunk_free(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.pc = chunk.bytecodes;

	InterpretResult result = run();

	chunk_free(&chunk);
}
