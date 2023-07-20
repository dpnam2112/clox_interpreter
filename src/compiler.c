#include "compiler.h"

void compile(const char * source)
{
	int line = -1;
	scanner_init(source);
	while (true)
	{
		Token token = scan_token();
		if (token.line != line)
		{
			printf("%4d ", token.line);
			line = token.line;
		}
		else
		{
			printf("   | ");
		}
		printf("%2d '%.*s'\n", token.type, token.length, token.start);
		if (token.type == TK_EOF)
			break;
	}
	scanner_free();
}
