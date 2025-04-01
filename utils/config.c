#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "tokenizer.h"
#include "config.h"

Config_t config={ .windowWidth=1920, .windowHeight=1080, .msaaSamples=4, .deviceIndex=0 };

static const char *keywords[]=
{
	// Section decelations
	"config",

	// Subsection definitions
	"windowSize", "msaaSamples", "deviceIndex",
};

static void printToken(const char *msg, const Token_t *token)
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

bool Config_ReadINI(Config_t *config, const char *filename)
{
	///////// Set up some defaults

	// Configurable from config file
	config->windowWidth=1920;
	config->windowHeight=1080;
	config->msaaSamples=4;
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
					printToken("Unexpected token ", token);
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
							int32_t param=0;

							// First token should be a left parenthesis '('
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type!=TOKEN_DELIMITER&&token->string[0]!='(')
							{
								printToken("Unexpected token ", token);
								return false;
							}
							else
							{
								// Loop until right parenthesis ')' or until break condition
								while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
								{
									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_INT&&param==0)
										config->windowWidth=(uint32_t)token->ival;
									else if(token->type==TOKEN_INT&&param==1)
										config->windowHeight=(uint32_t)token->ival;
									else
									{
										printToken("Unexpected token ", token);
										return false;
									}

									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_DELIMITER)
									{
										// Still expecting more parameters, error.
										if(token->string[0]!=','&&param<1)
										{
											DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
											return false;
										}
										// End of expected parameters, end.
										else if(token->string[0]==')'&&param==2)
											break;
										else
											param++;
									}

									if(param>2)
									{
										DBGPRINTF(DEBUG_ERROR, "Too many params windowSize(width, height)\n");
										return false;
									}
								}
							}
						}
						else if(strcmp(token->string, "msaaSamples")==0)
						{
							int32_t param=0;

							// First token should be a left parenthesis '('
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type!=TOKEN_DELIMITER&&token->string[0]!='(')
							{
								printToken("Unexpected token ", token);
								return false;
							}
							else
							{
								// Loop until right parenthesis ')' or until break condition
								while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
								{
									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_INT&&param==0)
										config->msaaSamples=(uint32_t)token->ival;
									else
									{
										printToken("Unexpected token ", token);
										return false;
									}

									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_DELIMITER)
									{
										if(token->string[0]==')'&&param==1)
											break;
										else
											param++;
									}

									if(param>1)
									{
										DBGPRINTF(DEBUG_ERROR, "Too many params msaaSamples(samples)\n");
										return false;
									}
								}
							}
						}
						else if(strcmp(token->string, "deviceIndex")==0)
						{
							int32_t param=0;

							// First token should be a left parenthesis '('
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type!=TOKEN_DELIMITER&&token->string[0]!='(')
							{
								printToken("Unexpected token ", token);
								return false;
							}
							else
							{
								// Loop until right parenthesis ')' or until break condition
								while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
								{
									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_INT&&param==0)
										config->deviceIndex=(uint32_t)token->ival;
									else
									{
										printToken("Unexpected token ", token);
										return false;
									}

									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_DELIMITER)
									{
										if(token->string[0]==')'&&param==1)
											break;
										else
											param++;
									}

									if(param>1)
									{
										DBGPRINTF(DEBUG_ERROR, "Too many params deviceIndex(index)\n");
										return false;
									}
								}
							}
						}
						else
						{
							printToken("Unknown token ", token);
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