#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tokenizer.h"

static const char delimiters[]="\'\"{}[]()<>!@#$%^&*-+=,.:;\\/|`~ \t\n\r";
static const char *booleans[]={ "false", "true" };
static const char *descriptors[]={ "descriptorSet", "pipeline" };
static const char *keywords[]=
{
	// Pipeline defintions
	"addBinding", "addStage", "addVertexBinding", "addVertexAttribute",

	// Pipeline state keywords:
	"subpass",
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

Token_t Token_GetNext(char **string)
{
	Token_t token={ TOKEN_UNKNOWN };

    // Skip whitespaces
	while(**string==' '||**string=='\t'||**string=='\n'||**string=='\r')
		(*string)++;

	if(IsAlpha(**string))
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
	else if((**string=='-'&&IsDigit(*((*string)+1)))||IsDigit(**string))
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
			token.fval=strtof(start, string);
		}
		else
		{
			token.type=TOKEN_INT;
			token.ival=strtol(start, string, 10);
		}
	}
	else if(IsDelimiter(**string))
	{
		// special case, treat quoted items as whole string tokens
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

Token_t Token_PeekNext(char **string)
{
	char *start=*string;
	Token_t result=Token_GetNext(&start);

	return result;
}
