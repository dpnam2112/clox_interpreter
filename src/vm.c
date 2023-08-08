#include "vm.h"
#include "chunk.h"
#include "debug.h"
#include "value.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include <stdarg.h>

VM vm;

static void stack_reset()
{
	vm.stack_top = vm.stack;
}

void vm_init(bool repl)
{
	stack_reset();
	table_init(&vm.strings);
	table_init(&vm.globals);
	vm.repl = repl;
}

void vm_free()
{
	table_free(&vm.strings);
	table_free(&vm.globals);
	free_objects();
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

static Value vm_stack_peek(int distance)
{
	return vm.stack_top[-1 - distance];	// ??
}

static bool is_falsey(Value val)
{
	return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val));
}

static void concatenate()
{
	StringObj * right = AS_STRING(vm_stack_pop());
	StringObj * left = AS_STRING(vm_stack_pop());

	size_t total_length = right->length + left->length;

	char * concat_string = malloc(total_length + 1);
	memcpy(concat_string, left->chars, left->length);
	memcpy(concat_string + left->length, right->chars, right->length);
	concat_string[total_length] = '\0';

	StringObj * concat_str_obj = StringObj_construct(concat_string, total_length);

	free(concat_string);
	vm_stack_push(OBJ_VAL(*concat_str_obj));

}

/* runtime_error: print out error message to stderr and reset the stack */
static void runtime_error(const char * format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);

	size_t inst_offset = vm.pc - vm.chunk->bytecodes - 1;
	uint16_t line = chunk_get_line(vm.chunk, inst_offset);
	fprintf(stderr, "[line %d] in script\n", line);
	stack_reset();
}

/* read n bytes from @vm.bytecodes, starting from @start.
 * @byte_count: number of bytes which represents the offset
 *
 * @byte_count does not exceed 4. If it does, it will be changed to 4.
 * return value: positive integer value */
static uint32_t read_bytes(uint8_t byte_count)
{
	if (byte_count > 4) byte_count = 4;
	uint32_t bytes = 0;
	memcpy(&bytes, vm.pc, byte_count);
	vm.pc += byte_count;
	return bytes;
}

static InterpretResult run()
{
#define READ_BYTE() *(vm.pc++)
#define READ_CONST() vm.chunk->constants.values[READ_BYTE()]
#define READ_BYTES(dest, n)\
	memcpy(dest, vm.pc, n);\
	vm.pc += n
#define READ_CONST_AT(offset) vm.chunk->constants.values[offset]
#define BINARY_OP(value_type, op)\
do {\
	if (!(IS_NUMBER(vm_stack_peek(0)) && IS_NUMBER(vm_stack_peek(1)))) \
	{\
		runtime_error("Operands must be numbers.");\
		return INTERPRET_RUNTIME_ERROR;\
	}\
	double right = AS_NUMBER(vm_stack_pop());\
	double left = AS_NUMBER(vm_stack_pop());\
	vm_stack_push(value_type(left op right));\
} while (false)

	for (;;)
	{
#ifdef DBG_TRACE_EXECUTION
		/* Print stack values */
		for (Value * ptr = vm.stack; ptr < vm.stack_top; ++ptr)
		{
			if (ptr == vm.stack)
				printf("	");
			printf("[ ");
			print_value(*ptr);
			printf(" ]\n");
			if (ptr + 1 == vm.stack_top)
				printf("\n");
		}
#endif
		uint8_t inst;
		switch (inst = READ_BYTE())
		{
		case OP_EXIT:
			return INTERPRET_OK;
		case OP_RETURN:
			{
				print_value(vm_stack_pop());
				printf("\n");
				break;
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
		case OP_TRUE:
			{
				vm_stack_push(BOOL_VAL(true));
				break;
			}
		case OP_FALSE:
			{
				vm_stack_push(BOOL_VAL(false));
				break;
			}
		case OP_NIL:
			{
				vm_stack_push(NIL_VAL());
				break;
			}
		case OP_NEGATE:
			{
				if (vm_stack_peek(0).type != VAL_NUMBER)
				{
					runtime_error("Cannot negate an object that is not numeric");
					return INTERPRET_RUNTIME_ERROR;
				}

				Value val_wrapper = vm_stack_pop();
				double negated_val = -AS_NUMBER(val_wrapper);
				vm_stack_push(NUMBER_VAL(negated_val));
				break;
			}
		case OP_NOT:
			{
				Value val_wrapper = vm_stack_pop();
				vm_stack_push(BOOL_VAL(is_falsey(val_wrapper)));
				break;
			}
		case OP_ADD:
			{
				Value right = vm_stack_peek(0);
				Value left = vm_stack_peek(1);

				bool bothAreNums = right.type == VAL_NUMBER && left.type == VAL_NUMBER;
				bool bothAreStrings = OBJ_TYPE(right) == OBJ_STRING && OBJ_TYPE(left) == OBJ_STRING;

				if (!(bothAreNums || bothAreStrings))
				{
					runtime_error("Both operands must be strings or numbers");
					return INTERPRET_RUNTIME_ERROR;
				}

				if (right.type == VAL_NUMBER)
				{
					vm_stack_pop();
					vm_stack_pop();
					vm_stack_push(NUMBER_VAL(AS_NUMBER(right) + AS_NUMBER(left)));
				}
				else
					/* concatenate two strings
					 * both operands are popped in the function */
					concatenate();
				break;

			}
			BINARY_OP(NUMBER_VAL, +);
			break;
		case OP_SUBTRACT:
			BINARY_OP(NUMBER_VAL, -);
			break;
		case OP_MUL:
			BINARY_OP(NUMBER_VAL, *);
			break;
		case OP_DIV:
			BINARY_OP(NUMBER_VAL, /);
			break;
		case OP_EQUAL:
			{
				Value x = vm_stack_pop();
				Value y = vm_stack_pop();
				vm_stack_push(BOOL_VAL(value_equal(x, y)));
				break;
			}
		case OP_LESS:
			BINARY_OP(BOOL_VAL, <);
			break;
		case OP_GREATER:
			BINARY_OP(BOOL_VAL, >);
			break;
		case META_LINE_NUM:
			vm.pc += 2;
			break;
		case OP_PRINT:
			{
				Value value = vm_stack_pop();
				print_value(value);
				printf("\n");
				break;
			}
		case OP_POP:
			{
				vm_stack_pop();
				break;
			}
		case OP_DEFINE_GLOBAL:
			{
				uint32_t iden_offset = read_bytes(3);
				StringObj * identifier = AS_OBJ(READ_CONST_AT(iden_offset));
				table_set(&vm.globals, identifier, vm_stack_peek(0));
				vm_stack_pop();
				break;
			}
		case OP_GET_GLOBAL:
			{
				uint32_t offset = read_bytes(3);
				StringObj * identifier = AS_OBJ(READ_CONST_AT(offset));
				Value value;
				if (!table_get(&vm.globals, identifier, &value))
				{
					runtime_error("Undefined identifier: '%s'.", identifier->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				else
					vm_stack_push(value);
				break;
			}
		case OP_SET_GLOBAL:
			{
				uint32_t offset = read_bytes(3);
				StringObj * identifier = AS_OBJ(READ_CONST_AT(offset));
				if (table_set(&vm.globals, identifier, vm_stack_peek(0)))
				{
					runtime_error("Undefined identifier: '%s'.", identifier->chars);
					table_delete(&vm.globals, identifier, NULL);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
		case OP_GET_LOCAL:
			{
				uint32_t stack_index = read_bytes(3);
				Value local_val = vm.stack[stack_index];
				vm_stack_push(local_val);
				break;
			}
		case OP_SET_LOCAL:
			{
				uint32_t stack_index = read_bytes(3);
				vm.stack[stack_index] = vm_stack_peek(0);
				break;
			}
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

	/* Compile the source code and put the output in @chunk */
	if (!compile(source, &chunk))
	{
		chunk_free(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}

	vm.chunk = &chunk;
	vm.pc = chunk.bytecodes;

#ifdef BYTECODE_DUMP
	chunk_bytecode_dump(&chunk, "Dump bytecodes");
#endif

#ifdef DBG_DISASSEMBLE
	/* Print the instruction to be executed */
	disassemble_chunk(&chunk, "Disassemble chunk");
#endif


	InterpretResult result = run();

	chunk_free(&chunk);
	return result;
}

