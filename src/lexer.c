// DeepLight Engine (c) Zeux 2006-2009

#include <core/common.h>

#include <lusp/lexer.h>

#include <core/string.h>

static inline bool is_whitespace(char data)
{
	return (unsigned char)(data - 1) < ' ';
}

static inline bool is_delimiter(char data)
{
	return is_whitespace(data) || data == '(' || data == ')' || data == '"' || data == ';';
}

static inline bool parse_string(struct lusp_lexer_t* lexer, const char* data)
{
	char* buffer = lexer->value;
	char* buffer_end = buffer + sizeof(lexer->value);
	
	// skip "
	DL_ASSERT(*data == '"');
	data++;
	
	// scan for closing quote
	while (*data && *data != '"')
	{
		if (*data == '\\')
		{
			if (buffer >= buffer_end || data[1] == 0) break;
			
			*buffer++ = data[1];
			data += 2;
		}
		else
		{
			if (buffer >= buffer_end) break;
			
			*buffer++ = *data++;
		}
	}
	
	// check for correct termination / buffer overflow
	if (buffer >= buffer_end || *data != '"')
	{
		lexer->data = data;
		
		return false;
	}
	else
	{
		lexer->data = data + 1;
		*buffer = 0;
		
		return true;
	}
} 

static inline bool parse_identifier(struct lusp_lexer_t* lexer, const char* data)
{
	// scan for delimiter
	const char* data_end = data;
	
	while (*data_end && !is_delimiter(*data_end)) ++data_end;
	
	// store identifier, if there is enough room
	size_t length = data_end - data;
	
	str_copy(lexer->value, min(sizeof(lexer->value), length + 1), data);
	
	// update state and return
	lexer->data = data_end;
	
	return length < sizeof(lexer->value);
}

static inline enum lusp_lexeme_t parse_comment(struct lusp_lexer_t* lexer, const char* data)
{
	// skip comment
	DL_ASSERT(*data == ';');
	
	while (*data && *data != '\n') ++data;
	
	// parse next token
	lexer->data = data;
	
	return lusp_lexer_next(lexer);
}

enum lusp_lexeme_t lusp_lexer_next(struct lusp_lexer_t* lexer)
{
	const char* data = lexer->data;
	enum lusp_lexeme_t lexeme = LUSP_LEX_UNKNOWN;
	
	// skip whitespaces
	while (is_whitespace(*data)) ++data;
	
	// parse symbol
	switch (*data)
	{
	case 0:
		// EOF
		break;
		
	case '(':
		lexeme = LUSP_LEX_OPEN_PARENS;
		lexer->data = data + 1;
		break;
		
	case ')':
		lexeme = LUSP_LEX_CLOSE_PARENS;
		lexer->data = data + 1;
		break;
		
	case '\'':
		lexeme = LUSP_LEX_APOSTROPHE;
		lexer->data = data + 1;
		break;
		
	case '`':
		lexeme = LUSP_LEX_BACKQUOTE;
		lexer->data = data + 1;
		break;
		
	case ',':
		lexeme = (data[1] == '@') ? LUSP_LEX_COMMA_AT : LUSP_LEX_COMMA;
		lexer->data = data + 1 + (lexeme == LUSP_LEX_COMMA_AT);
		break;
		
	case ';':
		lexeme = parse_comment(lexer, data);
		break;
		
	case '"':
		lexeme = parse_string(lexer, data) ? LUSP_LEX_STRING : LUSP_LEX_UNKNOWN;
		break;
		
	case '.':
		if (is_delimiter(data[1]))
		{
			lexeme = LUSP_LEX_DOT;
			lexer->data = data + 1;
			break;
		}
		
		// fallthrough
		
	default:
		lexeme = parse_identifier(lexer, data) ? LUSP_LEX_IDENTIFIER : LUSP_LEX_UNKNOWN;
	}
	
	lexer->lexeme = lexeme;
	
	return lexeme;
}

void lusp_lexer_init(struct lusp_lexer_t* lexer, const char* data)
{
	lexer->data = data;
	lexer->lexeme = LUSP_LEX_UNKNOWN;
	lexer->value[0] = 0;
	
	lusp_lexer_next(lexer);
}