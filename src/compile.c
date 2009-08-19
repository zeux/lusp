// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compile.h>

#include <lusp/compiler/lexer.h>
#include <lusp/compiler/parser.h>
#include <lusp/compiler/compiler.h>

#include <mem/arena.h>

#include <setjmp.h>
#include <stdio.h>

void error_handler(struct lusp_lexer_t* lexer, const char* message)
{
    printf("error: compile failed (%s)\n", message);

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

    struct lusp_ast_node_t* ast = lusp_parse(&lexer, arena);

    struct lusp_object_t bytecode = lusp_compile_ast(env, ast, flags);
    
    mem_arena_restore(arena, marker);
    
    return bytecode;
}