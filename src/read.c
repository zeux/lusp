// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/read.h>

#include <lusp/compiler/lexer.h>
#include <lusp/object.h>

static inline void check(struct lusp_lexer_t* lexer, bool condition, const char* message)
{
	if (!condition) lexer->error_handler(lexer, message);
}

static struct lusp_object_t* read_atom(struct lusp_lexer_t* lexer);

static struct lusp_object_t* read_list(struct lusp_lexer_t* lexer)
{
	// skip open brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_OPEN_BRACE);
	lusp_lexer_next(lexer);
	
	struct lusp_object_t* head = 0;
	struct lusp_object_t* tail = 0;
	
	// read a list of atoms
	while (lexer->lexeme != LUSP_LEXEME_CLOSE_BRACE)
	{
		if (lexer->lexeme == LUSP_LEXEME_DOT)
		{
			// read dotted pair
			check(lexer, tail != 0, "invalid dotted pair");
			
			// read last atom
			lusp_lexer_next(lexer);
			struct lusp_object_t* atom = read_atom(lexer);

			// append atom
			tail->cons.cdr = atom;

			// check that the atom was actually last one
			check(lexer, lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE, "invalid dotted pair");
			
			break;
		}
		
		// read atom
		struct lusp_object_t* atom = read_atom(lexer);
		
		// append atom
		if (tail) tail = tail->cons.cdr = lusp_mkcons(atom, 0);
		else head = tail = lusp_mkcons(atom, 0);
	}
	
	// skip closing brace
	DL_ASSERT(lexer->lexeme == LUSP_LEXEME_CLOSE_BRACE);
	lusp_lexer_next(lexer);

	return head;
}

static struct lusp_object_t* read_atom(struct lusp_lexer_t* lexer)
{
#define RETURN(lexeme, ret) case lexeme: { struct lusp_object_t* r = ret; lusp_lexer_next(lexer); return r; }
#define RETURNLIST(lexeme, symbol) case lexeme: { lusp_lexer_next(lexer); return lusp_mkcons(lusp_mksymbol(symbol), lusp_mkcons(read_atom(lexer), 0)); }
	
	switch (lexer->lexeme)
	{
	RETURN(LUSP_LEXEME_LITERAL_BOOLEAN, lusp_mkboolean(lexer->value.boolean));
	RETURN(LUSP_LEXEME_LITERAL_INTEGER, lusp_mkinteger(lexer->value.integer));
	RETURN(LUSP_LEXEME_LITERAL_REAL, lusp_mkreal(lexer->value.real));
	RETURN(LUSP_LEXEME_LITERAL_STRING, lusp_mkstring(lexer->value.string));
	RETURN(LUSP_LEXEME_SYMBOL, lusp_mksymbol(lexer->value.symbol));
	RETURNLIST(LUSP_LEXEME_QUOTE, "quote");
	RETURNLIST(LUSP_LEXEME_BACKQUOTE, "quasiquote");
	RETURNLIST(LUSP_LEXEME_COMMA, "unquote");
	RETURNLIST(LUSP_LEXEME_COMMA_AT, "unquote-splicing");
	
	case LUSP_LEXEME_OPEN_BRACE:
		return read_list(lexer);
		
	default:
		check(lexer, false, "unexpected token");
		return 0;
	}
	
#undef RETURNLIST
#undef RETURN
}

struct lusp_object_t* lusp_read(struct lusp_lexer_t* lexer)
{
	return read_atom(lexer);
}