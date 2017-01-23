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
    unsigned int marker = mem_arena_get_marker(arena);
    
    jmp_buf buf;
    struct lusp_lexer_t lexer;

    if (setjmp(buf))
    {
        mem_arena_restore(arena, marker);
        return lusp_mknull();
    }

    lusp_lexer_init(&lexer, string, &buf, error_handler);

    struct lusp_object_t bytecode = lusp_compile_ex(env, &lexer, arena, flags);
    
    mem_arena_restore(arena, marker);
    
    return bytecode;
}
