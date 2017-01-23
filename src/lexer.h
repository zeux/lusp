#pragma once

#include <stdbool.h>

enum lusp_lexeme_t
{
	LUSP_LEXEME_UNKNOWN,
	LUSP_LEXEME_EOF,
	LUSP_LEXEME_LITERAL_BOOLEAN,
	LUSP_LEXEME_LITERAL_INTEGER,
	LUSP_LEXEME_LITERAL_REAL,
	LUSP_LEXEME_LITERAL_STRING,
	LUSP_LEXEME_OPEN_PAREN,
	LUSP_LEXEME_CLOSE_PAREN,
	LUSP_LEXEME_OPEN_BRACE,
	LUSP_LEXEME_CLOSE_BRACE,
	LUSP_LEXEME_COMMA,
	LUSP_LEXEME_DOT,
	LUSP_LEXEME_ASSIGN,
	LUSP_LEXEME_VERTICAL_BAR,
	LUSP_LEXEME_ADD,
	LUSP_LEXEME_SUBTRACT,
	LUSP_LEXEME_MULTIPLY,
	LUSP_LEXEME_DIVIDE,
	LUSP_LEXEME_MODULO,
	LUSP_LEXEME_GREATER,
	LUSP_LEXEME_GREATER_EQUAL,
	LUSP_LEXEME_LESS,
	LUSP_LEXEME_LESS_EQUAL,
	LUSP_LEXEME_EQUAL,
	LUSP_LEXEME_NOT_EQUAL,
	LUSP_LEXEME_SYMBOL,
	LUSP_LEXEME_SYMBOL_LET,
	LUSP_LEXEME_SYMBOL_IF,
	LUSP_LEXEME_SYMBOL_ELSE,
};

union lusp_lexeme_value_t {
	bool boolean;
	int integer;
	float real;
	char string[1024];
	char symbol[1024];
};

struct lusp_lexer_t;

typedef void (*lusp_lexer_error_handler_t)(struct lusp_lexer_t* lexer, const char* message, ...);

struct lusp_lexer_t
{
	// current lexeme
	enum lusp_lexeme_t lexeme;
	union lusp_lexeme_value_t value;

	// lexeme information
	const char* lexeme_data;
	unsigned int lexeme_line;

	// stream information
	const char* data;
	unsigned int line;

	// error handler
	void* error_context;
	lusp_lexer_error_handler_t error_handler;
};

void lusp_lexer_init(struct lusp_lexer_t* lexer, const char* data, void* error_context, lusp_lexer_error_handler_t error_handler);
enum lusp_lexeme_t lusp_lexer_next(struct lusp_lexer_t* lexer);
