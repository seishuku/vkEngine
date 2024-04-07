#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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
Token_t Tokenizer_GetNext(Tokenizer_t *context)
{
	Token_t token={ TOKEN_UNKNOWN };

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

		if(count>MAX_TOKEN_STRING_LENGTH)
			count=MAX_TOKEN_STRING_LENGTH;

		token.type=TOKEN_STRING;
		memcpy(token.string, &context->string[context->stringPosition], count);
		token.string[count]='\0';

		context->stringPosition+=count;

		// Re-classify the string if it matches any of these:
		if(IsWord(token.string, booleans, sizeof(booleans)/sizeof(booleans[0])))
		{
			token.type=TOKEN_BOOLEAN;

			if(strcmp(token.string, "true")==0)
				token.boolean=true;
			else
				token.boolean=false;
		}

		if(IsWord(token.string, context->keywords, context->numKeywords))
			token.type=TOKEN_KEYWORD;
	}
	else if(GetChar(context, 0)=='0'&&(GetChar(context, 1)=='x'||GetChar(context, 1)=='X')) // Hexidecimal numbers
	{
		const char *start=context->string+context->stringPosition+2;
		char *end=NULL;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, &end, 16);
		context->stringPosition+=end-start;
	}
	else if(GetChar(context, 0)=='0'&&(GetChar(context, 1)=='b'||GetChar(context, 1)=='B')) // Binary numbers
	{
		const char *start=context->string+context->stringPosition+2;
		char *end=NULL;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, &end, 2);
		context->stringPosition+=end-start;
	}
	else if((GetChar(context, 0)=='-'&&IsDigit(GetChar(context, 1)))||IsDigit(GetChar(context, 0))) // Whole numbers and floating point
	{
		const char *start=context->string+context->stringPosition;
		char *end=NULL;
		size_t count=0;

		while(!IsDelimiter(GetChar(context, count)))
		{
			if(GetChar(context, count)=='\0')
				break;

			count++;
		}

		if(GetChar(context, count)=='.')
		{
			token.type=TOKEN_FLOAT;
			token.fval=strtod(start, &end);
		}
		else
		{
			token.type=TOKEN_INT;
			token.ival=strtoll(start, &end, 10);
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

			if(count>MAX_TOKEN_STRING_LENGTH)
				count=MAX_TOKEN_STRING_LENGTH;

			token.type=TOKEN_QUOTED;
			memcpy(token.string, &context->string[context->stringPosition], count);
			token.string[count]='\0';

			context->stringPosition+=count+1;
		}
		else
		{
			token.type=TOKEN_DELIMITER;
			token.string[0]=GetChar(context, 0);

			context->stringPosition++;
		}
	}
	else if(GetChar(context, 0)=='\0')
	{
		token.type=TOKEN_END;
		context->stringPosition++;
	}
	else
		context->stringPosition++;

	return token;
}

// Same as Token_GetNext, but does not advance string pointer
Token_t Tokenizer_PeekNext(Tokenizer_t *context)
{
	char *start=context->string;
	Token_t result=Tokenizer_GetNext(context);
	context->string=start;

	return result;
}
