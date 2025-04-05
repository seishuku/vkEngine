#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "tokenizer.h"
#include "base64.h"
#include "pipeline.h"

// TODO: don't like global for this, maybe change to creation flags on CreatePipeline?
static VkSampleCountFlags rasterizationSamplesOverride=VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM;

void PipelineOverrideRasterizationSamples(const VkSampleCountFlags rasterizationSamples)
{
	rasterizationSamplesOverride=rasterizationSamples;
}

// These are keywords for the pipeline description script
static const char *keywords[]=
{
	// Section decelations
	"descriptorSet", "pipeline",

	// Descriptor set definition
	"addBinding",

	// Pipeline defintions
	"addStage", "addVertexBinding", "addVertexAttribute",

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
	"srcAlphaBlendFactor", "dstAlphaBlendFactor", "alphaBlendOp", "colorWriteMask",

	// base64 encoded shader binary
	"base64",
};

bool CreatePipeline(VkuContext_t *context, Pipeline_t *pipeline, VkRenderPass renderPass, const char *filename)
{
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

	memset(pipeline, 0, sizeof(Pipeline_t));

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
			// Start building up descriptor set layout ("descriptorSet { }")
			if(strcmp(token->string, "descriptorSet")==0)
			{
				if(!vkuInitDescriptorSet(&pipeline->descriptorSet, context->device))
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to initialize descriptor set.\n");
					return false;
				}

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
						// Handle "addBinding" keyword
						if(strcmp(token->string, "addBinding")==0)
						{
							int32_t param=0;
							uint32_t binding=0;
							VkDescriptorType type=0;
							VkShaderStageFlags stage=0;

							// First token should be a left parenthesis '('
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

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
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_INT&&param==0)
										binding=(uint32_t)token->ival;
									else if(token->type==TOKEN_STRING)
									{
										// Type parameter
										if(strcmp(token->string, "sampler")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_SAMPLER;
										else if(strcmp(token->string, "combinedSampler")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
										else if(strcmp(token->string, "sampledImage")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
										else if(strcmp(token->string, "storageImage")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
										else if(strcmp(token->string, "uniformTexelBuffer")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
										else if(strcmp(token->string, "storageTexelBuffer")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
										else if(strcmp(token->string, "uniformBuffer")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
										else if(strcmp(token->string, "storageBuffer")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
										else if(strcmp(token->string, "uniformBufferDynamic")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
										else if(strcmp(token->string, "storageBufferDynamic")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
										else if(strcmp(token->string, "inputAttachment")==0&&param==1)
											type=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

										// Stage parameter
										else if(strcmp(token->string, "vertex")==0&&param==2)
											stage|=VK_SHADER_STAGE_VERTEX_BIT;
										else if(strcmp(token->string, "tessellationControl")==0&&param==2)
											stage|=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
										else if(strcmp(token->string, "tessellationEvaluation")==0&&param==2)
											stage|=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
										else if(strcmp(token->string, "geometry")==0&&param==2)
											stage|=VK_SHADER_STAGE_GEOMETRY_BIT;
										else if(strcmp(token->string, "fragment")==0&&param==2)
											stage|=VK_SHADER_STAGE_FRAGMENT_BIT;
										else if(strcmp(token->string, "compute")==0&&param==2)
											stage|=VK_SHADER_STAGE_COMPUTE_BIT;
										else
										{
											Tokenizer_PrintToken("Unknown parameter ", token);
											return false;
										}
									}
									else
									{
										Tokenizer_PrintToken("Unexpected token ", token);
										return false;
									}

									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									if(token->type==TOKEN_DELIMITER)
									{
										// Still expecting more parameters, error.
										if(token->string[0]!=','&&param<2)
										{
											DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
											return false;
										}
										// End of expected parameters, end.
										else if(token->string[0]==')'&&param==2)
											break;
										// Special case for 3rd parameter, because multiple stages can be specified.
										else if(token->string[0]!='|'&&param<2)
											param++;
									}

									if(param>2)
									{
										DBGPRINTF(DEBUG_ERROR, "Too many params addBinding(binding, type, stage)\n");
										return false;
									}
								}

								if(!vkuDescriptorSet_AddBinding(&pipeline->descriptorSet, binding, type, stage))
								{
									DBGPRINTF(DEBUG_ERROR, "Unable to add binding.\n");
									return false;
								}
							}
						}
						else
						{
							Tokenizer_PrintToken("Unknown token ", token);
							return false;
						}
					}
				}

				if(!vkuAssembleDescriptorSetLayout(&pipeline->descriptorSet))
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to assemble descriptor set layout.\n");
					return false;
				}
			}
			// Start building up pipeline ("pipeline { }")
			else if(strcmp(token->string, "pipeline")==0)
			{
				if(!vkuInitPipeline(&pipeline->pipeline, context->device, context->pipelineCache))
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to initialize pipeline.\n");
					return false;
				}

				Zone_Free(zone, token);
				token=Tokenizer_GetNext(&tokenizer);

				if(token->type!=TOKEN_DELIMITER&&token->string[0]!='{')
				{
					Tokenizer_PrintToken("Unexpected token ", token);
					return false;
				}

				while(!(token->type==TOKEN_DELIMITER&&token->string[0]=='}'))
				{
					Zone_Free(zone, token);
					token=Tokenizer_GetNext(&tokenizer);

					// Pipeline attribute keywords: "addStage", "addVertexBinding", "addVertexAttribute",
					// Pipeline state keywords:
					// subpass
					// Input assembly state: "topology", "primitiveRestart",
					// Rasterization state: "depthClamp", "rasterizerDiscard", "polygonMode", "cullMode", "frontFace",
					//						"depthBias", "depthBiasConstantFactor", "depthBiasClamp", "depthBiasSlopeFactor", "lineWidth",
					// Depth/stencil state: "depthTest", "depthWrite", "depthCompareOp", "depthBoundsTest", "stencilTest", "minDepthBounds", "maxDepthBounds",
					// Front face stencil functions: "frontStencilFailOp", "frontStencilPassOp", "frontStencilDepthFailOp", "frontStencilCompareOp",
					//								 "frontStencilCompareMask", "frontStencilWriteMask", "frontStencilReference",
					// Back face stencil functions: "backStencilFailOp", "backStencilPassOp", "backStencilDepthFailOp", "backStencilCompareOp",
					//								"backStencilCompareMask", "backStencilWriteMask", "backStencilReference",
					// Multisample state: "rasterizationSamples", "sampleShading", "minSampleShading", "sampleMask", "alphaToCoverage", "alphaToOne",
					// blend state: "blendLogicOp", "blendLogicOpState", "blend", "srcColorBlendFactor", "dstColorBlendFactor", "colorBlendOp",
					//				"srcAlphaBlendFactor", "dstAlphaBlendFactor", "alphaBlendOp", "colorWriteMask"
					// Pipeline push constants: pushConstant

					if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "addStage")==0)
					{
						uint32_t param=0;
						char shaderFilename[1025]={ 0 };
						uint8_t *shaderData=NULL;
						uint32_t shaderSize=0;
						VkShaderStageFlagBits stage=0;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_QUOTED&&param==0)
								strncpy(shaderFilename, token->string, 1024);
							else if(token->type==TOKEN_KEYWORD)
							{
								if(strcmp(token->string, "base64")==0&&param==0)
								{
									Zone_Free(zone, token);
									token=Tokenizer_GetNext(&tokenizer);

									shaderData=Zone_Malloc(zone, strlen(token->string));

									if(shaderData==NULL)
										return false;

									shaderSize=base64Decode(token->string, shaderData);
								}
							}
							else if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "vertex")==0&&param==1)
									stage=VK_SHADER_STAGE_VERTEX_BIT;
								else if(strcmp(token->string, "tessellationControl")==0&&param==1)
									stage=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
								else if(strcmp(token->string, "tessellationEvaluation")==0&&param==1)
									stage=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
								else if(strcmp(token->string, "geometry")==0&&param==1)
									stage=VK_SHADER_STAGE_GEOMETRY_BIT;
								else if(strcmp(token->string, "fragment")==0&&param==1)
									stage=VK_SHADER_STAGE_FRAGMENT_BIT;
								else if(strcmp(token->string, "compute")==0&&param==1)
									stage=VK_SHADER_STAGE_COMPUTE_BIT;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER)
							{
								if(token->string[0]!=','&&param<1)
								{
									DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
									return false;
								}
								else if(token->string[0]==')'&&param==1)
									break;
								else
									param++;
							}

							if(param>1)
							{
								DBGPRINTF(DEBUG_ERROR, "Too many params addStage(\"shader filename\", stage)\n");
								return false;
							}
						}

						if(shaderData)
						{
							if(!vkuPipeline_AddStageMemory(&pipeline->pipeline, (uint32_t *)shaderData, shaderSize, stage))
							{
								DBGPRINTF(DEBUG_ERROR, "Unable to add shader stage to pipeline: %s\n", shaderFilename);
								return false;
							}

							Zone_Free(zone, shaderData);
						}
						else
						{
							if(!vkuPipeline_AddStage(&pipeline->pipeline, shaderFilename, stage))
							{
								DBGPRINTF(DEBUG_ERROR, "Unable to add shader stage to pipeline: %s\n", shaderFilename);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "addVertexBinding")==0)
					{
						uint32_t param=0;
						uint32_t binding=0;
						uint32_t stride=0;
						VkVertexInputRate inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT&&param==0)
								binding=(uint32_t)token->ival;
							else if(token->type==TOKEN_INT&&param==1)
								stride=(uint32_t)token->ival;
							else if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "perVertex")==0&&param==2)
									inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
								else if(strcmp(token->string, "perInstance")==0&&param==2)
									inputRate=VK_VERTEX_INPUT_RATE_INSTANCE;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER)
							{
								if(token->string[0]!=','&&param<2)
								{
									DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
									return false;
								}
								else if(token->string[0]==')'&&param==2)
									break;
								else
									param++;
							}

							if(param>2)
							{
								DBGPRINTF(DEBUG_ERROR, "Too many params addVertexBinding(binding, stride, inputRate)\n");
								return false;
							}
						}

						if(!vkuPipeline_AddVertexBinding(&pipeline->pipeline, binding, stride, inputRate))
						{
							DBGPRINTF(DEBUG_ERROR, "Unable to add vertex binding.\n");
							return false;
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "addVertexAttribute")==0)
					{
						uint32_t param=0;
						uint32_t location=0;
						uint32_t binding=0;
						VkFormat format=VK_FORMAT_UNDEFINED;
						uint32_t offset=0;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT&&param==0)
								location=(uint32_t)token->ival;
							else if(token->type==TOKEN_INT&&param==1)
								binding=(uint32_t)token->ival;
							else if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "r8_unorm")==0&&param==2)
									format=VK_FORMAT_R8_UNORM;
								else if(strcmp(token->string, "r8_snorm")==0&&param==2)
									format=VK_FORMAT_R8_SNORM;
								else if(strcmp(token->string, "r8_uint")==0&&param==2)
									format=VK_FORMAT_R8_UINT;
								else if(strcmp(token->string, "r8_sint")==0&&param==2)
									format=VK_FORMAT_R8_SINT;
								else if(strcmp(token->string, "rg8_unorm")==0&&param==2)
									format=VK_FORMAT_R8G8_UNORM;
								else if(strcmp(token->string, "rg8_snorm")==0&&param==2)
									format=VK_FORMAT_R8G8_SNORM;
								else if(strcmp(token->string, "rg8_uint")==0&&param==2)
									format=VK_FORMAT_R8G8_UINT;
								else if(strcmp(token->string, "rg8_sint")==0&&param==2)
									format=VK_FORMAT_R8G8_SINT;
								else if(strcmp(token->string, "rgb8_unorm")==0&&param==2)
									format=VK_FORMAT_R8G8B8_UNORM;
								else if(strcmp(token->string, "rgb8_snorm")==0&&param==2)
									format=VK_FORMAT_R8G8B8_SNORM;
								else if(strcmp(token->string, "rgb8_uint")==0&&param==2)
									format=VK_FORMAT_R8G8B8_UINT;
								else if(strcmp(token->string, "rgb8_sint")==0&&param==2)
									format=VK_FORMAT_R8G8B8_SINT;
								else if(strcmp(token->string, "bgr8_unorm")==0&&param==2)
									format=VK_FORMAT_B8G8R8_UNORM;
								else if(strcmp(token->string, "bgr8_snorm")==0&&param==2)
									format=VK_FORMAT_B8G8R8_SNORM;
								else if(strcmp(token->string, "bgr8_uint")==0&&param==2)
									format=VK_FORMAT_B8G8R8_UINT;
								else if(strcmp(token->string, "bgr8_sint")==0&&param==2)
									format=VK_FORMAT_B8G8R8_SINT;
								else if(strcmp(token->string, "rgba8_unorm")==0&&param==2)
									format=VK_FORMAT_R8G8B8A8_UNORM;
								else if(strcmp(token->string, "rgba8_snorm")==0&&param==2)
									format=VK_FORMAT_R8G8B8A8_SNORM;
								else if(strcmp(token->string, "rgba8_uint")==0&&param==2)
									format=VK_FORMAT_R8G8B8A8_UINT;
								else if(strcmp(token->string, "rgba8_sint")==0&&param==2)
									format=VK_FORMAT_R8G8B8A8_SINT;
								else if(strcmp(token->string, "bgra8_unorm")==0&&param==2)
									format=VK_FORMAT_B8G8R8A8_UNORM;
								else if(strcmp(token->string, "bgra8_snorm")==0&&param==2)
									format=VK_FORMAT_B8G8R8A8_SNORM;
								else if(strcmp(token->string, "bgra8_uint")==0&&param==2)
									format=VK_FORMAT_B8G8R8A8_UINT;
								else if(strcmp(token->string, "bgra8_sint")==0&&param==2)
									format=VK_FORMAT_B8G8R8A8_SINT;
								else if(strcmp(token->string, "abgr8_unorm")==0&&param==2)
									format=VK_FORMAT_A8B8G8R8_UNORM_PACK32;
								else if(strcmp(token->string, "abgr8_uint")==0&&param==2)
									format=VK_FORMAT_A8B8G8R8_SNORM_PACK32;
								else if(strcmp(token->string, "abgr8_uint")==0&&param==2)
									format=VK_FORMAT_A8B8G8R8_UINT_PACK32;
								else if(strcmp(token->string, "abgr8_sint")==0&&param==2)
									format=VK_FORMAT_A8B8G8R8_SINT_PACK32;
								else if(strcmp(token->string, "a2r10g10b10_unorm")==0&&param==2)
									format=VK_FORMAT_A2R10G10B10_UNORM_PACK32;
								else if(strcmp(token->string, "a2r10g10b10_snorm")==0&&param==2)
									format=VK_FORMAT_A2R10G10B10_SNORM_PACK32;
								else if(strcmp(token->string, "a2r10g10b10_uint")==0&&param==2)
									format=VK_FORMAT_A2R10G10B10_UINT_PACK32;
								else if(strcmp(token->string, "a2r10g10b10_sint")==0&&param==2)
									format=VK_FORMAT_A2R10G10B10_SINT_PACK32;
								else if(strcmp(token->string, "a2b10g10r10_unorm")==0&&param==2)
									format=VK_FORMAT_A2B10G10R10_UNORM_PACK32;
								else if(strcmp(token->string, "a2b10g10r10_snorm")==0&&param==2)
									format=VK_FORMAT_A2B10G10R10_SNORM_PACK32;
								else if(strcmp(token->string, "a2b10g10r10_uint")==0&&param==2)
									format=VK_FORMAT_A2B10G10R10_UINT_PACK32;
								else if(strcmp(token->string, "a2b10g10r10_sint")==0&&param==2)
									format=VK_FORMAT_A2B10G10R10_SINT_PACK32;
								else if(strcmp(token->string, "r16_unorm")==0&&param==2)
									format=VK_FORMAT_R16_UNORM;
								else if(strcmp(token->string, "r16_snorm")==0&&param==2)
									format=VK_FORMAT_R16_SNORM;
								else if(strcmp(token->string, "r16_uint")==0&&param==2)
									format=VK_FORMAT_R16_UINT;
								else if(strcmp(token->string, "r16_sint")==0&&param==2)
									format=VK_FORMAT_R16_SINT;
								else if(strcmp(token->string, "r16_sfloat")==0&&param==2)
									format=VK_FORMAT_R16_SFLOAT;
								else if(strcmp(token->string, "rg16_unorm")==0&&param==2)
									format=VK_FORMAT_R16G16_UNORM;
								else if(strcmp(token->string, "rg16_snorm")==0&&param==2)
									format=VK_FORMAT_R16G16_SNORM;
								else if(strcmp(token->string, "rg16_uint")==0&&param==2)
									format=VK_FORMAT_R16G16_UINT;
								else if(strcmp(token->string, "rg16_sint")==0&&param==2)
									format=VK_FORMAT_R16G16_SINT;
								else if(strcmp(token->string, "rg16_sfloat")==0&&param==2)
									format=VK_FORMAT_R16G16_SFLOAT;
								else if(strcmp(token->string, "rgb16_unorm")==0&&param==2)
									format=VK_FORMAT_R16G16B16_UNORM;
								else if(strcmp(token->string, "rgb16_snorm")==0&&param==2)
									format=VK_FORMAT_R16G16B16_SNORM;
								else if(strcmp(token->string, "rgb16_uint")==0&&param==2)
									format=VK_FORMAT_R16G16B16_UINT;
								else if(strcmp(token->string, "rgb16_sint")==0&&param==2)
									format=VK_FORMAT_R16G16B16_SINT;
								else if(strcmp(token->string, "rgb16_sfloat")==0&&param==2)
									format=VK_FORMAT_R16G16B16_SFLOAT;
								else if(strcmp(token->string, "rgba16_unorm")==0&&param==2)
									format=VK_FORMAT_R16G16B16A16_UNORM;
								else if(strcmp(token->string, "rgba16_snorm")==0&&param==2)
									format=VK_FORMAT_R16G16B16A16_SNORM;
								else if(strcmp(token->string, "rgba16_uint")==0&&param==2)
									format=VK_FORMAT_R16G16B16A16_UINT;
								else if(strcmp(token->string, "rgba16_sint")==0&&param==2)
									format=VK_FORMAT_R16G16B16A16_SINT;
								else if(strcmp(token->string, "rgba16_sfloat")==0&&param==2)
									format=VK_FORMAT_R16G16B16A16_SFLOAT;
								else if(strcmp(token->string, "r32_uint")==0&&param==2)
									format=VK_FORMAT_R32_UINT;
								else if(strcmp(token->string, "r32_sint")==0&&param==2)
									format=VK_FORMAT_R32_SINT;
								else if(strcmp(token->string, "r32_sfloat")==0&&param==2)
									format=VK_FORMAT_R32_SFLOAT;
								else if(strcmp(token->string, "rg32_uint")==0&&param==2)
									format=VK_FORMAT_R32G32_UINT;
								else if(strcmp(token->string, "rg32_sint")==0&&param==2)
									format=VK_FORMAT_R32G32_SINT;
								else if(strcmp(token->string, "rg32_sfloat")==0&&param==2)
									format=VK_FORMAT_R32G32_SFLOAT;
								else if(strcmp(token->string, "rgb32_uint")==0&&param==2)
									format=VK_FORMAT_R32G32B32_UINT;
								else if(strcmp(token->string, "rgb32_sint")==0&&param==2)
									format=VK_FORMAT_R32G32B32_SINT;
								else if(strcmp(token->string, "rgb32_sfloat")==0&&param==2)
									format=VK_FORMAT_R32G32B32_SFLOAT;
								else if(strcmp(token->string, "rgba32_uint")==0&&param==2)
									format=VK_FORMAT_R32G32B32A32_UINT;
								else if(strcmp(token->string, "rgba32_sint")==0&&param==2)
									format=VK_FORMAT_R32G32B32A32_SINT;
								else if(strcmp(token->string, "rgba32_sfloat")==0&&param==2)
									format=VK_FORMAT_R32G32B32A32_SFLOAT;
								else if(strcmp(token->string, "r64_uint")==0&&param==2)
									format=VK_FORMAT_R64_UINT;
								else if(strcmp(token->string, "r64_sint")==0&&param==2)
									format=VK_FORMAT_R64_SINT;
								else if(strcmp(token->string, "r64_sfloat")==0&&param==2)
									format=VK_FORMAT_R64_SFLOAT;
								else if(strcmp(token->string, "rg64_uint")==0&&param==2)
									format=VK_FORMAT_R64G64_UINT;
								else if(strcmp(token->string, "rg64_sint")==0&&param==2)
									format=VK_FORMAT_R64G64_SINT;
								else if(strcmp(token->string, "rg64_sfloat")==0&&param==2)
									format=VK_FORMAT_R64G64_SFLOAT;
								else if(strcmp(token->string, "rgb64_uint")==0&&param==2)
									format=VK_FORMAT_R64G64B64_UINT;
								else if(strcmp(token->string, "rgb64_sint")==0&&param==2)
									format=VK_FORMAT_R64G64B64_SINT;
								else if(strcmp(token->string, "rgb64_sfloat")==0&&param==2)
									format=VK_FORMAT_R64G64B64_SFLOAT;
								else if(strcmp(token->string, "rgba64_uint")==0&&param==2)
									format=VK_FORMAT_R64G64B64A64_UINT;
								else if(strcmp(token->string, "rgba64_sint")==0&&param==2)
									format=VK_FORMAT_R64G64B64A64_SINT;
								else if(strcmp(token->string, "rgba64_sfloat")==0&&param==2)
									format=VK_FORMAT_R64G64B64A64_SFLOAT;
								else
								{
									Tokenizer_PrintToken("Unknown format parameter ", token);
									return false;
								}
							}
							else if(token->type==TOKEN_INT&&param==3)
								offset=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER)
							{
								if(token->string[0]!=','&&param<3)
								{
									DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
									return false;
								}
								else if(token->string[0]==')'&&param==3)
									break;
								else
									param++;
							}

							if(param>3)
							{
								DBGPRINTF(DEBUG_ERROR, "Too many params addVertexAttribute(location, binding, format, offset)\n");
								return false;
							}
						}

						if(!vkuPipeline_AddVertexAttribute(&pipeline->pipeline, location, binding, format, offset))
						{
							DBGPRINTF(DEBUG_ERROR, "Unable to add vertex binding.\n");
							return false;
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "subpass")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.subpass=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.subpass=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "topology")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "pointList")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
								else if(strcmp(token->string, "lineList")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
								else if(strcmp(token->string, "lineStrip")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
								else if(strcmp(token->string, "triangleList")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
								else if(strcmp(token->string, "triangleStrip")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
								else if(strcmp(token->string, "triangleFan")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
								else if(strcmp(token->string, "listListAdjacency")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
								else if(strcmp(token->string, "listStripAdjacency")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
								else if(strcmp(token->string, "triangleListAdjacency")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
								else if(strcmp(token->string, "triangleStripAdjacency")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
								else if(strcmp(token->string, "patchList")==0)
									pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
									break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "primitiveRestart")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.primitiveRestart=VK_TRUE;
								else
									pipeline->pipeline.primitiveRestart=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthClamp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.depthClamp=VK_TRUE;
								else
									pipeline->pipeline.depthClamp=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "rasterizerDiscard")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.rasterizerDiscard=VK_TRUE;
								else
									pipeline->pipeline.rasterizerDiscard=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "polygonMode")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.polygonMode=VK_POLYGON_MODE_FILL;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);
								
							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "fill")==0)
									pipeline->pipeline.polygonMode=VK_POLYGON_MODE_FILL;
								else if(strcmp(token->string, "line")==0)
									pipeline->pipeline.polygonMode=VK_POLYGON_MODE_LINE;
								else if(strcmp(token->string, "point")==0)
									pipeline->pipeline.polygonMode=VK_POLYGON_MODE_POINT;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "cullMode")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.cullMode=VK_CULL_MODE_NONE;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "none")==0)
									pipeline->pipeline.cullMode=VK_CULL_MODE_NONE;
								else if(strcmp(token->string, "front")==0)
									pipeline->pipeline.cullMode=VK_CULL_MODE_FRONT_BIT;
								else if(strcmp(token->string, "back")==0)
									pipeline->pipeline.cullMode=VK_CULL_MODE_BACK_BIT;
								else if(strcmp(token->string, "frontAndBack")==0)
									pipeline->pipeline.cullMode=VK_CULL_MODE_FRONT_AND_BACK;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontFace")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "ccw")==0)
									pipeline->pipeline.cullMode=VK_FRONT_FACE_COUNTER_CLOCKWISE;
								else if(strcmp(token->string, "cw")==0)
									pipeline->pipeline.cullMode=VK_FRONT_FACE_CLOCKWISE;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthBias")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.depthBias=VK_TRUE;
								else
									pipeline->pipeline.depthBias=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthBiasConstantFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.depthBiasConstantFactor=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.depthBiasConstantFactor=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthBiasClamp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.depthBiasClamp=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.depthBiasClamp=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthBiasSlopeFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.depthBiasSlopeFactor=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.depthBiasSlopeFactor=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "lineWidth")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.lineWidth=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.lineWidth=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthTest")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.depthTest=VK_TRUE;
								else
									pipeline->pipeline.depthTest=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthWrite")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.depthWrite=VK_TRUE;
								else
									pipeline->pipeline.depthWrite=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthCompareOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_NEVER;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "never")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_NEVER;
								else if(strcmp(token->string, "less")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_LESS;
								else if(strcmp(token->string, "equal")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_EQUAL;
								else if(strcmp(token->string, "lessOrEqual")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
								else if(strcmp(token->string, "greater")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_GREATER;
								else if(strcmp(token->string, "notEqual")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_NOT_EQUAL;
								else if(strcmp(token->string, "greaterOrEqual")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
								else if(strcmp(token->string, "always")==0)
									pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_ALWAYS;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "depthBoundsTest")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.depthBoundsTest=VK_TRUE;
								else
									pipeline->pipeline.depthBoundsTest=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "stencilTest")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.stencilTest=VK_TRUE;
								else
									pipeline->pipeline.stencilTest=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "minDepthBounds")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.minDepthBounds=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.minDepthBounds=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "maxDepthBounds")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.maxDepthBounds=0.0f;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.maxDepthBounds=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilFailOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilPassOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilDepthFailOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilCompareOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_NEVER;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "never")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_NEVER;
								else if(strcmp(token->string, "less")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_LESS;
								else if(strcmp(token->string, "equal")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_EQUAL;
								else if(strcmp(token->string, "lessOrEqual")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
								else if(strcmp(token->string, "greater")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_GREATER;
								else if(strcmp(token->string, "notEqual")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_NOT_EQUAL;
								else if(strcmp(token->string, "greaterOrEqual")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
								else if(strcmp(token->string, "always")==0)
									pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_ALWAYS;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilCompareMask")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilCompareMask=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.frontStencilCompareMask=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilWriteMask")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilWriteMask=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.frontStencilWriteMask=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "frontStencilReference")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.frontStencilReference=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.frontStencilReference=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilFailOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilPassOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilDepthFailOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_KEEP;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "keep")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_KEEP;
								else if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_ZERO;
								else if(strcmp(token->string, "replace")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_REPLACE;
								else if(strcmp(token->string, "incrementAndClamp")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
								else if(strcmp(token->string, "decrementAndClamp")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INVERT;
								else if(strcmp(token->string, "invcrementAndWrap")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
								else if(strcmp(token->string, "decrementAndWrap")==0)
									pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilCompareOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_NEVER;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "never")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_NEVER;
								else if(strcmp(token->string, "less")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_LESS;
								else if(strcmp(token->string, "equal")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_EQUAL;
								else if(strcmp(token->string, "lessOrEqual")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
								else if(strcmp(token->string, "greater")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_GREATER;
								else if(strcmp(token->string, "notEqual")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_NOT_EQUAL;
								else if(strcmp(token->string, "greaterOrEqual")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
								else if(strcmp(token->string, "always")==0)
									pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_ALWAYS;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilCompareMask")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilCompareMask=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.backStencilCompareMask=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilWriteMask")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilWriteMask=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.backStencilWriteMask=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "backStencilReference")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.backStencilReference=0;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
								pipeline->pipeline.backStencilReference=(uint32_t)token->ival;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "rasterizationSamples")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT)
							{
								if(token->ival==1)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
								else if(token->ival==2)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_2_BIT;
								else if(token->ival==4)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;
								else if(token->ival==8)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_8_BIT;
								else if(token->ival==16)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_16_BIT;
								else if(token->ival==32)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_32_BIT;
								else if(token->ival==64)
									pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_64_BIT;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "sampleShading")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.sampleShading=VK_TRUE;
								else
									pipeline->pipeline.sampleShading=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "minSampleShading")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_FLOAT)
								pipeline->pipeline.minSampleShading=(float)token->fval;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "sampleMask")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						Tokenizer_PrintToken("sampleMask not implemented! ", token);
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "alphaToCoverage")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.alphaToCoverage=VK_TRUE;
								else
									pipeline->pipeline.alphaToCoverage=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "alphaToOne")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.alphaToOne=VK_TRUE;
								else
									pipeline->pipeline.alphaToOne=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "blendLogicOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.blendLogicOp=VK_TRUE;
								else
									pipeline->pipeline.blendLogicOp=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "blendLogicOpState")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_CLEAR;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "clear")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_CLEAR;
								else if(strcmp(token->string, "and")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND;
								else if(strcmp(token->string, "andReverse")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND_REVERSE;
								else if(strcmp(token->string, "copy")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_COPY;
								else if(strcmp(token->string, "andInverted")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND_INVERTED;
								else if(strcmp(token->string, "nop")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NO_OP;
								else if(strcmp(token->string, "or")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR;
								else if(strcmp(token->string, "nor")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NOR;
								else if(strcmp(token->string, "equivalent")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_EQUIVALENT;
								else if(strcmp(token->string, "invert")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_INVERT;
								else if(strcmp(token->string, "orReverse")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR_REVERSE;
								else if(strcmp(token->string, "copyInverted")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_COPY_INVERTED;
								else if(strcmp(token->string, "orInverted")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR_INVERTED;
								else if(strcmp(token->string, "nand")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NAND;
								else if(strcmp(token->string, "set")==0)
									pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_SET;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "blend")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_BOOLEAN)
							{
								if(token->boolean)
									pipeline->pipeline.blend=VK_TRUE;
								else
									pipeline->pipeline.blend=VK_FALSE;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "srcColorBlendFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;
								else if(strcmp(token->string, "one")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
								else if(strcmp(token->string, "srcColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
								else if(strcmp(token->string, "oneMinusSrcColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
								else if(strcmp(token->string, "dstColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
								else if(strcmp(token->string, "oneMinusDstColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
								else if(strcmp(token->string, "srcAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
								else if(strcmp(token->string, "oneMinusSrcAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
								else if(strcmp(token->string, "dstAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
								else if(strcmp(token->string, "oneMinusDstAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
								else if(strcmp(token->string, "constantColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
								else if(strcmp(token->string, "oneMinusConstantColor")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
								else if(strcmp(token->string, "constantAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
								else if(strcmp(token->string, "oneMinusConstantAlpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
								else if(strcmp(token->string, "srcAlphaSaturate")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
								else if(strcmp(token->string, "src1Color")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
								else if(strcmp(token->string, "oneMinusSrc1Color")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
								else if(strcmp(token->string, "src1Alpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
								else if(strcmp(token->string, "oneMinusSrc1Alpha")==0)
									pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "dstColorBlendFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
								else if(strcmp(token->string, "one")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;
								else if(strcmp(token->string, "srcColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
								else if(strcmp(token->string, "oneMinusSrcColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
								else if(strcmp(token->string, "dstColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
								else if(strcmp(token->string, "oneMinusDstColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
								else if(strcmp(token->string, "srcAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
								else if(strcmp(token->string, "oneMinusSrcAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
								else if(strcmp(token->string, "dstAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
								else if(strcmp(token->string, "oneMinusDstAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
								else if(strcmp(token->string, "constantColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
								else if(strcmp(token->string, "oneMinusConstantColor")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
								else if(strcmp(token->string, "constantAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
								else if(strcmp(token->string, "oneMinusConstantAlpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
								else if(strcmp(token->string, "srcAlphaSaturate")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
								else if(strcmp(token->string, "src1Color")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
								else if(strcmp(token->string, "oneMinusSrc1Color")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
								else if(strcmp(token->string, "src1Alpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
								else if(strcmp(token->string, "oneMinusSrc1Alpha")==0)
									pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "colorBlendOp")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "add")==0)
									pipeline->pipeline.colorBlendOp=VK_BLEND_OP_ADD;
								else if(strcmp(token->string, "subtract")==0)
									pipeline->pipeline.colorBlendOp=VK_BLEND_OP_SUBTRACT;
								else if(strcmp(token->string, "reverseSubtract")==0)
									pipeline->pipeline.colorBlendOp=VK_BLEND_OP_REVERSE_SUBTRACT;
								else if(strcmp(token->string, "min")==0)
									pipeline->pipeline.colorBlendOp=VK_BLEND_OP_MIN;
								else if(strcmp(token->string, "max")==0)
									pipeline->pipeline.colorBlendOp=VK_BLEND_OP_MAX;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "srcAlphaBlendFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
								else if(strcmp(token->string, "one")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
								else if(strcmp(token->string, "srcColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
								else if(strcmp(token->string, "oneMinusSrcColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
								else if(strcmp(token->string, "dstColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
								else if(strcmp(token->string, "oneMinusDstColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
								else if(strcmp(token->string, "srcAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
								else if(strcmp(token->string, "oneMinusSrcAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
								else if(strcmp(token->string, "dstAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
								else if(strcmp(token->string, "oneMinusDstAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
								else if(strcmp(token->string, "constantColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
								else if(strcmp(token->string, "oneMinusConstantColor")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
								else if(strcmp(token->string, "constantAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
								else if(strcmp(token->string, "oneMinusConstantAlpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
								else if(strcmp(token->string, "srcAlphaSaturate")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
								else if(strcmp(token->string, "src1Color")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
								else if(strcmp(token->string, "oneMinusSrc1Color")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
								else if(strcmp(token->string, "src1Alpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
								else if(strcmp(token->string, "oneMinusSrc1Alpha")==0)
									pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "dstAlphaBlendFactor")==0)
					{
						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "zero")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
								else if(strcmp(token->string, "one")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
								else if(strcmp(token->string, "srcColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
								else if(strcmp(token->string, "oneMinusSrcColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
								else if(strcmp(token->string, "dstColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
								else if(strcmp(token->string, "oneMinusDstColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
								else if(strcmp(token->string, "srcAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
								else if(strcmp(token->string, "oneMinusSrcAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
								else if(strcmp(token->string, "dstAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
								else if(strcmp(token->string, "oneMinusDstAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
								else if(strcmp(token->string, "constantColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
								else if(strcmp(token->string, "oneMinusConstantColor")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
								else if(strcmp(token->string, "constantAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
								else if(strcmp(token->string, "oneMinusConstantAlpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
								else if(strcmp(token->string, "srcAlphaSaturate")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
								else if(strcmp(token->string, "src1Color")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
								else if(strcmp(token->string, "oneMinusSrc1Color")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
								else if(strcmp(token->string, "src1Alpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
								else if(strcmp(token->string, "oneMinusSrc1Alpha")==0)
									pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "alphaBlendOp")==0)
					{
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_ADD;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "add")==0)
									pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_ADD;
								else if(strcmp(token->string, "subtract")==0)
									pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_SUBTRACT;
								else if(strcmp(token->string, "reverseSubtract")==0)
									pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_REVERSE_SUBTRACT;
								else if(strcmp(token->string, "min")==0)
									pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_MIN;
								else if(strcmp(token->string, "max")==0)
									pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_MAX;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER&&token->string[0]==')')
								break;
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "colorWriteMask")==0)
					{
						pipeline->pipeline.colorWriteMask=0;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "none")==0)
									pipeline->pipeline.colorWriteMask=0;
								else if(strcmp(token->string, "colorR")==0)
									pipeline->pipeline.colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;
								else if(strcmp(token->string, "colorG")==0)
									pipeline->pipeline.colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;
								else if(strcmp(token->string, "colorB")==0)
									pipeline->pipeline.colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;
								else if(strcmp(token->string, "colorA")==0)
									pipeline->pipeline.colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER)
							{
								if(token->string[0]==')')
									break;
								else if(token->string[0]=='|')
									continue;
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}
						}
					}
					else if(token->type==TOKEN_KEYWORD&&strcmp(token->string, "pushConstant")==0)
					{
						uint32_t param=0;
						pipeline->pushConstant.offset=0;
						pipeline->pushConstant.size=0;
						pipeline->pushConstant.stageFlags=0;

						Zone_Free(zone, token);
						token=Tokenizer_GetNext(&tokenizer);

						while(!(token->type==TOKEN_DELIMITER&&token->string[0]==')'))
						{
							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_INT&&param==0)
								pipeline->pushConstant.offset=(uint32_t)token->ival;
							else if(token->type==TOKEN_INT&&param==1)
								pipeline->pushConstant.size=(uint32_t)token->ival;
							else if(token->type==TOKEN_STRING)
							{
								if(strcmp(token->string, "vertex")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_VERTEX_BIT;
								else if(strcmp(token->string, "tessellationControl")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
								else if(strcmp(token->string, "tessellationEvaluation")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
								else if(strcmp(token->string, "geometry")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_GEOMETRY_BIT;
								else if(strcmp(token->string, "fragment")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_FRAGMENT_BIT;
								else if(strcmp(token->string, "compute")==0&&param==2)
									pipeline->pushConstant.stageFlags|=VK_SHADER_STAGE_COMPUTE_BIT;
								else
								{
									Tokenizer_PrintToken("Unknown parameter ", token);
									return false;
								}
							}
							else
							{
								Tokenizer_PrintToken("Unexpected token ", token);
								return false;
							}

							Zone_Free(zone, token);
							token=Tokenizer_GetNext(&tokenizer);

							if(token->type==TOKEN_DELIMITER)
							{
								// Still expecting more parameters, error.
								if(token->string[0]!=','&&param<2)
								{
									DBGPRINTF(DEBUG_ERROR, "Missing comma\n");
									return false;
								}
								// End of expected parameters, end.
								else if(token->string[0]==')'&&param==2)
									break;
								// Special case for 3rd parameter, because multiple stages can be specified.
								else if(token->string[0]!='|'&&param<2)
									param++;
							}

							if(param>2)
							{
								DBGPRINTF(DEBUG_ERROR, "Too many params pushConstant(offset, size, stage)\n");
								return false;
							}
						}
					}
				}
			}
		}

		if(token)
			Zone_Free(zone, token);
	}

	Zone_Free(zone, buffer);

	if(vkCreatePipelineLayout(context->device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=pipeline->descriptorSet.descriptorSetLayout?1u:0u,
		.pSetLayouts=&pipeline->descriptorSet.descriptorSetLayout,
		.pushConstantRangeCount=pipeline->pushConstant.size?1u:0u,
		.pPushConstantRanges=&pipeline->pushConstant,
	}, 0, &pipeline->pipelineLayout)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to create pipeline layout.\n");
		return false;
	}

	vkuPipeline_SetPipelineLayout(&pipeline->pipeline, pipeline->pipelineLayout);
	vkuPipeline_SetRenderPass(&pipeline->pipeline, renderPass);

	// Probably should do validation checks on this?
	if(rasterizationSamplesOverride<VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM)
		pipeline->pipeline.rasterizationSamples=rasterizationSamplesOverride;

	// If the first stage shader is compute, then this must be a compute pipeline
	if(pipeline->pipeline.stages[0].stage&VK_SHADER_STAGE_COMPUTE_BIT)
	{
		if(!vkuAssembleComputePipeline(&pipeline->pipeline, VK_NULL_HANDLE))
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to assemble compute pipe: %s", filename);
			return false;
		}
	}
	else
	{
		if(!vkuAssemblePipeline(&pipeline->pipeline, VK_NULL_HANDLE))
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to assemble pipe: %s", filename);
			return false;
		}
	}

	return true;
}

void DestroyPipeline(VkuContext_t *context, Pipeline_t *pipeline)
{
	if(pipeline->descriptorSet.descriptorSetLayout)
		vkDestroyDescriptorSetLayout(context->device, pipeline->descriptorSet.descriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyPipeline(context->device, pipeline->pipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(context->device, pipeline->pipelineLayout, VK_NULL_HANDLE);
}
