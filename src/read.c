// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/read.h>

#include <lusp/object.h>

#include <core/string.h>

#include <setjmp.h>
#include <math.h>

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
	const char* message;
};

static inline bool is_whitespace(char data)
{
	return (unsigned char)(data - 1) < ' ';
}

static inline bool is_delimiter(char data)
{
	return is_whitespace(data) || data == '(' || data == ')' || data == '"' || data == ';';
}

static inline char tolower(char data)
{
	return (unsigned char)(data - 'A') < 26 ? data - 'A' : data;
}

static inline bool is_digit(char data)
{
	return (unsigned char)(data - '0') < 10;
}

static inline char nextchar(struct reader_t* reader)
{
	return *++reader->data;
}

static inline char peekchar(struct reader_t* reader)
{
	return *reader->data;
}

static inline void check(struct reader_t* reader, bool condition, const char* message)
{
	if (!condition)
	{
		reader->message = message;
		
		longjmp(*reader->error, 1);
	}
}

static inline void skipws(struct reader_t* reader)
{
	// skip whitespace
	while (is_whitespace(peekchar(reader))) nextchar(reader);
	
	// skip comment
	if (peekchar(reader) == ';')
	{
		char ch;
		
		while ((ch = peekchar(reader)) != 0 && ch != '\n') nextchar(reader);
		
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
	DL_ASSERT(peekchar(reader) == '"');

	// scan for closing quote
	char ch;
	
	while ((ch = nextchar(reader)) != 0 && ch != '"')
	{
		check(reader, buffer < buffer_end, "string is too long");
		
		if (ch == '\\')
		{
			ch = nextchar(reader);
			check(reader, ch != 0, "premature end of string");
		}
		
		*buffer++ = ch;
	}
	
	check(reader, ch == '"', "premature end of string");
	nextchar(reader);

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
	char ch = peekchar(reader);
	check(reader, ch != 0 && !is_delimiter(ch), "symbol expected");

	do
	{
		check(reader, buffer < buffer_end, "symbol is too long");
		
		*buffer++ = ch;
	}
	while ((ch = nextchar(reader)) != 0 && !is_delimiter(ch));
	
	// this is safe since buffer is one byte larger than buffer_end tells us
	*buffer = 0;
	
	return lusp_mksymbol(value);
}

static struct lusp_object_t* read_integer(struct reader_t* reader, int base, const char* message)
{
	int result = 0;
	
	char ch = peekchar(reader);
	
	do
	{
		int digit = is_digit(ch) ? ch - '0' : tolower(ch) - 'a';
		
		check(reader, digit >= 0 && digit < base, message);
		
		result = result * base + digit;
	}
	while ((ch = nextchar(reader)) != 0 && !is_delimiter(ch));
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_real(struct reader_t* reader, int integer)
{
	float fractional = 0;
	float power = 0.1f;
	
	for (char ch = peekchar(reader); ch != 0 && !is_delimiter(ch); ch = nextchar(reader))
	{
		check(reader, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		fractional = fractional + power * digit;
		power /= 10;
	}
	
	return lusp_mkreal(integer + fractional);
}

static struct lusp_object_t* read_number(struct reader_t* reader)
{
	int result = 0;
	
	char ch = peekchar(reader);
	
	do
	{
		if (ch == '.')
		{
			nextchar(reader);
			return read_real(reader, result);
		}
		
		check(reader, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		result = result * 10 + digit;
	}
	while ((ch = nextchar(reader)) != 0 && !is_delimiter(ch));
	
	return lusp_mkinteger(result);
}

static struct lusp_object_t* read_datum(struct reader_t* reader)
{
	char ch = peekchar(reader);
	
	if (ch == '#')
	{
		switch (tolower(nextchar(reader)))
		{
		case 't':
			nextchar(reader);
			return lusp_mkboolean(true);
			
		case 'f':
			nextchar(reader);
			return lusp_mkboolean(false);
			
		case 'd':
			nextchar(reader);
			return read_number(reader);
			
		case 'b':
			nextchar(reader);
			return read_integer(reader, 2, "binary digit expected");
			
		case 'o':
			nextchar(reader);
			return read_integer(reader, 8, "octal digit expected");
			
		case 'x':
			nextchar(reader);
			return read_integer(reader, 16, "hexadecimal digit expected");
			
		default:
			check(reader, false, "wrong literal type");
			return 0;
		}
	}
	else if (ch == '.')
	{
		ch = nextchar(reader);
		
		return (is_delimiter(ch) || ch == 0) ? lusp_mksymbol(".") : read_real(reader, 0);
	}
	else if (ch == '"')
		return read_string(reader);
	else if (is_digit(ch))
		return read_number(reader);
	else
		return read_symbol(reader);
}

static struct lusp_object_t* read_atom(struct reader_t* reader);

static struct lusp_object_t* read_list(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '(');
	nextchar(reader);
	
	struct lusp_object_t* result = 0;
	
	// read a list of atoms
	while ((skipws(reader), peekchar(reader)) != ')')
	{
		struct lusp_object_t* atom = read_atom(reader);
		
		result = lusp_mkcons(atom, result);
	}
	
	DL_ASSERT(peekchar(reader) == ')');
	nextchar(reader);

	return reverse(result);
}

static struct lusp_object_t* read_quote(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '\'');
	nextchar(reader);
	
	skipws(reader);
	
	struct lusp_object_t* atom = read_atom(reader);
	
	return lusp_mkcons(lusp_mksymbol("quote"), atom);
}

static struct lusp_object_t* read_atom(struct reader_t* reader)
{
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
	check(reader, peekchar(reader) == 0, "unexpected data");
	
	return program;
}

struct lusp_object_t* lusp_read(const char* data)
{
	jmp_buf buf;
	volatile struct reader_t reader = {data, &buf, ""};
	
	if (setjmp(buf))
	{
		printf("error: read failed (%s)\n", reader.message);
		return 0;
	}
	
	return read_program((struct reader_t*)&reader);
}