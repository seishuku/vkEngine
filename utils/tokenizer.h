#ifndef __TOKENIZER_H__
#define __TOKENIZER_H__

#include <stdint.h>

typedef enum
{
	TOKEN_STRING=0,
	TOKEN_QUOTED,
	TOKEN_BOOLEAN,
	TOKEN_KEYWORD,
	TOKEN_FLOAT,
	TOKEN_INT,
	TOKEN_DELIMITER,
	NUM_TOKENTYPE
} TokenType_e;

typedef struct Token_s
{
	TokenType_e type;
	union
	{
		bool boolean;
		double fval;
		int64_t ival;
		char *string;
	};
} Token_t;

typedef struct
{
	size_t stringLength;
	size_t stringPosition;
	char *string;

	size_t numKeywords;
	const char **keywords;
} Tokenizer_t;

bool Tokenizer_Init(Tokenizer_t *context, size_t stringLength, char *string, size_t numKeywords, const char **keywords);
Token_t *Tokenizer_GetNext(Tokenizer_t *context);
Token_t *Tokenizer_PeekNext(Tokenizer_t *context);
void Tokenizer_PrintToken(const char *msg, const Token_t *token);
bool Tokenizer_ArgumentHelper(Tokenizer_t *tokenizer, char *fmt, ...);

#endif
