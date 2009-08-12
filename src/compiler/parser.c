// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compiler/parser.h>

#include <lusp/compiler/lexer.h>
#include <lusp/object.h>

#include <mem/arena.h>

struct symbols_t
{
	struct lusp_object_t* quote;
	struct lusp_object_t* quasiquote;
	struct lusp_object_t* unquote;
	struct lusp_object_t* unquote_splicing;
	struct lusp_object_t* begin;
	struct lusp_object_t* define;
	struct lusp_object_t* lambda;
	struct lusp_object_t* let;
	struct lusp_object_t* letseq;
	struct lusp_object_t* letrec;
	struct lusp_object_t* set;
	struct lusp_object_t* if_;
	struct lusp_object_t* when;
	struct lusp_object_t* unless;
	struct lusp_object_t* do_;
};

struct parser_t
{
	// lexer
	struct lusp_lexer_t* lexer;
	
	// arena for ast
	struct mem_arena_t* arena;
	
	// symbol table
	struct symbols_t* symbols;
};

static inline void check(struct parser_t* parser, bool condition, const char* message)
{
	if (!condition) parser->lexer->error_handler(parser->lexer, message);
}

static inline struct lusp_ast_node_t* mknode(struct parser_t* parser, enum lusp_ast_node_type_t type)
{
	struct lusp_ast_node_t* result = MEM_ARENA_NEW(parser->arena, struct lusp_ast_node_t);
	check(parser, result != 0, "not enough memory to allocate ast");
	
	result->type = type;
	result->line = parser->lexer->lexeme_line;
	return result;
}

static inline struct lusp_ast_node_t* mkliteral(struct parser_t* parser, struct lusp_object_t* value)
{
	struct lusp_ast_node_t* result = mknode(parser, LUSP_AST_LITERAL);
	result->literal = value;
	return result;
}

static inline struct lusp_ast_node_t* mksymbol(struct parser_t* parser, const char* name)
{
	struct lusp_object_t* symbol = lusp_mksymbol(name);
	
	// create basic node
	struct lusp_ast_node_t* result = mknode(parser, LUSP_AST_SYMBOL);
	result->symbol = symbol;
	
	// correct type for special symbols
	struct symbols_t* symbols = parser->symbols;
	
	if (symbol == symbols->quote) result->type = LUSP_AST_SYMBOL_QUOTE;
	else if (symbol == symbols->quasiquote) result->type = LUSP_AST_SYMBOL_QUASIQUOTE;
	else if (symbol == symbols->unquote) result->type = LUSP_AST_SYMBOL_UNQUOTE;
	else if (symbol == symbols->unquote_splicing) result->type = LUSP_AST_SYMBOL_UNQUOTE_SPLICING;
	else if (symbol == symbols->begin) result->type = LUSP_AST_SYMBOL_BEGIN;
	else if (symbol == symbols->define) result->type = LUSP_AST_SYMBOL_DEFINE;
	else if (symbol == symbols->lambda) result->type = LUSP_AST_SYMBOL_LAMBDA;
	else if (symbol == symbols->let) result->type = LUSP_AST_SYMBOL_LET;
	else if (symbol == symbols->letseq) result->type = LUSP_AST_SYMBOL_LETSEQ;
	else if (symbol == symbols->letrec) result->type = LUSP_AST_SYMBOL_LETREC;
	else if (symbol == symbols->set) result->type = LUSP_AST_SYMBOL_SET;
	else if (symbol == symbols->if_) result->type = LUSP_AST_SYMBOL_IF;
	else if (symbol == symbols->when) result->type = LUSP_AST_SYMBOL_WHEN;
	else if (symbol == symbols->unless) result->type = LUSP_AST_SYMBOL_UNLESS;
	else if (symbol == symbols->do_) result->type = LUSP_AST_SYMBOL_DO;
	
	return result;
}

static inline struct lusp_ast_node_t* mkcons(struct parser_t* parser, struct lusp_ast_node_t* car, struct lusp_ast_node_t* cdr)
{
	struct lusp_ast_node_t* result = mknode(parser, LUSP_AST_CONS);
	result->cons.car = car;
	result->cons.cdr = cdr;
	return result;
}

static struct lusp_ast_node_t* read_atom(struct parser_t* parser);

static struct lusp_ast_node_t* read_list(struct parser_t* parser)
{
	struct lusp_lexer_t* lexer = parser->lexer;
	
	// skip open brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_BRACE);
	lusp_lexer_next(lexer);
	
	struct lusp_ast_node_t* head = 0;
	struct lusp_ast_node_t* tail = 0;
	
	// read a list of atoms
	while (lexer->lexeme != LUSP_LEXEME_CLOSE_BRACE)
	{
		if (lexer->lexeme == LUSP_LEXEME_DOT)
		{
			// read dotted pair
			check(parser, tail != 0, "invalid dotted pair");
			
			// read last atom
			lusp_lexer_next(lexer);
			struct lusp_ast_node_t* atom = read_atom(parser);

			// append atom
			tail->cons.cdr = atom;

			// check that the atom was actually last one
			check(parser, lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE, "invalid dotted pair");
			
			break;
		}
		
		// read atom
		struct lusp_ast_node_t* atom = read_atom(parser);
		
		// append atom
		if (tail) tail = tail->cons.cdr = mkcons(parser, atom, 0);
		else head = tail = mkcons(parser, atom, 0);
	}
	
	// skip closing brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE);
	lusp_lexer_next(lexer);

	return head;
}

static inline struct lusp_ast_node_t* next_lexeme(struct lusp_lexer_t* lexer, struct lusp_ast_node_t* result)
{
	lusp_lexer_next(lexer);
	return result;
}

static inline struct lusp_ast_node_t* read_symbol_list(struct parser_t* parser, const char* symbol)
{
	lusp_lexer_next(parser->lexer);
	return mkcons(parser, mksymbol(parser, symbol), mkcons(parser, read_atom(parser), 0));
}

static struct lusp_ast_node_t* read_atom(struct parser_t* parser)
{
	struct lusp_lexer_t* lexer = parser->lexer;
	
	switch (lexer->lexeme)
	{
	case LUSP_LEXEME_LITERAL_BOOLEAN:
		return next_lexeme(lexer, mkliteral(parser, lusp_mkboolean(lexer->value.boolean)));
		
	case LUSP_LEXEME_LITERAL_INTEGER:
		return next_lexeme(lexer, mkliteral(parser, lusp_mkinteger(lexer->value.integer)));
		
	case LUSP_LEXEME_LITERAL_REAL:
		return next_lexeme(lexer, mkliteral(parser, lusp_mkreal(lexer->value.real)));
		
	case LUSP_LEXEME_LITERAL_STRING:
		return next_lexeme(lexer, mkliteral(parser, lusp_mkstring(lexer->value.string)));
		
	case LUSP_LEXEME_SYMBOL:
		return next_lexeme(lexer, mksymbol(parser, lexer->value.symbol));
		
	case LUSP_LEXEME_QUOTE:
		return read_symbol_list(parser, "quote");
		
	case LUSP_LEXEME_BACKQUOTE:
		return read_symbol_list(parser, "quasiquote");
		
	case LUSP_LEXEME_COMMA:
		return read_symbol_list(parser, "unquote");
		
	case LUSP_LEXEME_COMMA_AT:
		return read_symbol_list(parser, "unquote-splicing");
		
	case LUSP_LEXEME_OPEN_BRACE:
		return read_list(parser);
		
	default:
		check(parser, false, "unexpected token");
		return 0;
	}
}

struct lusp_ast_node_t* lusp_parse(struct lusp_lexer_t* lexer, struct mem_arena_t* arena)
{
	// create symbol table
	struct symbols_t symbols =
	{
		lusp_mksymbol("quote"), lusp_mksymbol("quasiquote"), lusp_mksymbol("unquote"),
		lusp_mksymbol("unquote-splicing"), lusp_mksymbol("begin"), lusp_mksymbol("define"),
		lusp_mksymbol("lambda"), lusp_mksymbol("let"), lusp_mksymbol("let*"), lusp_mksymbol("letrec"),
		lusp_mksymbol("set!"), lusp_mksymbol("if"), lusp_mksymbol("when"), lusp_mksymbol("unless"),
		lusp_mksymbol("do")
	};

	// create parser 
	struct parser_t parser = {lexer, arena, &symbols};

	// parse
	struct lusp_ast_node_t* head = 0;
	struct lusp_ast_node_t* tail = head;

	while (lexer->lexeme != LUSP_LEXEME_EOF)
	{
		struct lusp_ast_node_t* atom = read_atom(&parser);

		if (tail) tail = tail->cons.cdr = mkcons(&parser, atom, 0);
		else head = tail = mkcons(&parser, atom, 0);
	}
	
	return head;
}