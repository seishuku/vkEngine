#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "pipeline.h"
#include "tokenizer.h"
#include "config.h"

Config_t config={ .windowWidth=1920, .windowHeight=1080, .msaaSamples=4, .deviceIndex=0 };

static const char *keywords[]=
{
	// Section decelations
	"config",

	// Subsection definitions
	"windowSize", "msaaSamples", "deviceIndex",
	"test1", "test2"
};

bool Config_ReadINI(Config_t *config, const char *filename)
{
	///////// Set up some defaults

	// Configurable from config file
	config->windowWidth=1920;
	config->windowHeight=1080;
	config->deviceIndex=0;

	// System state
	config->renderWidth=1920;
	config->renderHeight=1080;

	config->MSAA=VK_SAMPLE_COUNT_4_BIT;
	config->colorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
	config->depthFormat=VK_FORMAT_D32_SFLOAT;

	config->isVR=false;
	/////////

	FILE *stream=NULL;

	if((stream=fopen(filename, "rb"))==NULL)
		return false;

	fseek(stream, 0, SEEK_END);
	size_t length=ftell(stream);
	fseek(stream, 0, SEEK_SET);

	char *buffer=(char *)Zone_Malloc(zone, length+1);

	if(buffer==NULL)
		return false;

	fread(buffer, 1, length, stream);
	buffer[length]='\0';

	Tokenizer_t tokenizer;
	Tokenizer_Init(&tokenizer, length, buffer, sizeof(keywords)/sizeof(keywords[0]), keywords);

	Token_t *token=NULL;

	while(1)
	{
		token=Tokenizer_GetNext(&tokenizer);

		if(token==NULL)
			break;

		if(token->type==TOKEN_KEYWORD)
		{
			// Check if it's the 'config' section
			if(strcmp(token->string, "config")==0)
			{
				// Next token must be a left brace '{'
				Zone_Free(zone, token);
				token=Tokenizer_GetNext(&tokenizer);

				if(token->type!=TOKEN_DELIMITER&&token->string[0]!='{')
				{
					Tokenizer_PrintToken("Unexpected token ", token);
					return false;
				}

				// Loop through until closing right brace '}'
				while(!(token->type==TOKEN_DELIMITER&&token->string[0]=='}'))
				{
					// Look for keyword tokens
					Zone_Free(zone, token);
					token=Tokenizer_GetNext(&tokenizer);

					if(token->type==TOKEN_KEYWORD)
					{
						if(strcmp(token->string, "windowSize")==0)
						{
							int32_t width=0, height=0;

							if(!Tokenizer_ArgumentHelper(&tokenizer, "ii", &width, &height))
								return false;

							if(width>=0&&width<7680)
								config->windowWidth=width;
							else
							{
								DBGPRINTF(DEBUG_ERROR, "Config window width out of range.\n");
								return false;
							}

							if(height>=0&&height<4320)
								config->windowHeight=height;
							else
							{
								DBGPRINTF(DEBUG_ERROR, "Config window height out of range.\n");
								return false;
							}
						}
						else if(strcmp(token->string, "msaaSamples")==0)
						{
							int32_t msaaSamples=0;

							if(!Tokenizer_ArgumentHelper(&tokenizer, "i", &msaaSamples))
								return false;

							switch(msaaSamples)
							{
								case 1:		config->MSAA=VK_SAMPLE_COUNT_1_BIT;		break;
								case 2:		config->MSAA=VK_SAMPLE_COUNT_2_BIT;		break;
								case 4:		config->MSAA=VK_SAMPLE_COUNT_4_BIT;		break;
								case 8:		config->MSAA=VK_SAMPLE_COUNT_8_BIT;		break;
								case 16:	config->MSAA=VK_SAMPLE_COUNT_16_BIT;	break;
								case 32:	config->MSAA=VK_SAMPLE_COUNT_32_BIT;	break;
								case 64:	config->MSAA=VK_SAMPLE_COUNT_64_BIT;	break;
								default:	config->MSAA=VK_SAMPLE_COUNT_1_BIT;		break;
							}
						}
						else if(strcmp(token->string, "deviceIndex")==0)
						{
							int32_t deviceIndex=0;

							if(!Tokenizer_ArgumentHelper(&tokenizer, "i", &deviceIndex))
								return false;

							if(deviceIndex>=0&&deviceIndex<4)
								config->deviceIndex=deviceIndex;
							else
							{
								DBGPRINTF(DEBUG_ERROR, "Config device index out of range.\n");
								return false;
							}
						}
						else
						{
							Tokenizer_PrintToken("Unknown token ", token);
							return false;
						}
					}
				}
			}
		}

		if(token)
			Zone_Free(zone, token);
	}

	Zone_Free(zone, buffer);

	return true;
}