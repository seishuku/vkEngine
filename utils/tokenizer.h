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
	TOKEN_UNKNOWN,
	TOKEN_END,
	NUM_TOKENTYPE
} TokenType_e;

#define MAX_TOKEN_STRING_LENGTH 1024

typedef struct Token_s
{
	TokenType_e type;
	union
	{
		bool boolean;
		double fval;
		int64_t ival;
		char string[MAX_TOKEN_STRING_LENGTH+1];
	};
} Token_t;

typedef struct
{
	char *string;

	size_t numKeywords;
	const char **keywords;
} Tokenizer_t;

bool Tokenizer_Init(Tokenizer_t *context, char *string, size_t numKeywords, const char **keywords);
Token_t Tokenizer_GetNext(Tokenizer_t *context);
Token_t Tokenizer_PeekNext(Tokenizer_t *context);

#endif
