#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

// TODO:
//		Key/special words should be configurable via code.
//		Will probably have to make a context setup, which should also enable multithread safety.

static const char delimiters[]="\'\"{}[]()<>!@#$%^&*-+=,.:;\\/|`~ \t\n\r";
static const char *booleans[]={ "false", "true" };

// These are keywords for the pipeline decription script (see pipeline.c)
static const char *descriptors[]={ "descriptorSet", "pipeline" };
static const char *keywords[]=
{
	// Pipeline defintions
	"addBinding", "addStage", "addVertexBinding", "addVertexAttribute",

	// Pipeline state keywords:
	"subpass", "pushConstant",
	// Input assembly state
	"topology", "primitiveRestart",
	// Rasterization state
	"depthClamp", "rasterizerDiscard", "polygonMode", "cullMode", "frontFace",
	"depthBias", "depthBiasConstantFactor", "depthBiasClamp", "depthBiasSlopeFactor", "lineWidth",
	// Depth/stencil state
	"depthTest", "depthWrite", "depthCompareOp", "depthBoundsTest", "stencilTest", "minDepthBounds", "maxDepthBounds",
	// Front face stencil functions
	"frontStencilFailOp", "frontStencilPassOp", "frontStencilDepthFailOp", "frontStencilCompareOp",
	"frontStencilCompareMask", "frontStencilWriteMask", "frontStencilReference",
	// Back face stencil functions
	"backStencilFailOp", "backStencilPassOp", "backStencilDepthFailOp", "backStencilCompareOp",
	"backStencilCompareMask", "backStencilWriteMask", "backStencilReference",
	// Multisample state
	"rasterizationSamples", "sampleShading", "minSampleShading", "sampleMask", "alphaToCoverage", "alphaToOne",
	// blend state
	"blendLogicOp", "blendLogicOpState", "blend", "srcColorBlendFactor", "dstColorBlendFactor", "colorBlendOp",
	"srcAlphaBlendFactor", "dstAlphaBlendFactor", "alphaBlendOp", "colorWriteMask"
};

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
Token_t Token_GetNext(char **string)
{
	Token_t token={ TOKEN_UNKNOWN };

	SkipWhitespace(string);

	// Handle comments
	bool commentDone=true, singleLine=false;

	if(**string=='/'&&*((*string)+1)=='*')
	{
		singleLine=false;
		commentDone=false;
	}
	else if((**string=='/'&&*((*string)+1)=='/')||**string=='#')
	{
		singleLine=true;
		commentDone=false;
	}

	while(!commentDone)
	{
		while((*string)++)
		{
			if(singleLine)
			{
				if(**string=='\n'||**string=='\r')
				{
					SkipWhitespace(string);

					// Check if there are more comments and switch modes if needed
					if((**string=='/'&&*((*string)+1)=='/')||**string=='#')
						commentDone=false;
					else if(**string=='/'&&*((*string)+1)=='*')
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
				if(**string=='*'&&*((*string)+1)=='/')
				{
					// Eat the remaining '*' and '/'
					(*string)+=2;

					SkipWhitespace(string);

					if((**string=='/'&&*((*string)+1)=='/')||**string=='#')
					{
						commentDone=false;
						singleLine=true;
					}
					else if(**string=='/'&&*((*string)+1)=='*')
						commentDone=false;
					else
						commentDone=true;
					break;
				}
			}
		}
	}

	// Process the token
	if(IsAlpha(**string)) // Strings and special classification
	{
		const char *start=*string;
		uint32_t count=0;

		while(!IsDelimiter(**string))
		{
			if(**string=='\0')
				break;

			count++;
			(*string)++;
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

		if(IsWord(token.string, descriptors, sizeof(descriptors)/sizeof(descriptors[0])))
			token.type=TOKEN_DESCRIPTOR;

		if(IsWord(token.string, keywords, sizeof(keywords)/sizeof(keywords[0])))
			token.type=TOKEN_KEYWORD;
	}
	else if(**string=='0'&&(*((*string)+1)=='x'||*((*string)+1)=='X')) // Hexidecimal numbers
	{
		const char *start=*string+2;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, string, 16);
	}
	else if(**string=='0'&&(*((*string)+1)=='b'||*((*string)+1)=='B')) // Binary numbers
	{
		const char *start=*string+2;
		token.type=TOKEN_INT;
		token.ival=strtoll(start, string, 2);
	}
	else if((**string=='-'&&IsDigit(*((*string)+1)))||IsDigit(**string)) // Whole numbers and floating point
	{
		const char *start=*string;
		bool decimal=false;

		while(!IsDelimiter(**string)||**string=='.')
		{
			if(**string=='\0')
				break;

			if(**string=='.')
				decimal=true;

			(*string)++;
		}

		if(decimal)
		{
			token.type=TOKEN_FLOAT;
			token.fval=strtod(start, string);
		}
		else
		{
			token.type=TOKEN_INT;
			token.ival=strtoll(start, string, 10);
		}
	}
	else if(IsDelimiter(**string))
	{
		// special case, treat quoted items as special whole string tokens
		if(**string=='\"')
		{
			(*string)++;

			const char *start=*string;
			uint32_t count=0;

			while(**string!='\"')
			{
				if(**string=='\0')
					break;

				count++;
				(*string)++;
			}

			(*string)++;

			if(count>MAX_TOKEN_STRING_LENGTH)
				count=MAX_TOKEN_STRING_LENGTH;

			token.type=TOKEN_QUOTED;
			memcpy(token.string, start, count);
			token.string[count]='\0';
		}
		else
		{
			token.type=TOKEN_DELIMITER;
			token.string[0]=**string;

			(*string)++;
		}
	}
	else if(**string=='\0')
	{
		token.type=TOKEN_END;
		(*string)++;
	}
	else
		(*string)++;

	return token;
}

// Same as Token_GetNext, but does not advance string pointer
Token_t Token_PeekNext(char **string)
{
	char *start=*string;
	Token_t result=Token_GetNext(&start);

	return result;
}
