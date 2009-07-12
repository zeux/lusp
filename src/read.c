// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/read.h>

#include <lusp/object.h>

#include <core/string.h>

#include <setjmp.h>
#include <math.h>

#include <stdio.h>

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

static inline int read_sign(struct reader_t* reader)
{
	char ch = peekchar(reader);
	
	if (ch == '+' || ch == '-') nextchar(reader);
	
	return (ch == '-') ? -1 : 1;
}

static inline int read_integer_value(struct reader_t* reader, int base, const char* message)
{
	int result = 0;
	int sign = read_sign(reader);
	
	char ch = peekchar(reader);
	
	do
	{
		int digit = is_digit(ch) ? ch - '0' : tolower(ch) - 'a';
		
		check(reader, digit >= 0 && digit < base, message);
		
		result = result * base + digit;
	}
	while ((ch = nextchar(reader)) != 0 && !is_delimiter(ch));
	
	return result * sign;
}

static struct lusp_object_t* read_integer(struct reader_t* reader, int base, const char* message)
{
	return lusp_mkinteger(read_integer_value(reader, base, message));
}

static inline struct lusp_object_t* read_real_exp(struct reader_t* reader, float base, int sign)
{
	int power = read_integer_value(reader, 10, "decimal digit expected");
	
	return lusp_mkreal(sign * base * powf(10, (float)power));
}

static inline struct lusp_object_t* read_real(struct reader_t* reader, int integer, int sign)
{
	float fractional = 0;
	float power = 0.1f;
	
	for (char ch = peekchar(reader); ch != 0 && !is_delimiter(ch); ch = nextchar(reader))
	{
		if (tolower(ch) == 'e')
		{
			nextchar(reader);
			return read_real_exp(reader, integer + fractional, sign);
		}
		
		check(reader, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		fractional = fractional + power * digit;
		power /= 10;
	}
	
	return lusp_mkreal((integer + fractional) * sign);
}

static struct lusp_object_t* read_number(struct reader_t* reader, bool negative)
{
	int result = 0;
	int sign = negative ? -1 : read_sign(reader);
	
	char ch = peekchar(reader);
	
	do
	{
		if (ch == '.')
		{
			nextchar(reader);
			return read_real(reader, result, sign);
		}
		
		if (tolower(ch) == 'e')
		{
			nextchar(reader);
			return read_real_exp(reader, (float)result, sign);
		}
		
		check(reader, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		result = result * 10 + digit;
	}
	while ((ch = nextchar(reader)) != 0 && !is_delimiter(ch));
	
	return lusp_mkinteger(sign * result);
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
			return read_number(reader, false);
			
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
	else if (ch == '+')
	{
		ch = nextchar(reader);
		
		return (is_delimiter(ch) || ch == 0) ? lusp_mksymbol("+") : read_number(reader, false);
	}
	else if (ch == '-')
	{
		ch = nextchar(reader);
		
		return (is_delimiter(ch) || ch == 0) ? lusp_mksymbol("-") : read_number(reader, true);
	}
	else if (ch == '.')
	{
		ch = nextchar(reader);
		
		return (is_delimiter(ch) || ch == 0) ? lusp_mksymbol(".") : read_real(reader, 0, 1);
	}
	else if (ch == '"')
		return read_string(reader);
	else if (is_digit(ch))
		return read_number(reader, false);
	else
		return read_symbol(reader);
}

static struct lusp_object_t* read_atom(struct reader_t* reader);

static struct lusp_object_t* read_list(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '(');
	nextchar(reader);
	
	struct lusp_object_t* head = 0;
	struct lusp_object_t* tail = 0;
	
	// read a list of atoms
	while ((skipws(reader), peekchar(reader)) != ')')
	{
		struct lusp_object_t* atom = read_atom(reader);
		
		if (tail) tail = tail->cons.cdr = lusp_mkcons(atom, 0);
		else head = tail = lusp_mkcons(atom, 0);
	}
	
	DL_ASSERT(peekchar(reader) == ')');
	nextchar(reader);

	return head;
}

static struct lusp_object_t* read_quote(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '\'');
	nextchar(reader);
	
	skipws(reader);
	
	struct lusp_object_t* atom = read_atom(reader);
	
	return lusp_mkcons(lusp_mksymbol("quote"), atom);
}

static struct lusp_object_t* read_quasiquote(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == '`');
	nextchar(reader);
	
	skipws(reader);
	
	struct lusp_object_t* atom = read_atom(reader);
	
	return lusp_mkcons(lusp_mksymbol("quasiquote"), atom);
}

static struct lusp_object_t* read_unquote(struct reader_t* reader)
{
	DL_ASSERT(peekchar(reader) == ',');
	char ch = nextchar(reader);
	
	// handle unquote-splicing
	if (ch == '@') nextchar(reader);
	
	skipws(reader);
	
	struct lusp_object_t* atom = read_atom(reader);
	
	return lusp_mkcons(lusp_mksymbol(ch == '@' ? "unquote-splicing" : "unquote"), atom);
}

static struct lusp_object_t* read_atom(struct reader_t* reader)
{
	switch (peekchar(reader))
	{
	case '(':
		return read_list(reader);
		
	case '\'':
		return read_quote(reader);
		
	case '`':
		return read_quasiquote(reader);
		
	case ',':
		return read_unquote(reader);
		
	default:
		return read_datum(reader);
	}
}

static struct lusp_object_t* read_program(struct reader_t* reader)
{
	struct lusp_object_t* program = read_atom(reader);
	
	skipws(reader);
	
	return program;
}

bool lusp_read_ex(const char* data, const char** out_data, struct lusp_object_t** out_result)
{
	jmp_buf buf;
	volatile struct reader_t reader = {data, &buf, ""};
	
	if (setjmp(buf))
	{
		printf("error: read failed (%s)\n", reader.message);
		return false;
	}
	
	struct lusp_object_t* result = read_program((struct reader_t*)&reader);
	
	*out_data = reader.data;
	*out_result = result;
	
	return true;
}

struct lusp_object_t* lusp_read(const char* data)
{
	struct lusp_object_t* result;
	
	if (!lusp_read_ex(data, &data, &result)) return 0;
	
	if (*data)
	{
		printf("error: read failed (unexpected data)\n");
		return 0;
	}
	
	return result;
}