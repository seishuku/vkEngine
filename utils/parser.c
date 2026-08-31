#include <string.h>

#include "../system/system.h"
#include "parser.h"

void Parser_Init(Parser_t *parser, Tokenizer_t *tokenizer)
{
	parser->tokenizer=tokenizer;
}

Token_t *Parser_Peek(Parser_t *parser)
{
	return Tokenizer_PeekNext(parser->tokenizer);
}

bool Parser_Is(Parser_t *parser, TokenType_e type, char delimiter)
{
	Token_t *token=Parser_Peek(parser);
	bool result=false;

	if(token)
	{
		result=token->type==type&&token->string[0]==delimiter;
		Zone_Free(zone, token);
	}

	return result;
}

bool Parser_IsKeyword(Parser_t *parser, const char *keyword)
{
	Token_t *token=Parser_Peek(parser);
	bool result=false;

	if(token)
	{
		result=token->type==TOKEN_KEYWORD&&strcmp(token->string, keyword)==0;
		Zone_Free(zone, token);
	}

	return result;
}

bool Parser_Expect(Parser_t *parser, TokenType_e type, char delimiter)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected '%c'.\n", delimiter);
		return false;
	}

	if(token->type!=type||token->string[0]!=delimiter)
	{
		Tokenizer_PrintToken("Unexpected token,", token);
		Zone_Free(zone, token);
		return false;
	}

	Zone_Free(zone, token);
	return true;
}

bool Parser_ExpectKeyword(Parser_t *parser, const char *keyword)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected '%s'.\n", keyword);
		return false;
	}

	if(token->type!=TOKEN_KEYWORD||strcmp(token->string, keyword)!=0)
	{
		Tokenizer_PrintToken("Unexpected token,", token);
		Zone_Free(zone, token);
		return false;
	}

	Zone_Free(zone, token);
	return true;
}

bool Parser_Match(Parser_t *parser, TokenType_e type, char delimiter)
{
	Token_t *token=Parser_Peek(parser);
	bool matched=false;

	if(token)
	{
		matched=token->type==type&&token->string[0]==delimiter;
		Zone_Free(zone, token);
	}

	if(matched)
	{
		Token_t *consumed=Tokenizer_GetNext(parser->tokenizer);
		Zone_Free(zone, consumed);
	}

	return matched;
}

bool Parser_MatchKeyword(Parser_t *parser, const char *keyword)
{
	Token_t *token=Parser_Peek(parser);
	bool matched=false;

	if(token)
	{
		matched=token->type==TOKEN_KEYWORD&&strcmp(token->string, keyword)==0;
		Zone_Free(zone, token);
	}

	if(matched)
	{
		Token_t *consumed=Tokenizer_GetNext(parser->tokenizer);
		Zone_Free(zone, consumed);
	}

	return matched;
}

bool Parser_IsEnd(Parser_t *parser)
{
	Token_t *token=Parser_Peek(parser);
	bool atEnd=(token==NULL);

	if(token)
		Zone_Free(zone, token);

	return atEnd;
}

bool Parser_Unexpected(Parser_t *parser, const char *message)
{
	Token_t *token=Parser_Peek(parser);

	if(token)
	{
		Tokenizer_PrintToken(message, token);
		Zone_Free(zone, token);
	}
	else
		DBGPRINTF(DEBUG_ERROR, "%sunexpected end of file.\n", message);

	return false;
}

bool Parser_Integer(Parser_t *parser, int64_t *value)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected integer.\n");
		return false;
	}

	if(token->type!=TOKEN_INT)
	{
		Tokenizer_PrintToken("Unexpected token, expected integer, got", token);
		Zone_Free(zone, token);
		return false;
	}

	*value=token->ival;

	Zone_Free(zone, token);
	return true;
}

bool Parser_Float(Parser_t *parser, double *value)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected float.\n");
		return false;
	}

	if(token->type!=TOKEN_FLOAT)
	{
		Tokenizer_PrintToken("Unexpected token, expected float, got", token);
		Zone_Free(zone, token);
		return false;
	}

	*value=token->fval;

	Zone_Free(zone, token);
	return true;
}

bool Parser_Boolean(Parser_t *parser, bool *value)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected boolean.\n");
		return false;
	}

	if(token->type!=TOKEN_BOOLEAN)
	{
		Tokenizer_PrintToken("Unexpected token, expected boolean, got", token);
		Zone_Free(zone, token);
		return false;
	}

	*value=token->boolean;

	Zone_Free(zone, token);
	return true;
}

bool Parser_String(Parser_t *parser, char *buffer, size_t bufferSize)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected string.\n");
		return false;
	}

	if(token->type!=TOKEN_STRING)
	{
		Tokenizer_PrintToken("Unexpected token, expected string, got", token);
		Zone_Free(zone, token);
		return false;
	}

	strncpy(buffer, token->string, bufferSize-1);
	buffer[bufferSize-1]='\0';

	Zone_Free(zone, token);
	return true;
}

bool Parser_Quoted(Parser_t *parser, char *buffer, size_t bufferSize)
{
	Token_t *token=Tokenizer_GetNext(parser->tokenizer);

	if(token==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unexpected end of file, expected string.\n");
		return false;
	}

	if(token->type!=TOKEN_STRING)
	{
		Tokenizer_PrintToken("Unexpected token, expected string, got", token);
		Zone_Free(zone, token);
		return false;
	}

	strncpy(buffer, token->string, bufferSize-1);
	buffer[bufferSize-1]='\0';

	Zone_Free(zone, token);
	return true;
}
