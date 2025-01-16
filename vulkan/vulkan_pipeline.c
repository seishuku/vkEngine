// Vulkan pipeline helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vulkan.h"
#include "../utils/spvparse.h"

// Loads a binary shader from memory and creates a shader module
VkShaderModule vkuCreateShaderModuleMemory(VkDevice device, const uint32_t *data, const uint32_t size)
{
	VkShaderModule shaderModule=VK_NULL_HANDLE;

	VkShaderModuleCreateInfo CreateInfo={ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, size, data };
	VkResult Result=vkCreateShaderModule(device, &CreateInfo, VK_NULL_HANDLE, &shaderModule);

	parseSpv(data, size);

	if(Result==VK_SUCCESS)
		return shaderModule;

	return VK_NULL_HANDLE;
}

// Loads a binary shader file and creates a shader module
VkShaderModule vkuCreateShaderModule(VkDevice device, const char *shaderFile)
{
	VkShaderModule shaderModule=VK_NULL_HANDLE;
	FILE *stream=NULL;

	if((stream=fopen(shaderFile, "rb"))==NULL)
		return VK_NULL_HANDLE;

	// Seek to end of file to get file size, rescaling to align to 32 bit
	fseek(stream, 0, SEEK_END);
	uint32_t size=(uint32_t)(ceilf((float)ftell(stream)/sizeof(uint32_t))*sizeof(uint32_t));
	fseek(stream, 0, SEEK_SET);

	uint32_t *data=(uint32_t *)Zone_Malloc(zone, size);

	if(data==NULL)
		return VK_NULL_HANDLE;

	fread(data, 1, size, stream);

	fclose(stream);

	shaderModule=vkuCreateShaderModuleMemory(device, data, size);

	Zone_Free(zone, data);

	return shaderModule;
}

// Adds a vertex binding
VkBool32 vkuPipeline_AddVertexBinding(VkuPipeline_t *pipeline, uint32_t binding, uint32_t stride, VkVertexInputRate inputRate)
{
	if(!pipeline)
		return VK_FALSE;

	// Already at max bindings
	if(pipeline->numVertexBindings>=VKU_MAX_PIPELINE_VERTEX_BINDINGS)
		return VK_FALSE;

	// Set the binding decriptor
	VkVertexInputBindingDescription Descriptor=
	{
		.binding=binding,
		.stride=stride,
		.inputRate=inputRate
	};

	// Assign it to a slot
	pipeline->vertexBindings[pipeline->numVertexBindings]=Descriptor;
	pipeline->numVertexBindings++;

	return VK_TRUE;
}

// Adds a vertex attribute
VkBool32 vkuPipeline_AddVertexAttribute(VkuPipeline_t *pipeline, uint32_t location, uint32_t binding, VkFormat format, uint32_t offset)
{
	if(!pipeline)
		return VK_FALSE;

	// Already at max attributes
	if(pipeline->numVertexAttribs>=VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES)
		return VK_FALSE;

	// Set the attribute decriptor
	VkVertexInputAttributeDescription Descriptor=
	{
		.location=location,
		.binding=binding,
		.format=format,
		.offset=offset
	};

	// Assign it to a slot
	pipeline->vertexAttribs[pipeline->numVertexAttribs]=Descriptor;
	pipeline->numVertexAttribs++;

	return VK_TRUE;
}

// Loads a shader from memory and assign to a stage
VkBool32 vkuPipeline_AddStageMemory(VkuPipeline_t *pipeline, const uint32_t *data, const uint32_t size, VkShaderStageFlagBits stage)
{
	if(!pipeline)
		return VK_FALSE;

	// Already at max stages
	if(pipeline->numStages>=VKU_MAX_PIPELINE_SHADER_STAGES)
		return VK_FALSE;

	// Load and create the shader module from file
	VkShaderModule shaderModule=vkuCreateShaderModuleMemory(pipeline->device, data, size);

	// Check that it's valid
	if(shaderModule==VK_NULL_HANDLE)
		return VK_FALSE;

	// Set the create information
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage=stage,
		.module=shaderModule,
		.pName="main",
	};

	// Assign the slot
	pipeline->stages[pipeline->numStages]=shaderStageCreateInfo;
	pipeline->numStages++;

	return VK_TRUE;
}

// Loads a shader and assign to a stage
VkBool32 vkuPipeline_AddStage(VkuPipeline_t *pipeline, const char *shaderFilename, VkShaderStageFlagBits stage)
{
	if(!pipeline)
		return VK_FALSE;

	// Already at max stages
	if(pipeline->numStages>=VKU_MAX_PIPELINE_SHADER_STAGES)
		return VK_FALSE;

	// Load and create the shader module from file
	VkShaderModule shaderModule=vkuCreateShaderModule(pipeline->device, shaderFilename);

	// Check that it's valid
	if(shaderModule==VK_NULL_HANDLE)
		return VK_FALSE;

	// Set the create information
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage=stage,
		.module=shaderModule,
		.pName="main",
	};

	// Assign the slot
	pipeline->stages[pipeline->numStages]=shaderStageCreateInfo;
	pipeline->numStages++;

	return VK_TRUE;
}

VkBool32 vkuPipeline_SetRenderPass(VkuPipeline_t *pipeline, VkRenderPass renderPass)
{
	if(!pipeline)
		return VK_FALSE;

	pipeline->renderPass=renderPass;

	return VK_TRUE;
}

VkBool32 vkuPipeline_SetPipelineLayout(VkuPipeline_t *pipeline, VkPipelineLayout pipelineLayout)
{
	if(!pipeline)
		return VK_FALSE;

	pipeline->pipelineLayout=pipelineLayout;

	return VK_TRUE;
}

// Create an initial pipeline configuration with some default states
VkBool32 vkuInitPipeline(VkuPipeline_t *pipeline, VkDevice device, VkPipelineCache pipelineCache)
{
	if(!pipeline)
		return VK_FALSE;

	// Pass in handles to dependencies
	pipeline->device=device;
	pipeline->pipelineCache=pipelineCache;

	pipeline->pipelineLayout=VK_NULL_HANDLE;
	pipeline->renderPass=VK_NULL_HANDLE;

	// Set up default state:

	// Vertex binding descriptions
	pipeline->numVertexBindings=0;
	memset(pipeline->vertexBindings, 0, sizeof(VkVertexInputBindingDescription)*VKU_MAX_PIPELINE_VERTEX_BINDINGS);

	// Vertex attribute descriptions
	pipeline->numVertexAttribs=0;
	memset(pipeline->vertexAttribs, 0, sizeof(VkVertexInputAttributeDescription)*VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES);

	// Shader stages
	pipeline->numStages=0;
	memset(pipeline->stages, 0, sizeof(VkPipelineShaderStageCreateInfo)*VKU_MAX_PIPELINE_SHADER_STAGES);

	// Subpass 0
	pipeline->subpass=0;

	// Input assembly state
	pipeline->topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipeline->primitiveRestart=VK_FALSE;

	// Rasterization state
	pipeline->depthClamp=VK_FALSE;
	pipeline->rasterizerDiscard=VK_FALSE;
	pipeline->polygonMode=VK_POLYGON_MODE_FILL;
	pipeline->cullMode=VK_CULL_MODE_NONE;
	pipeline->frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeline->depthBias=VK_FALSE;
	pipeline->depthBiasConstantFactor=0.0f;
	pipeline->depthBiasClamp=0.0f;
	pipeline->depthBiasSlopeFactor=0.0f;
	pipeline->lineWidth=1.0f;

	// Depth/stencil state
	pipeline->depthTest=VK_FALSE;
	pipeline->depthWrite=VK_TRUE;
	pipeline->depthCompareOp=VK_COMPARE_OP_LESS;
	pipeline->depthBoundsTest=VK_FALSE;
	pipeline->stencilTest=VK_FALSE;
	pipeline->minDepthBounds=0.0f;
	pipeline->maxDepthBounds=0.0f;

	// Front face stencil functions
	pipeline->frontStencilFailOp=VK_STENCIL_OP_KEEP;
	pipeline->frontStencilPassOp=VK_STENCIL_OP_KEEP;
	pipeline->frontStencilDepthFailOp=VK_STENCIL_OP_KEEP;
	pipeline->frontStencilCompareOp=VK_COMPARE_OP_ALWAYS;
	pipeline->frontStencilCompareMask=0;
	pipeline->frontStencilWriteMask=0;
	pipeline->frontStencilReference=0;

	// Back face stencil functions
	pipeline->backStencilFailOp=VK_STENCIL_OP_KEEP;
	pipeline->backStencilPassOp=VK_STENCIL_OP_KEEP;
	pipeline->backStencilDepthFailOp=VK_STENCIL_OP_KEEP;
	pipeline->backStencilCompareOp=VK_COMPARE_OP_ALWAYS;
	pipeline->backStencilCompareMask=0;
	pipeline->backStencilWriteMask=0;
	pipeline->backStencilReference=0;

	// Multisample state
	pipeline->rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
	pipeline->sampleShading=VK_FALSE;
	pipeline->minSampleShading=1.0f;
	pipeline->sampleMask=VK_NULL_HANDLE;
	pipeline->alphaToCoverage=VK_FALSE;
	pipeline->alphaToOne=VK_FALSE;

	// blend state
	pipeline->blendLogicOp=VK_FALSE;
	pipeline->blendLogicOpState=VK_LOGIC_OP_COPY;
	pipeline->blend=VK_FALSE;
	pipeline->srcColorBlendFactor=VK_BLEND_FACTOR_ONE;
	pipeline->dstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
	pipeline->colorBlendOp=VK_BLEND_OP_ADD;
	pipeline->srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
	pipeline->dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
	pipeline->alphaBlendOp=VK_BLEND_OP_ADD;
	pipeline->colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

	return VK_TRUE;
}

VkBool32 vkuAssemblePipeline(VkuPipeline_t *pipeline, void *pNext)
{
	if(!pipeline)
		return VK_FALSE;

	VkResult result=vkCreateGraphicsPipelines(pipeline->device, pipeline->pipelineCache, 1, &(VkGraphicsPipelineCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext=pNext,
		.stageCount=pipeline->numStages,
		.pStages=pipeline->stages,
		.pVertexInputState=&(VkPipelineVertexInputStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount=pipeline->numVertexBindings,
			.pVertexBindingDescriptions=pipeline->vertexBindings,
			.vertexAttributeDescriptionCount=pipeline->numVertexAttribs,
			.pVertexAttributeDescriptions=pipeline->vertexAttribs,
		},
		.pInputAssemblyState=&(VkPipelineInputAssemblyStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology=pipeline->topology,
			.primitiveRestartEnable=pipeline->primitiveRestart,
		},
		.pViewportState=&(VkPipelineViewportStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount=1,
			.pViewports=VK_NULL_HANDLE,
			.scissorCount=1,
			.pScissors=VK_NULL_HANDLE,
		},
		.pRasterizationState=&(VkPipelineRasterizationStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable=pipeline->depthClamp,
			.rasterizerDiscardEnable=pipeline->rasterizerDiscard,
			.polygonMode=pipeline->polygonMode,
			.cullMode=pipeline->cullMode,
			.frontFace=pipeline->frontFace,
			.depthBiasEnable=pipeline->depthBias,
			.depthBiasConstantFactor=pipeline->depthBiasConstantFactor,
			.depthBiasClamp=pipeline->depthBiasClamp,
			.depthBiasSlopeFactor=pipeline->depthBiasSlopeFactor,
			.lineWidth=pipeline->lineWidth,
		},
		.pDepthStencilState=&(VkPipelineDepthStencilStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable=pipeline->depthTest,
			.depthWriteEnable=pipeline->depthWrite,
			.depthCompareOp=pipeline->depthCompareOp,
			.depthBoundsTestEnable=pipeline->depthBoundsTest,
			.stencilTestEnable=pipeline->stencilTest,
			.minDepthBounds=pipeline->minDepthBounds,
			.maxDepthBounds=pipeline->maxDepthBounds,
			.front=(VkStencilOpState)
			{
				.failOp=pipeline->frontStencilFailOp,
				.passOp=pipeline->frontStencilPassOp,
				.depthFailOp=pipeline->frontStencilDepthFailOp,
				.compareOp=pipeline->frontStencilCompareOp,
				.compareMask=pipeline->frontStencilCompareMask,
				.writeMask=pipeline->frontStencilWriteMask,
				.reference=pipeline->frontStencilReference,
			},
			.back=(VkStencilOpState)
			{
				.failOp=pipeline->backStencilFailOp,
				.passOp=pipeline->backStencilPassOp,
				.depthFailOp=pipeline->backStencilDepthFailOp,
				.compareOp=pipeline->backStencilCompareOp,
				.compareMask=pipeline->backStencilCompareMask,
				.writeMask=pipeline->backStencilWriteMask,
				.reference=pipeline->backStencilReference,
			},
		},
		.pMultisampleState=&(VkPipelineMultisampleStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples=pipeline->rasterizationSamples,
			.sampleShadingEnable=pipeline->sampleShading,
			.minSampleShading=pipeline->minSampleShading,
			.pSampleMask=pipeline->sampleMask,
			.alphaToCoverageEnable=pipeline->alphaToCoverage,
			.alphaToOneEnable=pipeline->alphaToOne,
		},
		.pColorBlendState=&(VkPipelineColorBlendStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable=pipeline->blendLogicOp,
			.logicOp=pipeline->blendLogicOpState,
			.attachmentCount=1,
			.pAttachments=(VkPipelineColorBlendAttachmentState[])
			{
				{
					.blendEnable=pipeline->blend,
					.srcColorBlendFactor=pipeline->srcColorBlendFactor,
					.dstColorBlendFactor=pipeline->dstColorBlendFactor,
					.colorBlendOp=pipeline->colorBlendOp,
					.srcAlphaBlendFactor=pipeline->srcAlphaBlendFactor,
					.dstAlphaBlendFactor=pipeline->dstAlphaBlendFactor,
					.alphaBlendOp=pipeline->alphaBlendOp,
					.colorWriteMask=pipeline->colorWriteMask,
				},
			},
		},
		.pDynamicState=&(VkPipelineDynamicStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount=2,
			.pDynamicStates=(VkDynamicState[]){ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		},
		.layout=pipeline->pipelineLayout,
		.renderPass=pipeline->renderPass,
		.subpass=pipeline->subpass,
	}, 0, &pipeline->pipeline);

	// Done with pipeline creation, delete shader modules
	for(uint32_t i=0;i<pipeline->numStages;i++)
		vkDestroyShaderModule(pipeline->device, pipeline->stages[i].module, 0);

	return result==VK_SUCCESS?VK_TRUE:VK_FALSE;
}

VkBool32 vkuAssembleComputePipeline(VkuPipeline_t *pipeline, void *pNext)
{
	if(!pipeline)
		return VK_FALSE;

	VkResult result=vkCreateComputePipelines(pipeline->device, pipeline->pipelineCache, 1, &(VkComputePipelineCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.pNext=pNext,
		.stage=pipeline->stages[0],
		.layout=pipeline->pipelineLayout,
	}, 0, &pipeline->pipeline);

	// Done with pipeline creation, delete shader modules
	for(uint32_t i=0;i<pipeline->numStages;i++)
		vkDestroyShaderModule(pipeline->device, pipeline->stages[i].module, 0);

	return result==VK_SUCCESS?VK_TRUE:VK_FALSE;
}
