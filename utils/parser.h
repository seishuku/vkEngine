#ifndef __PARSER_H__
#define __PARSER_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "tokenizer.h"

typedef struct
{
	Tokenizer_t *tokenizer;
} Parser_t;

void Parser_Init(Parser_t *parser, Tokenizer_t *tokenizer);

Token_t *Parser_Peek(Parser_t *parser);

bool Parser_Is(Parser_t *parser, TokenType_e type, char delimiter);
bool Parser_IsKeyword(Parser_t *parser, const char *keyword);

bool Parser_Expect(Parser_t *parser, TokenType_e type, char delimiter);
bool Parser_ExpectKeyword(Parser_t *parser, const char *keyword);

bool Parser_Match(Parser_t *parser, TokenType_e type, char delimiter);
bool Parser_MatchKeyword(Parser_t *parser, const char *keyword);

bool Parser_IsEnd(Parser_t *parser);

bool Parser_Integer(Parser_t *parser, int64_t *value);
bool Parser_Float(Parser_t *parser, double *value);
bool Parser_Boolean(Parser_t *parser, bool *value);
bool Parser_String(Parser_t *parser, char *buffer, size_t bufferSize);
bool Parser_Quoted(Parser_t *parser, char *buffer, size_t bufferSize);

#endif
