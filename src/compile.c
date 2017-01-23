#include "compile.h"

#include "lexer.h"
#include "compiler.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>

void error_handler(struct lusp_lexer_t* lexer, const char* message, ...)
{
    printf("error: compile failed (");
    
	va_list list;
	
	va_start(list, message);
    vprintf(message, list);
    va_end(list);
    
    printf(") at line %d\n", lexer->lexeme_line);

    longjmp(*(jmp_buf*)lexer->error_context, 1);
}

struct lusp_object_t lusp_compile(struct lusp_environment_t* env, struct mem_arena_t* arena, const char* string, unsigned int flags)
{
    jmp_buf buf;
    if (setjmp(buf))
        return lusp_mknull();

    struct lusp_lexer_t lexer;
    lusp_lexer_init(&lexer, string, &buf, error_handler);

    struct lusp_object_t bytecode = lusp_compile_ex(env, &lexer, arena, flags);
    return bytecode;
}
