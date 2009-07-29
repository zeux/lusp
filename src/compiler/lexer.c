// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/compiler/lexer.h>

#include <core/string.h>

#include <math.h>

static inline bool is_whitespace(char data)
{
	return (unsigned char)(data - 1) < ' ';
}

static inline bool is_delimiter(char data)
{
	return is_whitespace(data) || data == '(' || data == ')' || data == '"' || data == ';' || data == 0;
}

static inline char tolower(char data)
{
	return (unsigned char)(data - 'A') < 26 ? data - 'A' + 'a' : data;
}

static inline bool is_digit(char data)
{
	return (unsigned char)(data - '0') < 10;
}

static inline void check(struct lusp_lexer_t* lexer, bool condition, const char* message)
{
	if (!condition) lexer->error_handler(lexer, message);
}

static inline char nextchar(struct lusp_lexer_t* lexer)
{
	return *++lexer->data;
}

static inline char peekchar(struct lusp_lexer_t* lexer)
{
	return *lexer->data;
}

static inline void skipws(struct lusp_lexer_t* lexer)
{
	// skip whitespace
	while (is_whitespace(peekchar(lexer)))
	{
		// count lines
		lexer->line += (peekchar(lexer) == '\n');
		
		nextchar(lexer);
	}
	
	// skip comment
	if (peekchar(lexer) == ';')
	{
		char ch;
		
		while ((ch = peekchar(lexer)) != 0 && ch != '\n') nextchar(lexer);
		
		// skip trailing ws
		skipws(lexer);
	}
}

static inline void parse_string(struct lusp_lexer_t* lexer)
{
	char* buffer = lexer->value.string;
	char* buffer_end = buffer + sizeof(lexer->value.string) - 1;

	// skip "
	DL_ASSERT(peekchar(lexer) == '"');

	// scan for closing quote
	char ch;
	
	while ((ch = nextchar(lexer)) != 0 && ch != '"')
	{
		check(lexer, buffer < buffer_end, "string is too long");
		
		// count lines
		lexer->line += (ch == '\n');
		
		// unescape
		if (ch == '\\')
		{
			ch = nextchar(lexer);
			check(lexer, ch != 0, "premature end of string");
		}
		
		*buffer++ = ch;
	}
	
	check(lexer, ch == '"', "premature end of string");
	nextchar(lexer);

	// this is safe since buffer is one byte larger than buffer_end tells us
	*buffer = 0;
} 

static inline void parse_symbol(struct lusp_lexer_t* lexer)
{
	char* buffer = lexer->value.symbol;
	char* buffer_end = buffer + sizeof(lexer->value.symbol) - 1;
	
	// scan for delimiter
	char ch = peekchar(lexer);
	check(lexer, !is_delimiter(ch), "symbol expected");

	do
	{
		check(lexer, buffer < buffer_end, "symbol is too long");
		
		*buffer++ = ch;
	}
	while (!is_delimiter(ch = nextchar(lexer)));
	
	// this is safe since buffer is one byte larger than buffer_end tells us
	*buffer = 0;
}

static inline int parse_sign(struct lusp_lexer_t* lexer)
{
	char ch = peekchar(lexer);
	
	if (ch == '+' || ch == '-') nextchar(lexer);
	
	return (ch == '-') ? -1 : 1;
}

static inline int parse_integer(struct lusp_lexer_t* lexer, int base, const char* message)
{
	int result = 0;
	int sign = parse_sign(lexer);
	
	char ch = peekchar(lexer);
	
	do
	{
		// handle both decimal and hexadecimal digits
		int digit = is_digit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
		
		check(lexer, digit >= 0 && digit < base, message);
		
		result = result * base + digit;
	}
	while (!is_delimiter(ch = nextchar(lexer)));
	
	return result * sign;
}

static inline float parse_real_exp(struct lusp_lexer_t* lexer, float base, int sign)
{
	int power = parse_integer(lexer, 10, "decimal digit expected");
	
	return sign * base * powf(10, (float)power);
}

static inline float parse_real(struct lusp_lexer_t* lexer, int integer, int sign)
{
	float fractional = 0;
	float power = 0.1f;
	
	for (char ch = peekchar(lexer); !is_delimiter(ch); ch = nextchar(lexer))
	{
		// read exponent part
		if (tolower(ch) == 'e')
		{
			nextchar(lexer);
			return parse_real_exp(lexer, integer + fractional, sign);
		}
		
		check(lexer, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		fractional = fractional + power * digit;
		power /= 10;
	}
	
	return (integer + fractional) * sign;
}

static inline enum lusp_lexeme_t parse_number(struct lusp_lexer_t* lexer, bool negative)
{
	int result = 0;
	
	// get sign, unless we have a forced negative
	int sign = negative ? -1 : parse_sign(lexer);
	
	char ch = peekchar(lexer);
	
	do
	{
		// read fractional part
		if (ch == '.')
		{
			nextchar(lexer);
			lexer->value.real = parse_real(lexer, result, sign);
			return LUSP_LEXEME_LITERAL_REAL;
		}
		
		// read exponent part
		if (tolower(ch) == 'e')
		{
			nextchar(lexer);
			lexer->value.real = parse_real_exp(lexer, (float)result, sign);
			return LUSP_LEXEME_LITERAL_REAL;
		}
		
		check(lexer, is_digit(ch), "decimal digit expected");
		
		int digit = ch - '0';
		
		result = result * 10 + digit;
	}
	while (!is_delimiter(ch = nextchar(lexer)));
	
	lexer->value.integer = sign * result;
	return LUSP_LEXEME_LITERAL_INTEGER;
}

static inline enum lusp_lexeme_t parse_sharp_literal(struct lusp_lexer_t* lexer)
{
	// skip #
	DL_ASSERT(peekchar(lexer) == '#');
	
	// parse literal
	switch (tolower(nextchar(lexer)))
	{
	case 't':
		nextchar(lexer);
		lexer->value.boolean = true;
		return LUSP_LEXEME_LITERAL_BOOLEAN;

	case 'f':
		nextchar(lexer);
		lexer->value.boolean = false;
		return LUSP_LEXEME_LITERAL_BOOLEAN;

	case 'd':
		nextchar(lexer);
		return parse_number(lexer, false);

	case 'b':
		nextchar(lexer);
		lexer->value.integer = parse_integer(lexer, 2, "binary digit expected");
		return LUSP_LEXEME_LITERAL_INTEGER;

	case 'o':
		nextchar(lexer);
		lexer->value.integer = parse_integer(lexer, 8, "octal digit expected");
		return LUSP_LEXEME_LITERAL_INTEGER;

	case 'x':
		nextchar(lexer);
		lexer->value.integer = parse_integer(lexer, 16, "hexadecimal digit expected");
		return LUSP_LEXEME_LITERAL_INTEGER;

	default:
		check(lexer, false, "wrong literal type");
		return LUSP_LEXEME_UNKNOWN;
	}
}

void lusp_lexer_init(struct lusp_lexer_t* lexer, const char* data, void* error_context, lusp_lexer_error_handler_t error_handler)
{
	lexer->lexeme = LUSP_LEXEME_UNKNOWN;
	
	lexer->data = data;
	lexer->line = 1;
	
	lexer->error_context = error_context;
	lexer->error_handler = error_handler;
	
	lusp_lexer_next(lexer);
}

enum lusp_lexeme_t lusp_lexer_next(struct lusp_lexer_t* lexer)
{
	// skip whitespaces
	skipws(lexer);
	
	// remember lexeme position ($$$)
	lexer->lexeme_data = lexer->data;
	lexer->lexeme_line = lexer->line;
	
	// parse symbol
	char ch = peekchar(lexer);
	
	switch (ch)
	{
	case 0:
		lexer->lexeme = LUSP_LEXEME_EOF;
		break;
		
	case '#':
		lexer->lexeme = parse_sharp_literal(lexer);
		break;
		
	case '+':
		if (is_delimiter(nextchar(lexer)))
		{
			str_copy(lexer->value.symbol, sizeof(lexer->value.symbol), "+");
			lexer->lexeme = LUSP_LEXEME_SYMBOL;
		}
		else lexer->lexeme = parse_number(lexer, false);
		break;
		
	case '-':
		if (is_delimiter(nextchar(lexer)))
		{
			str_copy(lexer->value.symbol, sizeof(lexer->value.symbol), "-");
			lexer->lexeme = LUSP_LEXEME_SYMBOL;
		}
		else lexer->lexeme = parse_number(lexer, true);
		break;
		
	case '.':
		if (!is_delimiter(nextchar(lexer)))
		{
			lexer->value.real = parse_real(lexer, 0, 1);
			lexer->lexeme = LUSP_LEXEME_LITERAL_REAL;
		}
		else lexer->lexeme = LUSP_LEXEME_DOT;
		break;
		
	case '"':
		parse_string(lexer);
		lexer->lexeme = LUSP_LEXEME_LITERAL_STRING;
		break;
		
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		lexer->lexeme = parse_number(lexer, false);
		break;
		
	case '(':
		nextchar(lexer);
		lexer->lexeme = LUSP_LEXEME_OPEN_BRACE;
		break;
		
	case ')':
		nextchar(lexer);
		lexer->lexeme = LUSP_LEXEME_CLOSE_BRACE;
		break;
		
	case '\'':
		nextchar(lexer);
		lexer->lexeme = LUSP_LEXEME_QUOTE;
		break;
		
	case '`':
		nextchar(lexer);
		lexer->lexeme = LUSP_LEXEME_BACKQUOTE;
		break;
		
	case ',':
		if (nextchar(lexer) == '@')
		{
			nextchar(lexer);
			lexer->lexeme = LUSP_LEXEME_COMMA_AT;
		}
		else lexer->lexeme = LUSP_LEXEME_COMMA;
		break;
		
	default:
		parse_symbol(lexer);
		lexer->lexeme = LUSP_LEXEME_SYMBOL;
	}
	
	return lexer->lexeme;
}
