// DeepLight Engine (c) Zeux 2006-2009

#pragma once

enum lusp_lexeme_t
{
	LUSP_LEX_UNKNOWN,
	LUSP_LEX_OPEN_PARENS,
	LUSP_LEX_CLOSE_PARENS,
	LUSP_LEX_APOSTROPHE,
	LUSP_LEX_BACKQUOTE,
	LUSP_LEX_COMMA,
	LUSP_LEX_COMMA_AT,
	LUSP_LEX_DOT,
	LUSP_LEX_IDENTIFIER,
	LUSP_LEX_STRING
};

struct lusp_lexer_t
{
	const char* data;
	enum lusp_lexeme_t lexeme;
	
	char value[2048];
};

void lusp_lexer_init(struct lusp_lexer_t* lexer, const char* data);
enum lusp_lexeme_t lusp_lexer_next(struct lusp_lexer_t* lexer);
