#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

bool Tokenizer_Init(Tokenizer_t *context, char *string, size_t numKeywords, const char **keywords)
{
	context->numKeywords=numKeywords;
	context->keywords=keywords;

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

static void SkipWhitespace(char **string)
{
	while(**string==' '||**string=='\t'||**string=='\n'||**string=='\r')
		(*string)++;
}

// Pull a token from string stream, classify it and advance the string pointer.
Token_t Tokenizer_GetNext(Tokenizer_t *context)
{
	Token_t token={ TOKEN_UNKNOWN };

	SkipWhitespace(&context->string);

	// Handle comments
	bool commentDone=true, singleLine=false;

	if(*context->string=='/'&&(*context->string+1)=='*')
	{
		singleLine=false;
		commentDone=false;
	}
	else if((*context->string=='/'&&(*context->string+1)=='/')||*context->string=='#')
	{
		singleLine=true;
		commentDone=false;
	}

	while(!commentDone)
	{
		while((*context->string)++)
		{
			if(singleLine)
			{
				if(*context->string=='\n'||*context->string=='\r')
				{
					SkipWhitespace(&context->string);

					// Check if there are more comments and switch modes if needed
					if((*context->string=='/'&&(*context->string+1)=='/')||*context->string=='#')
						commentDone=false;
					else if(*context->string=='/'&&(*context->string+1)=='*')
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
				if(*context->string=='*'&&(*context->string+1)=='/')
				{
					// Eat the remaining '*' and '/'
					(*context->string)+=2;

					SkipWhitespace(&context->string);

					if((*context->string=='/'&&(*context->string+1)=='/')||*context->string=='#')
					{
						commentDone=false;
						singleLine=true;
					}
					else if(*context->string=='/'&&(*context->string+1)=='*')
						commentDone=false;
					else
						commentDone=true;
					break;
				}
			}
		}
	}

	// Process the token
	if(IsAlpha(*context->string)) // Strings and special classification
	{
		const char *start=context->string;
		uint32_t count=0;

		while(!IsDelimiter(*context->string))
		{
			if(*context->string=='\0')
				break;

			count++;
			context->string++;
		}

		if(count>MAX_TOKEN_STRING_LENGTH)
			count=MAX_TOKEN_STRING_LENGTH;

		token.type=TOKEN_STRING;
		memcpy(token.string, start, count);
		token.string[count]='\0';

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
	else if(*context->string=='0'&&((*context->string+1)=='x'||(*context->string+1)=='X')) // Hexidecimal numbers
	{
		const char *start=context->string+2;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, &context->string, 16);
	}
	else if(*context->string=='0'&&((*context->string+1)=='b'||(*context->string+1)=='B')) // Binary numbers
	{
		const char *start=context->string+2;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, &context->string, 2);
	}
	else if((*context->string=='-'&&IsDigit((*context->string+1)))||IsDigit(*context->string)) // Whole numbers and floating point
	{
		const char *start=context->string;
		bool decimal=false;

		while(!IsDelimiter(*context->string)||*context->string=='.')
		{
			if(*context->string=='\0')
				break;

			if(*context->string=='.')
				decimal=true;

			context->string++;
		}

		if(decimal)
		{
			token.type=TOKEN_FLOAT;
			token.fval=strtod(start, &context->string);
		}
		else
		{
			token.type=TOKEN_INT;
			token.ival=strtoll(start, &context->string, 10);
		}
	}
	else if(IsDelimiter(*context->string))
	{
		// special case, treat quoted items as special whole string tokens
		if(*context->string=='\"')
		{
			context->string++;

			const char *start=context->string;
			uint32_t count=0;

			while(*context->string!='\"')
			{
				if(*context->string=='\0')
					break;

				count++;
				context->string++;
			}

			context->string++;

			if(count>MAX_TOKEN_STRING_LENGTH)
				count=MAX_TOKEN_STRING_LENGTH;

			token.type=TOKEN_QUOTED;
			memcpy(token.string, start, count);
			token.string[count]='\0';
		}
		else
		{
			token.type=TOKEN_DELIMITER;
			token.string[0]=*context->string;

			context->string++;
		}
	}
	else if(*context->string=='\0')
	{
		token.type=TOKEN_END;
		context->string++;
	}
	else
		context->string++;

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
