// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/read.h>

#include <lusp/compiler/lexer.h>
#include <lusp/object.h>

static inline void check(struct lusp_lexer_t* lexer, bool condition, const char* message)
{
	if (!condition) lexer->error_handler(lexer, message);
}

static struct lusp_object_t read_atom(struct lusp_lexer_t* lexer);

static struct lusp_object_t read_list(struct lusp_lexer_t* lexer)
{
	// skip open brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_BRACE);
	lusp_lexer_next(lexer);
	
	struct lusp_object_t head = lusp_mknull();
	struct lusp_object_t tail = lusp_mknull();
	
	// read a list of atoms
	while (lexer->lexeme != LUSP_LEXEME_CLOSE_BRACE)
	{
		if (lexer->lexeme == LUSP_LEXEME_DOT)
		{
			// read dotted pair
			check(lexer, tail.type == LUSP_OBJECT_CONS, "invalid dotted pair");
			
			// read last atom
			lusp_lexer_next(lexer);
			struct lusp_object_t atom = read_atom(lexer);

			// append atom
			*tail.cons.cdr = atom;

			// check that the atom was actually last one
			check(lexer, lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE, "invalid dotted pair");
			
			break;
		}
		
		// read atom
		struct lusp_object_t atom = read_atom(lexer);
		
		// append atom
		if (tail.type != LUSP_OBJECT_NULL) tail = *tail.cons.cdr = lusp_mkcons(atom, lusp_mknull());
		else head = tail = lusp_mkcons(atom, lusp_mknull());
	}
	
	// skip closing brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE);
	lusp_lexer_next(lexer);

	return head;
}

static inline struct lusp_object_t next_lexeme(struct lusp_lexer_t* lexer, struct lusp_object_t result)
{
	lusp_lexer_next(lexer);
	return result;
}

static inline struct lusp_object_t read_symbol_list(struct lusp_lexer_t* lexer, const char* symbol)
{
	lusp_lexer_next(lexer);
	return lusp_mkcons(lusp_mksymbol(symbol), lusp_mkcons(read_atom(lexer), lusp_mknull()));
}

static struct lusp_object_t read_atom(struct lusp_lexer_t* lexer)
{
	switch (lexer->lexeme)
	{
	case LUSP_LEXEME_LITERAL_BOOLEAN:
		return next_lexeme(lexer, lusp_mkboolean(lexer->value.boolean));
		
	case LUSP_LEXEME_LITERAL_INTEGER:
		return next_lexeme(lexer, lusp_mkinteger(lexer->value.integer));
		
	case LUSP_LEXEME_LITERAL_REAL:
		return next_lexeme(lexer, lusp_mkreal(lexer->value.real));
		
	case LUSP_LEXEME_LITERAL_STRING:
		return next_lexeme(lexer, lusp_mkstring(lexer->value.string));
		
	case LUSP_LEXEME_SYMBOL:
		return next_lexeme(lexer, lusp_mksymbol(lexer->value.symbol));
		
	case LUSP_LEXEME_QUOTE:
		return read_symbol_list(lexer, "quote");
		
	case LUSP_LEXEME_BACKQUOTE:
		return read_symbol_list(lexer, "quasiquote");
		
	case LUSP_LEXEME_COMMA:
		return read_symbol_list(lexer, "unquote");
		
	case LUSP_LEXEME_COMMA_AT:
		return read_symbol_list(lexer, "unquote-splicing");
		
	case LUSP_LEXEME_OPEN_BRACE:
		return read_list(lexer);
		
	default:
		check(lexer, false, "unexpected token");
		return lusp_mknull();
	}
}

struct lusp_object_t lusp_read(struct lusp_lexer_t* lexer)
{
	return read_atom(lexer);
}