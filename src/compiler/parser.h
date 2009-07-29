// DeepLight Engine (c) Zeux 2006-2009

#pragma once

struct lusp_object_t;

enum lusp_ast_node_type_t
{
	LUSP_AST_LITERAL,
	LUSP_AST_CONS,
	LUSP_AST_SYMBOL,
	LUSP_AST_SYMBOL_QUOTE,
	LUSP_AST_SYMBOL_QUASIQUOTE,
	LUSP_AST_SYMBOL_UNQUOTE,
	LUSP_AST_SYMBOL_UNQUOTE_SPLICING,
	LUSP_AST_SYMBOL_BEGIN,
	LUSP_AST_SYMBOL_DEFINE,
	LUSP_AST_SYMBOL_LAMBDA,
	LUSP_AST_SYMBOL_LET,
	LUSP_AST_SYMBOL_LETSEQ,
	LUSP_AST_SYMBOL_LETREC,
	LUSP_AST_SYMBOL_SET,
	LUSP_AST_SYMBOL_IF,
	LUSP_AST_SYMBOL_WHEN,
	LUSP_AST_SYMBOL_UNLESS
};

struct lusp_ast_node_t
{
	enum lusp_ast_node_type_t type;
	unsigned int line;
	
	union
	{
		struct lusp_object_t* literal;
		struct lusp_object_t* symbol;
		
		struct
		{
			struct lusp_ast_node_t* car;
			struct lusp_ast_node_t* cdr;
		} cons;
	};
};

struct mem_arena_t;
struct lusp_lexer_t;

struct lusp_ast_node_t* lusp_parse(struct lusp_lexer_t* lexer, struct mem_arena_t* arena);