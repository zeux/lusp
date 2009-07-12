// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/lexer.h>

#include <lusp/object.h>

#include <core/string.h>

static struct lusp_object_t* reverse(struct lusp_object_t* list)
{
	struct lusp_object_t* prev = 0;
	
	while (list)
	{
		DL_ASSERT(list->type == LUSP_OBJECT_CONS);
	
		struct lusp_object_t* cdr = list->cons.cdr;
		list->cons.cdr = prev;
		
		prev = list;
		list = cdr;
	}
	
	return prev;
}

static struct lusp_object_t* read_atom(struct lusp_lexer_t* lexer);

static struct lusp_object_t* read_string(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_STRING);
	
	struct lusp_object_t* result = lusp_mkstring(lexer->value);
	
	lusp_lexer_next(lexer);
	
	return result;
}

static struct lusp_object_t* read_symbol(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_IDENTIFIER);
	
	struct lusp_object_t* result = lusp_mksymbol(lexer->value);
	
	lusp_lexer_next(lexer);
	
	return result;
}

static struct lusp_object_t* read_integer_hex(struct lusp_lexer_t* lexer)
{
	int result = 0;
	
	for (const char* s = lexer->value + 2; *s; ++s)
	{
		int digit = (*s >= '0' && *s <= '9') ? *s - '0' : (*s >= 'a' && *s <= 'f') ? (*s - 'a') + 10 : (*s - 'A') + 10;
		
		result = result * 16 + digit;
	}
	
	lusp_lexer_next(lexer);
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_integer_dec(struct lusp_lexer_t* lexer)
{
	int result = 0;
	
	for (const char* s = lexer->value; *s; ++s)
	{
		int digit = *s - '0';
		result = result * 10 + digit;
	}
	
	lusp_lexer_next(lexer);
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_number(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_IDENTIFIER);
	
	return read_integer_dec(lexer);
}

static struct lusp_object_t* read_identifier(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_IDENTIFIER);
	
	char ch = lexer->value[0];
	
	if (ch == '.' || (ch >= '0' && ch <= '9')) return read_number(lexer);
	else if (ch == '#')
	{
		switch (lexer->value[1])
		{
		case 't':
		case 'T':
			lusp_lexer_next(lexer);
			return lusp_mkboolean(true);
			
		case 'f':
		case 'F':
			lusp_lexer_next(lexer);
			return lusp_mkboolean(false);
			
		case 'x':
		case 'X':
			return read_integer_hex(lexer);
			
		default:
			DL_ASSERT(false); // read error, actually
			return 0;
		}
	}
	else return read_symbol(lexer);
}

static struct lusp_object_t* read_datum(struct lusp_lexer_t* lexer)
{
	return (lexer->lexeme == LUSP_LEX_STRING) ? read_string(lexer) : read_identifier(lexer);
}

static struct lusp_object_t* read_list(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_OPEN_PARENS);
	lusp_lexer_next(lexer);
	
	struct lusp_object_t* result = 0;
	
	// read a list of atoms
	struct lusp_object_t* atom;

	while ((atom = read_atom(lexer)) != 0) result = lusp_mkcons(atom, result);
	
	if (lexer->lexeme == LUSP_LEX_CLOSE_PARENS)
	{
		lusp_lexer_next(lexer);
		
		return reverse(result);
	}
	else
	{
		DL_ASSERT(false); // read error, actually
		return 0;
	}
}

static struct lusp_object_t* read_quote(struct lusp_lexer_t* lexer)
{
	DL_ASSERT(lexer->lexeme == LUSP_LEX_APOSTROPHE);
	
	lusp_lexer_next(lexer);
	
	struct lusp_object_t* atom = read_atom(lexer);
	
	return atom ? lusp_mkcons(lusp_mksymbol("quote"), atom) : 0;
}

static struct lusp_object_t* read_atom(struct lusp_lexer_t* lexer)
{
	switch (lexer->lexeme)
	{
	case LUSP_LEX_OPEN_PARENS:
		return read_list(lexer);
		
	case LUSP_LEX_APOSTROPHE:
		return read_quote(lexer);
		
	case LUSP_LEX_IDENTIFIER:
	case LUSP_LEX_STRING:
		return read_datum(lexer);
		
	default:
		return 0;
	}
}

static struct lusp_object_t* read_program(struct lusp_lexer_t* lexer)
{
	return read_atom(lexer);
}

struct lusp_object_t* lusp_read(const char* data)
{
	struct lusp_lexer_t lexer;
	
	lusp_lexer_init(&lexer, data);
	
	return read_program(&lexer);
}