// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/read.h>

#include <lusp/object.h>

#include <core/string.h>

#include <setjmp.h>
#include <stdio.h>

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

struct reader_t
{
	const char* data;
	jmp_buf* error;
};

static inline bool is_whitespace(char data)
{
	return (unsigned char)(data - 1) < ' ';
}

static inline bool is_delimiter(char data)
{
	return is_whitespace(data) || data == '(' || data == ')' || data == '"' || data == ';';
}

static inline bool is_range(char data, char min, char max)
{
	return (unsigned char)(data - min) <= (unsigned char)(max - min);
}

static inline bool is_digit(char data)
{
	return is_range(data, '0', '9');
}

static inline char getchar(struct reader_t* reader)
{
	return *reader->data++;
}

static inline char peekchar(struct reader_t* reader)
{
	return *reader->data;
}

static inline void check(struct reader_t* reader, bool condition)
{
	if (!condition) longjmp(*reader->error, 1);
}

static inline void skipws(struct reader_t* reader)
{
	// skip whitespace
	while (is_whitespace(peekchar(reader))) getchar(reader);
	
	// skip comment
	if (peekchar(reader) == ';')
	{
		char ch;
		
		while ((ch = peekchar(reader)) != 0 && ch != '\n') getchar(reader);
		
		// skip trailing ws
		skipws(reader);
	}
}

static struct lusp_object_t* read_string(struct reader_t* reader)
{
	char value[1025];
	
	char* buffer = value;
	char* buffer_end = buffer + sizeof(value) - 1;

	// skip "
	char ch = getchar(reader);
	DL_ASSERT(ch == '"');

	// scan for closing quote
	while ((ch = getchar(reader)) != 0 && ch != '"')
	{
		if (ch == '\\')
		{
			char escaped = getchar(reader);
			check(reader, buffer < buffer_end && escaped != 0);

			*buffer++ = escaped;
		}
		else
		{
			check(reader, buffer < buffer_end);

			*buffer++ = ch;
		}
	}
	
	check(reader, ch == '"');

	// this is safe since buffer is one byte larger than buffer_end tells us
	*buffer = 0;
	
	return lusp_mkstring(value);
} 

static struct lusp_object_t* read_symbol(struct reader_t* reader)
{
	char value[1025];
	
	char* buffer = value;
	char* buffer_end = buffer + sizeof(value) - 1;
	
	// scan for delimiter
	char ch;

	while ((ch = peekchar(reader)) != 0 && !is_delimiter(ch))
	{
		check(reader, buffer < buffer_end);
		
		*buffer++ = ch;
		
		getchar(reader);
	}
	
	// no empty symbols
	check(reader, buffer > value);
	
	// this is safe since buffer is one byte larger than buffer_end tells us
	*buffer = 0;
	
	return lusp_mksymbol(value);
}

static struct lusp_object_t* read_integer_hex(struct reader_t* reader)
{
	int result = 0;
	
	char ch;
	
	while ((ch = peekchar(reader)) != 0 && !is_delimiter(ch))
	{
		check(reader, is_digit(ch) || is_range(ch, 'a', 'f') || is_range(ch, 'A', 'F'));
		
		int digit = is_digit(ch) ? ch - '0' : is_range(ch, 'a', 'f') ? ch - 'a' : ch - 'A';
		
		result = result * 16 + digit;
		
		getchar(reader);
	}
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_integer_dec(struct reader_t* reader)
{
	int result = 0;
	
	char ch;
	
	while ((ch = peekchar(reader)) != 0 && !is_delimiter(ch))
	{
		check(reader, is_digit(ch));
		
		int digit = ch - '0';
		
		result = result * 10 + digit;
		
		getchar(reader);
	}
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_datum(struct reader_t* reader)
{
	char ch = peekchar(reader);
	
	if (ch == '.' || is_digit(ch))
		return read_integer_dec(reader); // TODO: real
	else if (ch == '#')
	{
		getchar(reader);
		
		switch (getchar(reader))
		{
		case 't':
		case 'T':
			return lusp_mkboolean(true);
			
		case 'f':
		case 'F':
			return lusp_mkboolean(false);
			
		case 'x':
		case 'X':
			return read_integer_hex(reader);
			
		default:
			check(reader, false);
			return 0;
		}
	}
	else if (ch == '"')
		return read_string(reader);
	else
		return read_symbol(reader);
}

static struct lusp_object_t* read_atom(struct reader_t* reader);

static struct lusp_object_t* read_list(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '(');
	getchar(reader);
	
	struct lusp_object_t* result = 0;
	
	// read a list of atoms
	while ((skipws(reader), peekchar(reader)) != ')')
	{
		struct lusp_object_t* atom = read_atom(reader);
		
		result = lusp_mkcons(atom, result);
	}
	
	DL_ASSERT(peekchar(reader) == ')');
	getchar(reader);

	return reverse(result);
}

static struct lusp_object_t* read_quote(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '\'');
	getchar(reader);
	
	skipws(reader);
	
	struct lusp_object_t* atom = read_atom(reader);
	
	return lusp_mkcons(lusp_mksymbol("quote"), atom);
}

static struct lusp_object_t* read_atom(struct reader_t* reader)
{
	skipws(reader);
	
	switch (peekchar(reader))
	{
	case '(':
		return read_list(reader);
		
	case '\'':
		return read_quote(reader);
		
	default:
		return read_datum(reader);
	}
}

static struct lusp_object_t* read_program(struct reader_t* reader)
{
	struct lusp_object_t* program = read_atom(reader);
	
	skipws(reader);
	check(reader, peekchar(reader) == 0);
	
	return program;
}

struct lusp_object_t* lusp_read(const char* data)
{
	jmp_buf buf;
	
	if (setjmp(buf))
	{
		printf("error: read failed\n");
		return 0;
	}
	
	struct reader_t reader = {data, &buf};
	
	return read_program(&reader);
}