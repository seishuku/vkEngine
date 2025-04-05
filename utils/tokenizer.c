#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../system/system.h"
#include "tokenizer.h"

bool Tokenizer_Init(Tokenizer_t *context, size_t stringLength, char *string, size_t numKeywords, const char **keywords)
{
	context->numKeywords=numKeywords;
	context->keywords=keywords;

	context->stringLength=stringLength;
	context->stringPosition=0;
	context->string=string;

	return true;
}

static const char delimiters[]="\'\"{}[]()<>!@#$%^&*-+=,.:;\\/|`~ \t\n\r";
static const char *booleans[]={ "false", "true" };

static bool IsAlpha(const char c)
{
	if((c>='A'&&c<='Z')||(c>='a'&&c<='z'))
		return true;

	return false;
}

static bool IsDigit(const char c)
{
	if(c>='0'&&c<='9')
		return true;

	return false;
}

static bool IsDelimiter(const char c)
{
	for(uint32_t i=0;i<sizeof(delimiters)-1;i++)
	{
		if(c==delimiters[i])
			return true;
	}

	return false;
}

static bool IsWord(const char *word, const char *stringArray[], size_t arraySize)
{
	for(size_t i=0;i<arraySize;i++)
	{
		if(strcmp(word, stringArray[i])==0)
			return true;
	}

	return false;
}

static char GetChar(Tokenizer_t *context, size_t offset)
{
	if((context->stringPosition+offset)>=context->stringLength)
		return 0;

	return context->string[context->stringPosition+offset];
}

static void SkipWhitespace(Tokenizer_t *context)
{
	char c=GetChar(context, 0);

	while(c==' '||c=='\t'||c=='\n'||c=='\r')
	{
		context->stringPosition++;
		c=GetChar(context, 0);
	}
}

// Pull a token from string stream, classify it and advance the string pointer.
// Token must be freed.
Token_t *Tokenizer_GetNext(Tokenizer_t *context)
{
	Token_t *token=NULL;

	SkipWhitespace(context);

	// Handle comments
	bool commentDone=true, singleLine=false;

	if(GetChar(context, 0)=='/'&&GetChar(context, 1)=='*')
	{
		singleLine=false;
		commentDone=false;
	}
	else if((GetChar(context, 0)=='/'&&GetChar(context, 1)=='/')||GetChar(context, 0)=='#')
	{
		singleLine=true;
		commentDone=false;
	}

	while(!commentDone)
	{
		while(GetChar(context, 0), context->stringPosition++)
		{
			if(singleLine)
			{
				if(GetChar(context, 0)=='\n'||GetChar(context, 0)=='\r')
				{
					SkipWhitespace(context);

					// Check if there are more comments and switch modes if needed
					if((GetChar(context, 0)=='/'&&GetChar(context, 1)=='/')||GetChar(context, 0)=='#')
						commentDone=false;
					else if(GetChar(context, 0)=='/'&&GetChar(context, 1)=='*')
					{
						commentDone=false;
						singleLine=false;
					}
					else
						commentDone=true;
					break;
				}
			}
			else
			{
				if(GetChar(context, 0)=='*'&&GetChar(context, 1)=='/')
				{
					// Eat the remaining '*' and '/'
					context->stringPosition+=2;

					SkipWhitespace(context);

					if((GetChar(context, 0)=='/'&&GetChar(context, 1)=='/')||GetChar(context, 0)=='#')
					{
						commentDone=false;
						singleLine=true;
					}
					else if(GetChar(context, 0)=='/'&&GetChar(context, 1)=='*')
						commentDone=false;
					else
						commentDone=true;
					break;
				}
			}
		}
	}

	// Process the token
	if(IsAlpha(GetChar(context, 0))) // Strings and special classification
	{
		uint32_t count=0;

		while(!IsDelimiter(GetChar(context, count))&&GetChar(context, count)!='\0')
			count++;

		token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t)+count+1);

		if(token==NULL)
			return NULL;

		token->type=TOKEN_STRING;
		token->string=(char *)(token+1);

		memcpy(token->string, &context->string[context->stringPosition], count);
		token->string[count]='\0';

		context->stringPosition+=count;

		// Re-classify the string if it matches any of these:
		if(IsWord(token->string, booleans, sizeof(booleans)/sizeof(booleans[0])))
		{
			token->type=TOKEN_BOOLEAN;

			if(strcmp(token->string, "true")==0)
				token->boolean=true;
			else
				token->boolean=false;
		}

		if(IsWord(token->string, context->keywords, context->numKeywords))
			token->type=TOKEN_KEYWORD;
	}
	else if(GetChar(context, 0)=='0'&&(GetChar(context, 1)=='x'||GetChar(context, 1)=='X')) // Hexidecimal numbers
	{
		const char *start=context->string+context->stringPosition+2;
		char *end=NULL;

		token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t));

		if(token==NULL)
			return NULL;

		token->type=TOKEN_INT;
		token->ival=strtoll(start, &end, 16);

		context->stringPosition+=end-start;
	}
	else if(GetChar(context, 0)=='0'&&(GetChar(context, 1)=='b'||GetChar(context, 1)=='B')) // Binary numbers
	{
		const char *start=context->string+context->stringPosition+2;
		char *end=NULL;

		token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t));

		if(token==NULL)
			return NULL;

		token->type=TOKEN_INT;
		token->ival=strtoll(start, &end, 2);

		context->stringPosition+=end-start;
	}
	else if((GetChar(context, 0)=='-'&&IsDigit(GetChar(context, 1)))||IsDigit(GetChar(context, 0))) // Whole numbers and floating point
	{
		const char *start=context->string+context->stringPosition;
		char *end=NULL;
		size_t count=0;

		token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t));

		if(token==NULL)
			return NULL;

		while(!IsDelimiter(GetChar(context, count)))
		{
			if(GetChar(context, count)=='\0')
				break;

			count++;
		}

		if(GetChar(context, count)=='.')
		{
			token->type=TOKEN_FLOAT;
			token->fval=strtod(start, &end);
		}
		else
		{
			token->type=TOKEN_INT;
			token->ival=strtoll(start, &end, 10);
		}

		context->stringPosition+=end-start;
	}
	else if(IsDelimiter(GetChar(context, 0)))
	{
		// special case, treat quoted items as special whole string tokens
		if(GetChar(context, 0)=='\"')
		{
			context->stringPosition++;

			uint32_t count=0;

			while(GetChar(context, count)!='\"'&&GetChar(context, count)!='\0')
				count++;

			token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t)+count+1);

			if(token==NULL)
				return NULL;

			token->type=TOKEN_QUOTED;
			token->string=(char *)(token+1);

			memcpy(token->string, &context->string[context->stringPosition], count);
			token->string[count]='\0';

			context->stringPosition+=count+1;
		}
		else
		{
			token=(Token_t *)Zone_Malloc(zone, sizeof(Token_t));

			if(token==NULL)
				return NULL;

			token->type=TOKEN_DELIMITER;
			token->string=((char *)token)+sizeof(TokenType_e);
			token->string[0]=GetChar(context, 0);
			token->string[1]='\0';

			context->stringPosition++;
		}
	}
	else
		context->stringPosition++;

	return token;
}

// Same as Token_GetNext, but does not advance string pointer
// Token must be freed.
Token_t *Tokenizer_PeekNext(Tokenizer_t *context)
{
	size_t startPosition=context->stringPosition;
	Token_t *result=Tokenizer_GetNext(context);
	context->stringPosition=startPosition;

	return result;
}

void Tokenizer_PrintToken(const char *msg, const Token_t *token)
{
	if(token==NULL)
		DBGPRINTF(DEBUG_ERROR, "End token");
	else if(token->type==TOKEN_STRING)
		DBGPRINTF(DEBUG_ERROR, "%s string: %s\n", msg, token->string);
	else if(token->type==TOKEN_QUOTED)
		DBGPRINTF(DEBUG_ERROR, "%s quoted string: %s\n", msg, token->string);
	else if(token->type==TOKEN_BOOLEAN)
		DBGPRINTF(DEBUG_ERROR, "%s boolean string: %s\n", msg, token->string);
	else if(token->type==TOKEN_KEYWORD)
		DBGPRINTF(DEBUG_ERROR, "%s keyword string: %s\n", msg, token->string);
	else if(token->type==TOKEN_FLOAT)
		DBGPRINTF(DEBUG_ERROR, "%s floating point number: %lf\n", msg, token->fval);
	else if(token->type==TOKEN_INT)
		DBGPRINTF(DEBUG_ERROR, "%s integer number: %lld\n", msg, token->ival);
	else if(token->type==TOKEN_DELIMITER)
		DBGPRINTF(DEBUG_ERROR, "%s delimiter: %c\n", msg, token->string[0]);
}

bool Tokenizer_ArgumentHelper(Tokenizer_t *tokenizer, char *fmt, ...)
{
	char *pFmt=fmt;
	va_list ap;

	va_start(ap, fmt);

	// First token should be a left parenthesis '('
	Token_t *token=Tokenizer_GetNext(tokenizer);

	if(token->type!=TOKEN_DELIMITER&&token->string[0]!='(')
	{
		Tokenizer_PrintToken("Unexpected token ", token);
		return false;
	}
	else
	{
		// Loop until right parenthesis ')' or until break condition
		while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
		{
			Zone_Free(zone, token);
			token=Tokenizer_GetNext(tokenizer);

			void *arg=va_arg(ap, void *);

			if(arg==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "Got NULL argument pointer.\n");
				return false;
			}

			switch(token->type)
			{
				case TOKEN_STRING:
				case TOKEN_QUOTED:
				{
					if(*pFmt=='s')
					{
						pFmt++;

						int32_t maxLength=strtol(pFmt, &pFmt, 10);

						if(maxLength>0)
						{
							strncpy((char *)arg, token->string, maxLength);
							((char *)arg)[maxLength-1]='\0';
						}
					}
					else
						DBGPRINTF(DEBUG_ERROR, "Type mismatch!\n");
					break;
				}

				case TOKEN_BOOLEAN:
				{
					if(*pFmt=='b')
					{
						pFmt++;
						*(bool *)arg=token->boolean;
					}
					else
						DBGPRINTF(DEBUG_ERROR, "Type mismatch!\n");
					break;
				}

				case TOKEN_FLOAT:
				{
					if(*pFmt=='f')
					{
						pFmt++;
						*(float *)arg=(float)token->fval;
					}
					else
						DBGPRINTF(DEBUG_ERROR, "Type mismatch!\n");
					break;
				}

				case TOKEN_INT:
				{
					if(*pFmt=='i')
					{
						pFmt++;
						*(uint32_t *)arg=(uint32_t)token->ival;
					}
					else
						DBGPRINTF(DEBUG_ERROR, "Type mismatch!\n");
					break;
				}

				default:
					Tokenizer_PrintToken("Unexpected token ", token);
					return false;
			}

			Zone_Free(zone, token);
			token=Tokenizer_GetNext(tokenizer);

			if(token->type==TOKEN_DELIMITER)
			{
				if(token->string[0]!=','&&*pFmt!='\0')
				{
					DBGPRINTF(DEBUG_ERROR, "Too few arguments, missing comma.\n");
					return false;
				}
				else if(token->string[0]==','&&*pFmt=='\0')
				{
					DBGPRINTF(DEBUG_ERROR, "Too many arguments.\n");
					return false;
				}
				else if(token->string[0]==')'&&*pFmt=='\0')
					break;
			}
		}
	}

	va_end(ap);
	Zone_Free(zone, token);

	return true;
}
