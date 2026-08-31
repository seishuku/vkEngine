#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "pipeline.h"
#include "tokenizer.h"
#include "parser.h"
#include "config.h"

Config_t config={ .windowWidth=1920, .windowHeight=1080, .msaaSamples=4, .deviceIndex=0 };

static const char *keywords[]=
{
	// Section decelations
	"config",

	// Subsection definitions
	"windowSize", "msaaSamples", "deviceIndex", "vsync"
};

bool Config_ReadINI(Config_t *config, const char *filename)
{
	///////// Set up some defaults

	// Configurable from config file
	config->windowWidth=1920;
	config->windowHeight=1080;
	config->deviceIndex=0;
	config->msaaSamples=2;

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
	fclose(stream);
	
	buffer[length]='\0';

	Tokenizer_t tokenizer;
	Tokenizer_Init(&tokenizer, length, buffer, sizeof(keywords)/sizeof(keywords[0]), keywords);

	Parser_t parser;
	Parser_Init(&parser, &tokenizer);

	while(!Parser_IsEnd(&parser))
	{
		// Check if it's the 'config' section
		if(Parser_MatchKeyword(&parser, "config"))
		{
			if(!Parser_Expect(&parser, TOKEN_DELIMITER, '{'))
				return false;

			// Loop through until closing right brace '}'
			while(!Parser_Match(&parser, TOKEN_DELIMITER, '}'))
			{
				if(Parser_MatchKeyword(&parser, "windowSize"))
				{
					int64_t width=0, height=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &width))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_Integer(&parser, &height))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
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
				else if(Parser_MatchKeyword(&parser, "msaaSamples"))
				{
					int64_t msaaSamples=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &msaaSamples))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					if(msaaSamples>=1&&msaaSamples<=64)
					{
						config->msaaSamples=msaaSamples;

						switch(config->msaaSamples)
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
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Config MSAA out of range (%d).\n", msaaSamples);
						return false;
					}
				}
				else if(Parser_MatchKeyword(&parser, "deviceIndex"))
				{
					int64_t deviceIndex=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &deviceIndex))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					if(deviceIndex>=0&&deviceIndex<=3)
						config->deviceIndex=deviceIndex;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Config device index out of range (%d).\n", deviceIndex);
						return false;
					}
				}
				else if(Parser_MatchKeyword(&parser, "vsync"))
				{
					bool vsync=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Boolean(&parser, &vsync))
						return false;
					else
						config->vsync=vsync?true:false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else
				{
					Parser_Unexpected(&parser, "Unexpected");
					return false;
				}
			}
		}
	}

	Zone_Free(zone, buffer);

	return true;
}
