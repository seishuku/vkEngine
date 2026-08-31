#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "tokenizer.h"
#include "parser.h"
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
	fclose(stream);

	buffer[length]='\0';

	memset(pipeline, 0, sizeof(Pipeline_t));

	Tokenizer_t tokenizer;
	Tokenizer_Init(&tokenizer, length, buffer, sizeof(keywords)/sizeof(keywords[0]), keywords);
	
	Parser_t parser;
	Parser_Init(&parser, &tokenizer);

	while(!Parser_IsEnd(&parser))
	{
		// Start building up descriptor set layout ("descriptorSet { }")
		if(Parser_MatchKeyword(&parser, "descriptorSet"))
		{
			if(!vkuInitDescriptorSet(&pipeline->descriptorSet, context->device))
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to initialize descriptor set.\n");
				return false;
			}

			if(!Parser_Expect(&parser, TOKEN_DELIMITER, '{'))
				return false;

			// Next token must be a left brace '{'
			while(!Parser_Match(&parser, TOKEN_DELIMITER, '}'))
			{
				// Look for keyword tokens
				// Handle "addBinding" keyword
				if(Parser_ExpectKeyword(&parser, "addBinding"))
				{
					int64_t binding=0;
					VkDescriptorType type=0;
					char typeString[32]={ 0 };
					VkShaderStageFlags stage=0;
					char stageString[32]={ 0 };

					// First token should be a left parenthesis '('
					if(Parser_Expect(&parser, TOKEN_DELIMITER, '('))
					{
						if(!Parser_Integer(&parser, &binding))
							return false;

						if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
							return false;

						if(!Parser_String(&parser, typeString, sizeof(typeString)))
							return false;

						// Type parameter
						if(strcmp(typeString, "sampler")==0)
							type=VK_DESCRIPTOR_TYPE_SAMPLER;
						else if(strcmp(typeString, "combinedSampler")==0)
							type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						else if(strcmp(typeString, "sampledImage")==0)
							type=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
						else if(strcmp(typeString, "storageImage")==0)
							type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
						else if(strcmp(typeString, "uniformTexelBuffer")==0)
							type=VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
						else if(strcmp(typeString, "storageTexelBuffer")==0)
							type=VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
						else if(strcmp(typeString, "uniformBuffer")==0)
							type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
						else if(strcmp(typeString, "storageBuffer")==0)
							type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
						else if(strcmp(typeString, "uniformBufferDynamic")==0)
							type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
						else if(strcmp(typeString, "storageBufferDynamic")==0)
							type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
						else if(strcmp(typeString, "inputAttachment")==0)
							type=VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

						if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
							return false;

						for(;;)
						{
							if(!Parser_String(&parser, stageString, sizeof(stageString)))
								return false;

							if(strcmp(stageString, "vertex")==0)
								stage|=VK_SHADER_STAGE_VERTEX_BIT;
							else if(strcmp(stageString, "tessellationControl")==0)
								stage|=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
							else if(strcmp(stageString, "tessellationEvaluation")==0)
								stage|=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
							else if(strcmp(stageString, "geometry")==0)
								stage|=VK_SHADER_STAGE_GEOMETRY_BIT;
							else if(strcmp(stageString, "fragment")==0)
								stage|=VK_SHADER_STAGE_FRAGMENT_BIT;
							else if(strcmp(stageString, "compute")==0)
								stage|=VK_SHADER_STAGE_COMPUTE_BIT;
							else
							{
								DBGPRINTF(DEBUG_ERROR, "Unknown shader stage '%s'.\n", stageString);
								return false;
							}

							if(!Parser_Match(&parser, TOKEN_DELIMITER, '|'))
								break;
						}

						if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
							return false;

						if(!vkuDescriptorSet_AddBinding(&pipeline->descriptorSet, (uint32_t)binding, type, stage))
						{
							DBGPRINTF(DEBUG_ERROR, "Unable to add binding.\n");
							return false;
						}
					}
					else
						return false;
				}
				else
					return false;
			}

			if(!vkuAssembleDescriptorSetLayout(&pipeline->descriptorSet))
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to assemble descriptor set layout.\n");
				return false;
			}
		}
		// Start building up pipeline ("pipeline { }")
		else if(Parser_MatchKeyword(&parser, "pipeline"))
		{
			if(!vkuInitPipeline(&pipeline->pipeline, context->device, context->pipelineCache))
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to initialize pipeline.\n");
				return false;
			}

			if(!Parser_Expect(&parser, TOKEN_DELIMITER, '{'))
				return false;

			while(!Parser_Match(&parser, TOKEN_DELIMITER, '}'))
			{
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

				if(Parser_MatchKeyword(&parser, "addStage"))
				{
					char shaderFilename[1025]={ 0 };
					uint8_t *shaderData=NULL;
					uint32_t shaderSize=0;
					char stageString[32]={ 0 };
					VkShaderStageFlagBits stage=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_MatchKeyword(&parser, "base64"))
					{
						Token_t *token=Tokenizer_GetNext(&tokenizer);

						if(token->type!=TOKEN_QUOTED)
						{
							Tokenizer_PrintToken("Unexpected token ", token);
							return false;
						}

						shaderData=Zone_Malloc(zone, strlen(token->string));

						if(shaderData==NULL)
							return false;

						shaderSize=base64Decode(token->string, shaderData);

						Zone_Free(zone, token);
					}
					else if(!Parser_String(&parser, shaderFilename, sizeof(shaderFilename)))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;
			
					if(Parser_String(&parser, stageString, sizeof(stageString)))
					{
						if(strcmp(stageString, "vertex")==0)
							stage=VK_SHADER_STAGE_VERTEX_BIT;
						else if(strcmp(stageString, "tessellationControl")==0)
							stage=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						else if(strcmp(stageString, "tessellationEvaluation")==0)
							stage=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						else if(strcmp(stageString, "geometry")==0)
							stage=VK_SHADER_STAGE_GEOMETRY_BIT;
						else if(strcmp(stageString, "fragment")==0)
							stage=VK_SHADER_STAGE_FRAGMENT_BIT;
						else if(strcmp(stageString, "compute")==0)
							stage=VK_SHADER_STAGE_COMPUTE_BIT;
						else
						{
							DBGPRINTF(DEBUG_ERROR, "Unknown shader stage '%s'.\n", stageString);
							return false;
						}
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

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
				else if(Parser_MatchKeyword(&parser, "addVertexBinding"))
				{
					int64_t binding=0;
					int64_t stride=0;
					char inputRateString[32]={ 0 };
					VkVertexInputRate inputRate=VK_VERTEX_INPUT_RATE_VERTEX;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &binding))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_Integer(&parser, &stride))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_String(&parser, inputRateString, sizeof(inputRateString)))
						return false;

					if(strcmp(inputRateString, "perVertex")==0)
						inputRate=VK_VERTEX_INPUT_RATE_VERTEX;
					else if(strcmp(inputRateString, "perInstance")==0)
						inputRate=VK_VERTEX_INPUT_RATE_INSTANCE;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown vertex input rate '%s'.\n", inputRateString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					if(!vkuPipeline_AddVertexBinding(&pipeline->pipeline, binding, stride, inputRate))
					{
						DBGPRINTF(DEBUG_ERROR, "Unable to add vertex binding.\n");
						return false;
					}
				}
				else if(Parser_MatchKeyword(&parser, "addVertexAttribute"))
				{
					int64_t location=0;
					int64_t binding=0;
					char formatString[32]={ 0 };
					VkFormat format=VK_FORMAT_UNDEFINED;
					int64_t offset=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &location))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_Integer(&parser, &binding))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_String(&parser, formatString, sizeof(formatString)))
						return false;

					if(strcmp(formatString, "r8_unorm")==0)
						format=VK_FORMAT_R8_UNORM;
					else if(strcmp(formatString, "r8_snorm")==0)
						format=VK_FORMAT_R8_SNORM;
					else if(strcmp(formatString, "r8_uint")==0)
						format=VK_FORMAT_R8_UINT;
					else if(strcmp(formatString, "r8_sint")==0)
						format=VK_FORMAT_R8_SINT;
					else if(strcmp(formatString, "rg8_unorm")==0)
						format=VK_FORMAT_R8G8_UNORM;
					else if(strcmp(formatString, "rg8_snorm")==0)
						format=VK_FORMAT_R8G8_SNORM;
					else if(strcmp(formatString, "rg8_uint")==0)
						format=VK_FORMAT_R8G8_UINT;
					else if(strcmp(formatString, "rg8_sint")==0)
						format=VK_FORMAT_R8G8_SINT;
					else if(strcmp(formatString, "rgb8_unorm")==0)
						format=VK_FORMAT_R8G8B8_UNORM;
					else if(strcmp(formatString, "rgb8_snorm")==0)
						format=VK_FORMAT_R8G8B8_SNORM;
					else if(strcmp(formatString, "rgb8_uint")==0)
						format=VK_FORMAT_R8G8B8_UINT;
					else if(strcmp(formatString, "rgb8_sint")==0)
						format=VK_FORMAT_R8G8B8_SINT;
					else if(strcmp(formatString, "bgr8_unorm")==0)
						format=VK_FORMAT_B8G8R8_UNORM;
					else if(strcmp(formatString, "bgr8_snorm")==0)
						format=VK_FORMAT_B8G8R8_SNORM;
					else if(strcmp(formatString, "bgr8_uint")==0)
						format=VK_FORMAT_B8G8R8_UINT;
					else if(strcmp(formatString, "bgr8_sint")==0)
						format=VK_FORMAT_B8G8R8_SINT;
					else if(strcmp(formatString, "rgba8_unorm")==0)
						format=VK_FORMAT_R8G8B8A8_UNORM;
					else if(strcmp(formatString, "rgba8_snorm")==0)
						format=VK_FORMAT_R8G8B8A8_SNORM;
					else if(strcmp(formatString, "rgba8_uint")==0)
						format=VK_FORMAT_R8G8B8A8_UINT;
					else if(strcmp(formatString, "rgba8_sint")==0)
						format=VK_FORMAT_R8G8B8A8_SINT;
					else if(strcmp(formatString, "bgra8_unorm")==0)
						format=VK_FORMAT_B8G8R8A8_UNORM;
					else if(strcmp(formatString, "bgra8_snorm")==0)
						format=VK_FORMAT_B8G8R8A8_SNORM;
					else if(strcmp(formatString, "bgra8_uint")==0)
						format=VK_FORMAT_B8G8R8A8_UINT;
					else if(strcmp(formatString, "bgra8_sint")==0)
						format=VK_FORMAT_B8G8R8A8_SINT;
					else if(strcmp(formatString, "abgr8_unorm")==0)
						format=VK_FORMAT_A8B8G8R8_UNORM_PACK32;
					else if(strcmp(formatString, "abgr8_snorm")==0)
						format=VK_FORMAT_A8B8G8R8_SNORM_PACK32;
					else if(strcmp(formatString, "abgr8_uint")==0)
						format=VK_FORMAT_A8B8G8R8_UINT_PACK32;
					else if(strcmp(formatString, "abgr8_sint")==0)
						format=VK_FORMAT_A8B8G8R8_SINT_PACK32;
					else if(strcmp(formatString, "a2r10g10b10_unorm")==0)
						format=VK_FORMAT_A2R10G10B10_UNORM_PACK32;
					else if(strcmp(formatString, "a2r10g10b10_snorm")==0)
						format=VK_FORMAT_A2R10G10B10_SNORM_PACK32;
					else if(strcmp(formatString, "a2r10g10b10_uint")==0)
						format=VK_FORMAT_A2R10G10B10_UINT_PACK32;
					else if(strcmp(formatString, "a2r10g10b10_sint")==0)
						format=VK_FORMAT_A2R10G10B10_SINT_PACK32;
					else if(strcmp(formatString, "a2b10g10r10_unorm")==0)
						format=VK_FORMAT_A2B10G10R10_UNORM_PACK32;
					else if(strcmp(formatString, "a2b10g10r10_snorm")==0)
						format=VK_FORMAT_A2B10G10R10_SNORM_PACK32;
					else if(strcmp(formatString, "a2b10g10r10_uint")==0)
						format=VK_FORMAT_A2B10G10R10_UINT_PACK32;
					else if(strcmp(formatString, "a2b10g10r10_sint")==0)
						format=VK_FORMAT_A2B10G10R10_SINT_PACK32;
					else if(strcmp(formatString, "r16_unorm")==0)
						format=VK_FORMAT_R16_UNORM;
					else if(strcmp(formatString, "r16_snorm")==0)
						format=VK_FORMAT_R16_SNORM;
					else if(strcmp(formatString, "r16_uint")==0)
						format=VK_FORMAT_R16_UINT;
					else if(strcmp(formatString, "r16_sint")==0)
						format=VK_FORMAT_R16_SINT;
					else if(strcmp(formatString, "r16_sfloat")==0)
						format=VK_FORMAT_R16_SFLOAT;
					else if(strcmp(formatString, "rg16_unorm")==0)
						format=VK_FORMAT_R16G16_UNORM;
					else if(strcmp(formatString, "rg16_snorm")==0)
						format=VK_FORMAT_R16G16_SNORM;
					else if(strcmp(formatString, "rg16_uint")==0)
						format=VK_FORMAT_R16G16_UINT;
					else if(strcmp(formatString, "rg16_sint")==0)
						format=VK_FORMAT_R16G16_SINT;
					else if(strcmp(formatString, "rg16_sfloat")==0)
						format=VK_FORMAT_R16G16_SFLOAT;
					else if(strcmp(formatString, "rgb16_unorm")==0)
						format=VK_FORMAT_R16G16B16_UNORM;
					else if(strcmp(formatString, "rgb16_snorm")==0)
						format=VK_FORMAT_R16G16B16_SNORM;
					else if(strcmp(formatString, "rgb16_uint")==0)
						format=VK_FORMAT_R16G16B16_UINT;
					else if(strcmp(formatString, "rgb16_sint")==0)
						format=VK_FORMAT_R16G16B16_SINT;
					else if(strcmp(formatString, "rgb16_sfloat")==0)
						format=VK_FORMAT_R16G16B16_SFLOAT;
					else if(strcmp(formatString, "rgba16_unorm")==0)
						format=VK_FORMAT_R16G16B16A16_UNORM;
					else if(strcmp(formatString, "rgba16_snorm")==0)
						format=VK_FORMAT_R16G16B16A16_SNORM;
					else if(strcmp(formatString, "rgba16_uint")==0)
						format=VK_FORMAT_R16G16B16A16_UINT;
					else if(strcmp(formatString, "rgba16_sint")==0)
						format=VK_FORMAT_R16G16B16A16_SINT;
					else if(strcmp(formatString, "rgba16_sfloat")==0)
						format=VK_FORMAT_R16G16B16A16_SFLOAT;
					else if(strcmp(formatString, "r32_uint")==0)
						format=VK_FORMAT_R32_UINT;
					else if(strcmp(formatString, "r32_sint")==0)
						format=VK_FORMAT_R32_SINT;
					else if(strcmp(formatString, "r32_sfloat")==0)
						format=VK_FORMAT_R32_SFLOAT;
					else if(strcmp(formatString, "rg32_uint")==0)
						format=VK_FORMAT_R32G32_UINT;
					else if(strcmp(formatString, "rg32_sint")==0)
						format=VK_FORMAT_R32G32_SINT;
					else if(strcmp(formatString, "rg32_sfloat")==0)
						format=VK_FORMAT_R32G32_SFLOAT;
					else if(strcmp(formatString, "rgb32_uint")==0)
						format=VK_FORMAT_R32G32B32_UINT;
					else if(strcmp(formatString, "rgb32_sint")==0)
						format=VK_FORMAT_R32G32B32_SINT;
					else if(strcmp(formatString, "rgb32_sfloat")==0)
						format=VK_FORMAT_R32G32B32_SFLOAT;
					else if(strcmp(formatString, "rgba32_uint")==0)
						format=VK_FORMAT_R32G32B32A32_UINT;
					else if(strcmp(formatString, "rgba32_sint")==0)
						format=VK_FORMAT_R32G32B32A32_SINT;
					else if(strcmp(formatString, "rgba32_sfloat")==0)
						format=VK_FORMAT_R32G32B32A32_SFLOAT;
					else if(strcmp(formatString, "r64_uint")==0)
						format=VK_FORMAT_R64_UINT;
					else if(strcmp(formatString, "r64_sint")==0)
						format=VK_FORMAT_R64_SINT;
					else if(strcmp(formatString, "r64_sfloat")==0)
						format=VK_FORMAT_R64_SFLOAT;
					else if(strcmp(formatString, "rg64_uint")==0)
						format=VK_FORMAT_R64G64_UINT;
					else if(strcmp(formatString, "rg64_sint")==0)
						format=VK_FORMAT_R64G64_SINT;
					else if(strcmp(formatString, "rg64_sfloat")==0)
						format=VK_FORMAT_R64G64_SFLOAT;
					else if(strcmp(formatString, "rgb64_uint")==0)
						format=VK_FORMAT_R64G64B64_UINT;
					else if(strcmp(formatString, "rgb64_sint")==0)
						format=VK_FORMAT_R64G64B64_SINT;
					else if(strcmp(formatString, "rgb64_sfloat")==0)
						format=VK_FORMAT_R64G64B64_SFLOAT;
					else if(strcmp(formatString, "rgba64_uint")==0)
						format=VK_FORMAT_R64G64B64A64_UINT;
					else if(strcmp(formatString, "rgba64_sint")==0)
						format=VK_FORMAT_R64G64B64A64_SINT;
					else if(strcmp(formatString, "rgba64_sfloat")==0)
						format=VK_FORMAT_R64G64B64A64_SFLOAT;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown vertex attribute format '%s'.\n", formatString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;


					if(!Parser_Integer(&parser, &offset))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					if(!vkuPipeline_AddVertexAttribute(&pipeline->pipeline, location, binding, format, offset))
					{
						DBGPRINTF(DEBUG_ERROR, "Unable to add vertex binding.\n");
						return false;
					}
				}
				else if(Parser_MatchKeyword(&parser, "subpass"))
				{
					int64_t subpass=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &subpass))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.subpass=(uint32_t)subpass;
				}
				else if(Parser_MatchKeyword(&parser, "topology"))
				{
					char topologyString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, topologyString, sizeof(topologyString)))
						return false;

					if(strcmp(topologyString, "pointList")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
					else if(strcmp(topologyString, "lineList")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
					else if(strcmp(topologyString, "lineStrip")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
					else if(strcmp(topologyString, "triangleList")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
					else if(strcmp(topologyString, "triangleStrip")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
					else if(strcmp(topologyString, "triangleFan")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
					else if(strcmp(topologyString, "listListAdjacency")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
					else if(strcmp(topologyString, "listStripAdjacency")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
					else if(strcmp(topologyString, "triangleListAdjacency")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
					else if(strcmp(topologyString, "triangleStripAdjacency")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
					else if(strcmp(topologyString, "patchList")==0)
						pipeline->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown topology '%s'.\n", topologyString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "primitiveRestart"))
				{
					bool primitiveRestart=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &primitiveRestart))
						pipeline->pipeline.primitiveRestart=primitiveRestart?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthClamp"))
				{
					bool depthClamp=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Boolean(&parser, &depthClamp))
						return false;
					else
						pipeline->pipeline.depthClamp=depthClamp?VK_TRUE:VK_FALSE;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "rasterizerDiscard"))
				{
					bool rasterizerDiscard=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Boolean(&parser, &rasterizerDiscard))
						return false;
					else
						pipeline->pipeline.rasterizerDiscard=rasterizerDiscard?VK_TRUE:VK_FALSE;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "polygonMode"))
				{
					char polygonModeString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, polygonModeString, sizeof(polygonModeString)))
						return false;

					if(strcmp(polygonModeString, "fill")==0)
						pipeline->pipeline.polygonMode=VK_POLYGON_MODE_FILL;
					else if(strcmp(polygonModeString, "line")==0)
						pipeline->pipeline.polygonMode=VK_POLYGON_MODE_LINE;
					else if(strcmp(polygonModeString, "point")==0)
						pipeline->pipeline.polygonMode=VK_POLYGON_MODE_POINT;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown polygonMode '%s'.\n", polygonModeString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "cullMode"))
				{
					char cullModeString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, cullModeString, sizeof(cullModeString)))
						return false;

					if(strcmp(cullModeString, "none")==0)
						pipeline->pipeline.cullMode=VK_CULL_MODE_NONE;
					else if(strcmp(cullModeString, "front")==0)
						pipeline->pipeline.cullMode=VK_CULL_MODE_FRONT_BIT;
					else if(strcmp(cullModeString, "back")==0)
						pipeline->pipeline.cullMode=VK_CULL_MODE_BACK_BIT;
					else if(strcmp(cullModeString, "frontAndBack")==0)
						pipeline->pipeline.cullMode=VK_CULL_MODE_FRONT_AND_BACK;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown cullMode '%s'.\n", cullModeString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "frontFace"))
				{
					char frontFaceString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, frontFaceString, sizeof(frontFaceString)))
						return false;

					if(strcmp(frontFaceString, "ccw")==0)
						pipeline->pipeline.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
					else if(strcmp(frontFaceString, "cw")==0)
						pipeline->pipeline.frontFace=VK_FRONT_FACE_CLOCKWISE;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown frontFace '%s'.\n", frontFaceString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthBias"))
				{
					bool depthBias=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &depthBias))
						pipeline->pipeline.depthBias=depthBias?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthBiasConstantFactor"))
				{
					double depthBiasConstantFactor=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &depthBiasConstantFactor))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.depthBiasConstantFactor=(float)depthBiasConstantFactor;
				}
				else if(Parser_MatchKeyword(&parser, "depthBiasClamp"))
				{
					double depthBiasClamp=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &depthBiasClamp))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.depthBiasClamp=(float)depthBiasClamp;
				}
				else if(Parser_MatchKeyword(&parser, "depthBiasSlopeFactor"))
				{
					double depthBiasSlopeFactor=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &depthBiasSlopeFactor))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.depthBiasSlopeFactor=(float)depthBiasSlopeFactor;
				}
				else if(Parser_MatchKeyword(&parser, "lineWidth"))
				{
					double lineWidth=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &lineWidth))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.lineWidth=(float)lineWidth;
				}
				else if(Parser_MatchKeyword(&parser, "depthTest"))
				{
					bool depthTest=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &depthTest))
						pipeline->pipeline.depthTest=depthTest?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthWrite"))
				{
					bool depthWrite=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &depthWrite))
						pipeline->pipeline.depthWrite=depthWrite?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthCompareOp"))
				{
					char depthCompareOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, depthCompareOpString, sizeof(depthCompareOpString)))
						return false;

					if(strcmp(depthCompareOpString, "never")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_NEVER;
					else if(strcmp(depthCompareOpString, "less")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_LESS;
					else if(strcmp(depthCompareOpString, "equal")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_EQUAL;
					else if(strcmp(depthCompareOpString, "lessOrEqual")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
					else if(strcmp(depthCompareOpString, "greater")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_GREATER;
					else if(strcmp(depthCompareOpString, "notEqual")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_NOT_EQUAL;
					else if(strcmp(depthCompareOpString, "greaterOrEqual")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
					else if(strcmp(depthCompareOpString, "always")==0)
						pipeline->pipeline.depthCompareOp=VK_COMPARE_OP_ALWAYS;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown depthCompareOp '%s'.\n", depthCompareOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "depthBoundsTest"))
				{
					bool depthBoundsTest=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &depthBoundsTest))
						pipeline->pipeline.depthBoundsTest=depthBoundsTest?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "stencilTest"))
				{
					bool stencilTest=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &stencilTest))
						pipeline->pipeline.stencilTest=stencilTest?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "minDepthBounds"))
				{
					double minDepthBounds=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &minDepthBounds))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.minDepthBounds=(float)minDepthBounds;
				}
				else if(Parser_MatchKeyword(&parser, "maxDepthBounds"))
				{
					double maxDepthBounds=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &maxDepthBounds))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.maxDepthBounds=(float)maxDepthBounds;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilFailOp"))
				{
					char frontStencilFailOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, frontStencilFailOpString, sizeof(frontStencilFailOpString)))
						return false;

					if(strcmp(frontStencilFailOpString, "keep")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(frontStencilFailOpString, "zero")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(frontStencilFailOpString, "replace")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(frontStencilFailOpString, "incrementAndClamp")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(frontStencilFailOpString, "decrementAndClamp")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(frontStencilFailOpString, "invert")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(frontStencilFailOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(frontStencilFailOpString, "decrementAndWrap")==0)
						pipeline->pipeline.frontStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown frontStencilFailOp '%s'.\n", frontStencilFailOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilPassOp"))
				{
					char frontStencilPassOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, frontStencilPassOpString, sizeof(frontStencilPassOpString)))
						return false;

					if(strcmp(frontStencilPassOpString, "keep")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(frontStencilPassOpString, "zero")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(frontStencilPassOpString, "replace")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(frontStencilPassOpString, "incrementAndClamp")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(frontStencilPassOpString, "decrementAndClamp")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(frontStencilPassOpString, "invert")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(frontStencilPassOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(frontStencilPassOpString, "decrementAndWrap")==0)
						pipeline->pipeline.frontStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown frontStencilPassOp '%s'.\n", frontStencilPassOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilDepthFailOp"))
				{
					char frontStencilDepthFailOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, frontStencilDepthFailOpString, sizeof(frontStencilDepthFailOpString)))
						return false;

					if(strcmp(frontStencilDepthFailOpString, "keep")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(frontStencilDepthFailOpString, "zero")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(frontStencilDepthFailOpString, "replace")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(frontStencilDepthFailOpString, "incrementAndClamp")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(frontStencilDepthFailOpString, "decrementAndClamp")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(frontStencilDepthFailOpString, "invert")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(frontStencilDepthFailOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(frontStencilDepthFailOpString, "decrementAndWrap")==0)
						pipeline->pipeline.frontStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown frontStencilDepthFailOp '%s'.\n", frontStencilDepthFailOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilCompareOp"))
				{
					char frontStencilCompareOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, frontStencilCompareOpString, sizeof(frontStencilCompareOpString)))
						return false;

					if(strcmp(frontStencilCompareOpString, "never")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_NEVER;
					else if(strcmp(frontStencilCompareOpString, "less")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_LESS;
					else if(strcmp(frontStencilCompareOpString, "equal")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_EQUAL;
					else if(strcmp(frontStencilCompareOpString, "lessOrEqual")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
					else if(strcmp(frontStencilCompareOpString, "greater")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_GREATER;
					else if(strcmp(frontStencilCompareOpString, "notEqual")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_NOT_EQUAL;
					else if(strcmp(frontStencilCompareOpString, "greaterOrEqual")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
					else if(strcmp(frontStencilCompareOpString, "always")==0)
						pipeline->pipeline.frontStencilCompareOp=VK_COMPARE_OP_ALWAYS;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown frontStencilCompareOp '%s'.\n", frontStencilCompareOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilCompareMask"))
				{
					int64_t frontStencilCompareMask=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &frontStencilCompareMask))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.frontStencilCompareMask=(uint32_t)frontStencilCompareMask;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilWriteMask"))
				{
					int64_t frontStencilWriteMask=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &frontStencilWriteMask))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.frontStencilWriteMask=(uint32_t)frontStencilWriteMask;
				}
				else if(Parser_MatchKeyword(&parser, "frontStencilReference"))
				{
					int64_t frontStencilReference=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &frontStencilReference))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.frontStencilReference=(uint32_t)frontStencilReference;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilFailOp"))
				{
					char backStencilFailOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, backStencilFailOpString, sizeof(backStencilFailOpString)))
						return false;

					if(strcmp(backStencilFailOpString, "keep")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(backStencilFailOpString, "zero")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(backStencilFailOpString, "replace")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(backStencilFailOpString, "incrementAndClamp")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(backStencilFailOpString, "decrementAndClamp")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(backStencilFailOpString, "invert")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(backStencilFailOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(backStencilFailOpString, "decrementAndWrap")==0)
						pipeline->pipeline.backStencilFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown backStencilFailOp '%s'.\n", backStencilFailOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilPassOp"))
				{
					char backStencilPassOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, backStencilPassOpString, sizeof(backStencilPassOpString)))
						return false;

					if(strcmp(backStencilPassOpString, "keep")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(backStencilPassOpString, "zero")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(backStencilPassOpString, "replace")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(backStencilPassOpString, "incrementAndClamp")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(backStencilPassOpString, "decrementAndClamp")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(backStencilPassOpString, "invert")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(backStencilPassOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(backStencilPassOpString, "decrementAndWrap")==0)
						pipeline->pipeline.backStencilPassOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown backStencilPassOp '%s'.\n", backStencilPassOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilDepthFailOp"))
				{
					char backStencilDepthFailOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, backStencilDepthFailOpString, sizeof(backStencilDepthFailOpString)))
						return false;

					if(strcmp(backStencilDepthFailOpString, "keep")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_KEEP;
					else if(strcmp(backStencilDepthFailOpString, "zero")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_ZERO;
					else if(strcmp(backStencilDepthFailOpString, "replace")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_REPLACE;
					else if(strcmp(backStencilDepthFailOpString, "incrementAndClamp")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_CLAMP;
					else if(strcmp(backStencilDepthFailOpString, "decrementAndClamp")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_CLAMP;
					else if(strcmp(backStencilDepthFailOpString, "invert")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INVERT;
					else if(strcmp(backStencilDepthFailOpString, "invcrementAndWrap")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_INCREMENT_AND_WRAP;
					else if(strcmp(backStencilDepthFailOpString, "decrementAndWrap")==0)
						pipeline->pipeline.backStencilDepthFailOp=VK_STENCIL_OP_DECREMENT_AND_WRAP;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown backStencilDepthFailOp '%s'.\n", backStencilDepthFailOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilCompareOp"))
				{
					char backStencilCompareOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, backStencilCompareOpString, sizeof(backStencilCompareOpString)))
						return false;

					if(strcmp(backStencilCompareOpString, "never")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_NEVER;
					else if(strcmp(backStencilCompareOpString, "less")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_LESS;
					else if(strcmp(backStencilCompareOpString, "equal")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_EQUAL;
					else if(strcmp(backStencilCompareOpString, "lessOrEqual")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL;
					else if(strcmp(backStencilCompareOpString, "greater")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_GREATER;
					else if(strcmp(backStencilCompareOpString, "notEqual")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_NOT_EQUAL;
					else if(strcmp(backStencilCompareOpString, "greaterOrEqual")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
					else if(strcmp(backStencilCompareOpString, "always")==0)
						pipeline->pipeline.backStencilCompareOp=VK_COMPARE_OP_ALWAYS;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown backStencilCompareOp '%s'.\n", backStencilCompareOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilCompareMask"))
				{
					int64_t backStencilCompareMask=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &backStencilCompareMask))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.backStencilCompareMask=(uint32_t)backStencilCompareMask;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilWriteMask"))
				{
					int64_t backStencilWriteMask=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &backStencilWriteMask))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.backStencilWriteMask=(uint32_t)backStencilWriteMask;
				}
				else if(Parser_MatchKeyword(&parser, "backStencilReference"))
				{
					int64_t backStencilReference=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &backStencilReference))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.backStencilReference=(uint32_t)backStencilReference;
				}
				else if(Parser_MatchKeyword(&parser, "rasterizationSamples"))
				{
					int64_t rasterizationSamples=1;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &rasterizationSamples))
						return false;

					switch(rasterizationSamples)
					{
						case 1:  pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;  break;
						case 2:  pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_2_BIT;  break;
						case 4:  pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_4_BIT;  break;
						case 8:  pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_8_BIT;  break;
						case 16: pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_16_BIT; break;
						case 32: pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_32_BIT; break;
						case 64: pipeline->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_64_BIT; break;

						default:
							DBGPRINTF(DEBUG_ERROR, "Unknown rasterizationSamples value %lld.\n", (long long)rasterizationSamples);
							return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "sampleShading"))
				{
					bool sampleShading=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &sampleShading))
						pipeline->pipeline.sampleShading=sampleShading?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "minSampleShading"))
				{
					double minSampleShading=0.0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Float(&parser, &minSampleShading))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pipeline.minSampleShading=(float)minSampleShading;
				}
				else if(Parser_MatchKeyword(&parser, "sampleMask"))
				{
					DBGPRINTF(DEBUG_ERROR, "sampleMask not implemented, skipping.\n");

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					// TODO: not implemented yet -- just consume tokens up to the closing ')'
					// so the rest of the file still parses correctly instead of desyncing.
					while(!Parser_Match(&parser, TOKEN_DELIMITER, ')'))
					{
						Token_t *skip=Tokenizer_GetNext(&tokenizer);

						if(skip==NULL)
						{
							DBGPRINTF(DEBUG_ERROR, "Unexpected end of file inside sampleMask(...).\n");
							return false;
						}

						Zone_Free(zone, skip);
					}
				}
				else if(Parser_MatchKeyword(&parser, "alphaToCoverage"))
				{
					bool alphaToCoverage=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &alphaToCoverage))
						pipeline->pipeline.alphaToCoverage=alphaToCoverage?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "alphaToOne"))
				{
					bool alphaToOne=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &alphaToOne))
						pipeline->pipeline.alphaToOne=alphaToOne?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "blendLogicOp"))
				{
					bool blendLogicOp=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &blendLogicOp))
						pipeline->pipeline.blendLogicOp=blendLogicOp?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "blendLogicOpState"))
				{
					char blendLogicOpStateString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, blendLogicOpStateString, sizeof(blendLogicOpStateString)))
						return false;

					if(strcmp(blendLogicOpStateString, "clear")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_CLEAR;
					else if(strcmp(blendLogicOpStateString, "and")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND;
					else if(strcmp(blendLogicOpStateString, "andReverse")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND_REVERSE;
					else if(strcmp(blendLogicOpStateString, "copy")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_COPY;
					else if(strcmp(blendLogicOpStateString, "andInverted")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_AND_INVERTED;
					else if(strcmp(blendLogicOpStateString, "nop")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NO_OP;
					else if(strcmp(blendLogicOpStateString, "or")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR;
					else if(strcmp(blendLogicOpStateString, "nor")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NOR;
					else if(strcmp(blendLogicOpStateString, "equivalent")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_EQUIVALENT;
					else if(strcmp(blendLogicOpStateString, "invert")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_INVERT;
					else if(strcmp(blendLogicOpStateString, "orReverse")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR_REVERSE;
					else if(strcmp(blendLogicOpStateString, "copyInverted")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_COPY_INVERTED;
					else if(strcmp(blendLogicOpStateString, "orInverted")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_OR_INVERTED;
					else if(strcmp(blendLogicOpStateString, "nand")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_NAND;
					else if(strcmp(blendLogicOpStateString, "set")==0)
						pipeline->pipeline.blendLogicOpState=VK_LOGIC_OP_SET;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown blendLogicOpState '%s'.\n", blendLogicOpStateString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "blend"))
				{
					bool blend=false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(Parser_Boolean(&parser, &blend))
						pipeline->pipeline.blend=blend?VK_TRUE:VK_FALSE;
					else
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "srcColorBlendFactor"))
				{
					char srcColorBlendFactorString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, srcColorBlendFactorString, sizeof(srcColorBlendFactorString)))
						return false;

					if(strcmp(srcColorBlendFactorString, "zero")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ZERO;
					else if(strcmp(srcColorBlendFactorString, "one")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
					else if(strcmp(srcColorBlendFactorString, "srcColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
					else if(strcmp(srcColorBlendFactorString, "oneMinusSrcColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
					else if(strcmp(srcColorBlendFactorString, "dstColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
					else if(strcmp(srcColorBlendFactorString, "oneMinusDstColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
					else if(strcmp(srcColorBlendFactorString, "srcAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "oneMinusSrcAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "dstAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "oneMinusDstAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "constantColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
					else if(strcmp(srcColorBlendFactorString, "oneMinusConstantColor")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
					else if(strcmp(srcColorBlendFactorString, "constantAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "oneMinusConstantAlpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "srcAlphaSaturate")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
					else if(strcmp(srcColorBlendFactorString, "src1Color")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
					else if(strcmp(srcColorBlendFactorString, "oneMinusSrc1Color")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
					else if(strcmp(srcColorBlendFactorString, "src1Alpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
					else if(strcmp(srcColorBlendFactorString, "oneMinusSrc1Alpha")==0)
						pipeline->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown srcColorBlendFactor '%s'.\n", srcColorBlendFactorString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "dstColorBlendFactor"))
				{
					char dstColorBlendFactorString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, dstColorBlendFactorString, sizeof(dstColorBlendFactorString)))
						return false;

					if(strcmp(dstColorBlendFactorString, "zero")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
					else if(strcmp(dstColorBlendFactorString, "one")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;
					else if(strcmp(dstColorBlendFactorString, "srcColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
					else if(strcmp(dstColorBlendFactorString, "oneMinusSrcColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
					else if(strcmp(dstColorBlendFactorString, "dstColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
					else if(strcmp(dstColorBlendFactorString, "oneMinusDstColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
					else if(strcmp(dstColorBlendFactorString, "srcAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "oneMinusSrcAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "dstAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "oneMinusDstAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "constantColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
					else if(strcmp(dstColorBlendFactorString, "oneMinusConstantColor")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
					else if(strcmp(dstColorBlendFactorString, "constantAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "oneMinusConstantAlpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "srcAlphaSaturate")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
					else if(strcmp(dstColorBlendFactorString, "src1Color")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
					else if(strcmp(dstColorBlendFactorString, "oneMinusSrc1Color")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
					else if(strcmp(dstColorBlendFactorString, "src1Alpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
					else if(strcmp(dstColorBlendFactorString, "oneMinusSrc1Alpha")==0)
						pipeline->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown dstColorBlendFactor '%s'.\n", dstColorBlendFactorString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "colorBlendOp"))
				{
					char colorBlendOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, colorBlendOpString, sizeof(colorBlendOpString)))
						return false;

					if(strcmp(colorBlendOpString, "add")==0)
						pipeline->pipeline.colorBlendOp=VK_BLEND_OP_ADD;
					else if(strcmp(colorBlendOpString, "subtract")==0)
						pipeline->pipeline.colorBlendOp=VK_BLEND_OP_SUBTRACT;
					else if(strcmp(colorBlendOpString, "reverseSubtract")==0)
						pipeline->pipeline.colorBlendOp=VK_BLEND_OP_REVERSE_SUBTRACT;
					else if(strcmp(colorBlendOpString, "min")==0)
						pipeline->pipeline.colorBlendOp=VK_BLEND_OP_MIN;
					else if(strcmp(colorBlendOpString, "max")==0)
						pipeline->pipeline.colorBlendOp=VK_BLEND_OP_MAX;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown colorBlendOp '%s'.\n", colorBlendOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "srcAlphaBlendFactor"))
				{
					char srcAlphaBlendFactorString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, srcAlphaBlendFactorString, sizeof(srcAlphaBlendFactorString)))
						return false;

					if(strcmp(srcAlphaBlendFactorString, "zero")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
					else if(strcmp(srcAlphaBlendFactorString, "one")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
					else if(strcmp(srcAlphaBlendFactorString, "srcColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusSrcColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "dstColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusDstColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "srcAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusSrcAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "dstAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusDstAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "constantColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusConstantColor")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "constantAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusConstantAlpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "srcAlphaSaturate")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
					else if(strcmp(srcAlphaBlendFactorString, "src1Color")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusSrc1Color")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
					else if(strcmp(srcAlphaBlendFactorString, "src1Alpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
					else if(strcmp(srcAlphaBlendFactorString, "oneMinusSrc1Alpha")==0)
						pipeline->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown srcAlphaBlendFactor '%s'.\n", srcAlphaBlendFactorString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "dstAlphaBlendFactor"))
				{
					char dstAlphaBlendFactorString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, dstAlphaBlendFactorString, sizeof(dstAlphaBlendFactorString)))
						return false;

					if(strcmp(dstAlphaBlendFactorString, "zero")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
					else if(strcmp(dstAlphaBlendFactorString, "one")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
					else if(strcmp(dstAlphaBlendFactorString, "srcColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusSrcColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "dstColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_DST_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusDstColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "srcAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusSrcAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "dstAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_DST_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusDstAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "constantColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusConstantColor")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "constantAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_CONSTANT_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusConstantAlpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "srcAlphaSaturate")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
					else if(strcmp(dstAlphaBlendFactorString, "src1Color")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusSrc1Color")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
					else if(strcmp(dstAlphaBlendFactorString, "src1Alpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_SRC1_ALPHA;
					else if(strcmp(dstAlphaBlendFactorString, "oneMinusSrc1Alpha")==0)
						pipeline->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown dstAlphaBlendFactor '%s'.\n", dstAlphaBlendFactorString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "alphaBlendOp"))
				{
					char alphaBlendOpString[32]={ 0 };

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_String(&parser, alphaBlendOpString, sizeof(alphaBlendOpString)))
						return false;

					if(strcmp(alphaBlendOpString, "add")==0)
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_ADD;
					else if(strcmp(alphaBlendOpString, "subtract")==0)
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_SUBTRACT;
					else if(strcmp(alphaBlendOpString, "reverseSubtract")==0)
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_REVERSE_SUBTRACT;
					else if(strcmp(alphaBlendOpString, "min")==0)
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_MIN;
					else if(strcmp(alphaBlendOpString, "max")==0)
						pipeline->pipeline.alphaBlendOp=VK_BLEND_OP_MAX;
					else
					{
						DBGPRINTF(DEBUG_ERROR, "Unknown alphaBlendOp '%s'.\n", alphaBlendOpString);
						return false;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "colorWriteMask"))
				{
					char maskString[32]={ 0 };
					VkColorComponentFlags colorWriteMask=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					for(;;)
					{
						if(!Parser_String(&parser, maskString, sizeof(maskString)))
							return false;

						if(strcmp(maskString, "none")==0)
							colorWriteMask=0;
						else if(strcmp(maskString, "colorR")==0)
							colorWriteMask|=VK_COLOR_COMPONENT_R_BIT;
						else if(strcmp(maskString, "colorG")==0)
							colorWriteMask|=VK_COLOR_COMPONENT_G_BIT;
						else if(strcmp(maskString, "colorB")==0)
							colorWriteMask|=VK_COLOR_COMPONENT_B_BIT;
						else if(strcmp(maskString, "colorA")==0)
							colorWriteMask|=VK_COLOR_COMPONENT_A_BIT;
						else
						{
							DBGPRINTF(DEBUG_ERROR, "Unknown colorWriteMask parameter '%s'.\n", maskString);
							return false;
						}

						if(!Parser_Match(&parser, TOKEN_DELIMITER, '|'))
							break;
					}

					pipeline->pipeline.colorWriteMask=colorWriteMask;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;
				}
				else if(Parser_MatchKeyword(&parser, "pushConstant"))
				{
					int64_t offset=0;
					int64_t size=0;
					char stageString[32]={ 0 };
					VkShaderStageFlags stageFlags=0;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, '('))
						return false;

					if(!Parser_Integer(&parser, &offset))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					if(!Parser_Integer(&parser, &size))
						return false;

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ','))
						return false;

					for(;;)
					{
						if(!Parser_String(&parser, stageString, sizeof(stageString)))
							return false;

						if(strcmp(stageString, "vertex")==0)
							stageFlags|=VK_SHADER_STAGE_VERTEX_BIT;
						else if(strcmp(stageString, "tessellationControl")==0)
							stageFlags|=VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
						else if(strcmp(stageString, "tessellationEvaluation")==0)
							stageFlags|=VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
						else if(strcmp(stageString, "geometry")==0)
							stageFlags|=VK_SHADER_STAGE_GEOMETRY_BIT;
						else if(strcmp(stageString, "fragment")==0)
							stageFlags|=VK_SHADER_STAGE_FRAGMENT_BIT;
						else if(strcmp(stageString, "compute")==0)
							stageFlags|=VK_SHADER_STAGE_COMPUTE_BIT;
						else
						{
							DBGPRINTF(DEBUG_ERROR, "Unknown shader stage '%s'.\n", stageString);
							return false;
						}

						if(!Parser_Match(&parser, TOKEN_DELIMITER, '|'))
							break;
					}

					if(!Parser_Expect(&parser, TOKEN_DELIMITER, ')'))
						return false;

					pipeline->pushConstant.offset=(uint32_t)offset;
					pipeline->pushConstant.size=(uint32_t)size;
					pipeline->pushConstant.stageFlags=stageFlags;
				}
				else
				{
					Parser_Unexpected(&parser, "Unknown statement");
					return false;
				}
			}
		}
		else
		{
			Parser_Unexpected(&parser, "Unknown section");
			return false;
		}
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
