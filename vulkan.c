// Vulkan helper functions

#include "vulkan.h"
#include <stdio.h>
#include "math.h"

VkShaderModule vkuCreateShaderModule(VkDevice Device, const char *shaderFile)
{
	VkShaderModule shaderModule=VK_NULL_HANDLE;
	FILE *stream=NULL;

	if(fopen_s(&stream, shaderFile, "rb"))
		return VK_NULL_HANDLE;

	// Seek to end of file to get file size, rescaling to align to 32 bit
	fseek(stream, 0, SEEK_END);
	uint32_t size=(uint32_t)(ceilf((float)ftell(stream)/sizeof(uint32_t))*sizeof(uint32_t));
	fseek(stream, 0, SEEK_SET);

	uint32_t *data=(uint32_t *)malloc(size);

	if(data==NULL)
		return VK_NULL_HANDLE;

	fread_s(data, size, 1, size, stream);

	fclose(stream);

	VkShaderModuleCreateInfo CreateInfo={ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, size, data };
	VkResult Result=vkCreateShaderModule(Device, &CreateInfo, VK_NULL_HANDLE, &shaderModule);

	free(data);

	if(Result==VK_SUCCESS)
		return shaderModule;

	return VK_NULL_HANDLE;
}

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask)
{
	// Search memtypes to find first index with those properties
	for(uint32_t i=0;i<memory_properties.memoryTypeCount;i++)
	{
		if((typeBits&1)==1)
		{
			// Type is available, does it match user properties?
			if((memory_properties.memoryTypes[i].propertyFlags&requirements_mask)==requirements_mask)
				return i;
		}

		typeBits>>=1;
	}

	// No memory types matched, return failure
	return 0;
}

VkBool32 vkuCreateImageBuffer(VkDevice Device, const uint32_t *QueueFamilyIndices, VkPhysicalDeviceMemoryProperties MemoryProperties,
	VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth,
	VkImage *Image, VkDeviceMemory *Memory,  VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags)
{
	VkImageCreateInfo ImageInfo=
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType=ImageType,
		.format=Format,
		.mipLevels=MipLevels,
		.arrayLayers=Layers,
		.samples=VK_SAMPLE_COUNT_1_BIT,
		.tiling=Tiling,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.extent.width=Width,
		.extent.height=Height,
		.extent.depth=Depth,
		.flags=CreateFlags,
	};

	if(vkCreateImage(Device, &ImageInfo, NULL, Image)!=VK_SUCCESS)
		return VK_FALSE;

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(Device, *Image, &memoryRequirements);

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(MemoryProperties, memoryRequirements.memoryTypeBits, RequirementsMask),
	};
	if(vkAllocateMemory(Device, &AllocateInfo, NULL, Memory)!=VK_SUCCESS)
		return VK_FALSE;

	vkBindImageMemory(Device, *Image, *Memory, 0);

	return VK_TRUE;
}

VkBool32 vkuCreateBuffer(VkDevice Device, const uint32_t *QueueFamilyIndices, VkPhysicalDeviceMemoryProperties MemoryProperties, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask)
{
	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo BufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=Size,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=QueueFamilyIndices,
	};

	if(vkCreateBuffer(Device, &BufferInfo, NULL, Buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(Device, *Buffer, &memoryRequirements);

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(MemoryProperties, memoryRequirements.memoryTypeBits, RequirementsMask),
	};

	if(vkAllocateMemory(Device, &AllocateInfo, NULL, Memory)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkBindBufferMemory(Device, *Buffer, *Memory, 0)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

// Copy from one buffer to another
VkBool32 vkuCopyBuffer(VkDevice Device, VkQueue Queue, VkCommandPool CommandPool, VkBuffer Src, VkBuffer Dest, uint32_t Size)
{
	VkCommandBuffer copyCmd=VK_NULL_HANDLE;
	VkFence fence=VK_NULL_HANDLE;

	// Create a command buffer to submit a copy command from the staging buffer into the static vertex buffer
	vkAllocateCommandBuffers(Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);
	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	// Copy command
	vkCmdCopyBuffer(copyCmd, Src, Dest, 1, &(VkBufferCopy) { .srcOffset=0, .dstOffset=0, .size=Size });

	// End command buffer and submit
	vkEndCommandBuffer(copyCmd);
		
	vkCreateFence(Device, &(VkFenceCreateInfo) { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence);

	vkQueueSubmit(Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, fence);

	// Wait for it to finish
	vkWaitForFences(Device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(Device, fence, VK_NULL_HANDLE);
	vkFreeCommandBuffers(Device, CommandPool, 1, &copyCmd);

	return VK_TRUE;
}

///// Pipeline setup code

// Adds a vertex binding
VkBool32 vkuPipeline_AddVertexBinding(VkuPipeline_t *Pipeline, uint32_t Binding, uint32_t Stride, VkVertexInputRate InputRate)
{
	// Already at max bindings
	if(Pipeline->NumVertexBindings>=5)
		return VK_FALSE;

	// Set the binding decriptor
	VkVertexInputBindingDescription Descriptor=
	{
		.binding=Binding,
		.stride=Stride,
		.inputRate=InputRate
	};

	// Assign it to a slot
	Pipeline->VertexBindings[Pipeline->NumVertexBindings]=Descriptor;
	Pipeline->NumVertexBindings++;

	return VK_TRUE;
}

// Adds a vertex attribute
VkBool32 vkuPipeline_AddVertexAttribute(VkuPipeline_t *Pipeline, uint32_t Location, uint32_t Binding, VkFormat Format, uint32_t Offset)
{
	// Already at max attributes
	if(Pipeline->NumVertexAttribs>=5)
		return VK_FALSE;

	// Set the attribute decriptor
	VkVertexInputAttributeDescription Descriptor=
	{
		.location=Location,
		.binding=Binding,
		.format=Format,
		.offset=Offset
	};

	// Assign it to a slot
	Pipeline->VertexAttribs[Pipeline->NumVertexAttribs]=Descriptor;
	Pipeline->NumVertexAttribs++;

	return VK_TRUE;
}

// Loads a shader and assign to a stage
VkBool32 vkuPipeline_AddStage(VkuPipeline_t *Pipeline, const char *ShaderFilename, VkShaderStageFlagBits Stage)
{
	// Already at max stages
	if(Pipeline->NumStages>=4)
		return VK_FALSE;

	// Load and create the shader module from file
	VkShaderModule ShaderModule=vkuCreateShaderModule(Pipeline->Device, ShaderFilename);

	// Check that it's valid
	if(ShaderModule==VK_NULL_HANDLE)
		return VK_FALSE;

	// Set the create information
	VkPipelineShaderStageCreateInfo ShaderStage=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage=Stage,
		.module=ShaderModule,
		.pName="main",
	};

	// Assign the slot
	Pipeline->Stages[Pipeline->NumStages]=ShaderStage;
	Pipeline->NumStages++;

	return VK_TRUE;
}

// Create an initial pipeline configuration with some default states
VkBool32 vkuInitPipeline(VkuPipeline_t *Pipeline, VkDevice Device, VkPipelineLayout PipelineLayout, VkRenderPass RenderPass)
{
	// Pass in handles to dependencies
	Pipeline->Device=Device;
	Pipeline->PipelineLayout=PipelineLayout;
	Pipeline->RenderPass=RenderPass;

	// Set up default state:

	// Vertex binding descriptions
	Pipeline->NumVertexBindings=0;
	memset(Pipeline->VertexBindings, 0, sizeof(VkVertexInputBindingDescription)*5);

	// Vertex attribute descriptions
	Pipeline->NumVertexAttribs=0;
	memset(Pipeline->VertexAttribs, 0, sizeof(VkVertexInputAttributeDescription)*5);

	// Shader stages
	Pipeline->NumStages=0;
	memset(Pipeline->Stages, 0, sizeof(VkPipelineShaderStageCreateInfo)*4);

	// Input assembly state
	Pipeline->Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	Pipeline->PrimitiveRestart=VK_FALSE;

	// Rasterization state
	Pipeline->DepthClamp=VK_FALSE;
	Pipeline->RasterizerDiscard=VK_FALSE;
	Pipeline->PolygonMode=VK_POLYGON_MODE_FILL;
	Pipeline->CullMode=VK_CULL_MODE_NONE;
	Pipeline->FrontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
	Pipeline->DepthBias=VK_FALSE;
	Pipeline->DepthBiasConstantFactor=0.0f;
	Pipeline->DepthBiasClamp=0.0f;
	Pipeline->DepthBiasSlopeFactor=0.0f;
	Pipeline->LineWidth=1.0f;

	// Depth/stencil state
	Pipeline->DepthTest=VK_FALSE;
	Pipeline->DepthWrite=VK_TRUE;
	Pipeline->DepthCompareOp=VK_COMPARE_OP_LESS;
	Pipeline->DepthBoundsTest=VK_FALSE;
	Pipeline->StencilTest=VK_FALSE;
	Pipeline->MinDepthBounds=0.0f;
	Pipeline->MaxDepthBounds=0.0f;

	// Front face stencil functions
	Pipeline->FrontStencilFailOp=VK_STENCIL_OP_KEEP;
	Pipeline->FrontStencilPassOp=VK_STENCIL_OP_KEEP;
	Pipeline->FrontStencilDepthFailOp=VK_STENCIL_OP_KEEP;
	Pipeline->FrontStencilCompareOp=VK_COMPARE_OP_ALWAYS;
	Pipeline->FrontStencilCompareMask=0;
	Pipeline->FrontStencilWriteMask=0;
	Pipeline->FrontStencilRference=0;

	// Back face stencil functions
	Pipeline->BackStencilFailOp=VK_STENCIL_OP_KEEP;
	Pipeline->BackStencilPassOp=VK_STENCIL_OP_KEEP;
	Pipeline->BackStencilDepthFailOp=VK_STENCIL_OP_KEEP;
	Pipeline->BackStencilCompareOp=VK_COMPARE_OP_ALWAYS;
	Pipeline->BackStencilCompareMask=0;
	Pipeline->BackStencilWriteMask=0;
	Pipeline->BackStencilReference=0;

	// Multisample state
	Pipeline->RasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
	Pipeline->SampleShading=VK_FALSE;
	Pipeline->MinSampleShading=1.0f;
	Pipeline->SampleMask=VK_NULL_HANDLE;
	Pipeline->AlphaToCoverage=VK_FALSE;
	Pipeline->AlphaToOne=VK_FALSE;

	// Blend state
	Pipeline->BlendLogicOp=VK_FALSE;
	Pipeline->BlendLogicOpState=VK_LOGIC_OP_COPY;
	Pipeline->Blend=VK_FALSE;
	Pipeline->SrcColorBlendFactor=VK_BLEND_FACTOR_ONE;
	Pipeline->DstColorBlendFactor=VK_BLEND_FACTOR_ZERO;
	Pipeline->ColorBlendOp=VK_BLEND_OP_ADD;
	Pipeline->SrcAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
	Pipeline->DstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO;
	Pipeline->AlphaBlendOp=VK_BLEND_OP_ADD;
	Pipeline->ColorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;

	return VK_TRUE;
}

VkBool32 vkuAssemblePipeline(VkuPipeline_t *Pipeline)
{
	VkResult Result=vkCreateGraphicsPipelines(Pipeline->Device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount=Pipeline->NumStages,
		.pStages=Pipeline->Stages,
		.pVertexInputState=&(VkPipelineVertexInputStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount=Pipeline->NumVertexBindings,
			.pVertexBindingDescriptions=Pipeline->VertexBindings,
			.vertexAttributeDescriptionCount=Pipeline->NumVertexAttribs,
			.pVertexAttributeDescriptions=Pipeline->VertexAttribs,
		},
		.pInputAssemblyState=&(VkPipelineInputAssemblyStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology=Pipeline->Topology,
			.primitiveRestartEnable=Pipeline->PrimitiveRestart,
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
			.depthClampEnable=Pipeline->DepthClamp,
			.rasterizerDiscardEnable=Pipeline->RasterizerDiscard,
			.polygonMode=Pipeline->PolygonMode,
			.cullMode=Pipeline->CullMode,
			.frontFace=Pipeline->FrontFace,
			.depthBiasEnable=Pipeline->DepthBias,
			.depthBiasConstantFactor=Pipeline->DepthBiasConstantFactor,
			.depthBiasClamp=Pipeline->DepthBiasClamp,
			.depthBiasSlopeFactor=Pipeline->DepthBiasSlopeFactor,
			.lineWidth=Pipeline->LineWidth,
		},
		.pDepthStencilState=&(VkPipelineDepthStencilStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable=Pipeline->DepthTest,
			.depthWriteEnable=Pipeline->DepthWrite,
			.depthCompareOp=Pipeline->DepthCompareOp,
			.depthBoundsTestEnable=Pipeline->DepthBoundsTest,
			.stencilTestEnable=Pipeline->StencilTest,
			.minDepthBounds=Pipeline->MinDepthBounds,
			.maxDepthBounds=Pipeline->MaxDepthBounds,
			.front=(VkStencilOpState)
			{
				.failOp=Pipeline->FrontStencilFailOp,
				.passOp=Pipeline->FrontStencilPassOp,
				.depthFailOp=Pipeline->FrontStencilDepthFailOp,
				.compareOp=Pipeline->FrontStencilCompareOp,
				.compareMask=Pipeline->FrontStencilCompareMask,
				.writeMask=Pipeline->FrontStencilWriteMask,
				.reference=Pipeline->FrontStencilRference,
			},
			.back=(VkStencilOpState)
			{
				.failOp=Pipeline->BackStencilFailOp,
				.passOp=Pipeline->BackStencilPassOp,
				.depthFailOp=Pipeline->BackStencilDepthFailOp,
				.compareOp=Pipeline->BackStencilCompareOp,
				.compareMask=Pipeline->BackStencilCompareMask,
				.writeMask=Pipeline->BackStencilWriteMask,
				.reference=Pipeline->BackStencilReference,
			},
		},
		.pMultisampleState=&(VkPipelineMultisampleStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples=Pipeline->RasterizationSamples,
			.sampleShadingEnable=Pipeline->SampleShading,
			.minSampleShading=Pipeline->MinSampleShading,
			.pSampleMask=Pipeline->SampleMask,
			.alphaToCoverageEnable=Pipeline->AlphaToCoverage,
			.alphaToOneEnable=Pipeline->AlphaToOne,
		},
		.pColorBlendState=&(VkPipelineColorBlendStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable=Pipeline->BlendLogicOp,
			.logicOp=Pipeline->BlendLogicOpState,
			.attachmentCount=1,
			.pAttachments=(VkPipelineColorBlendAttachmentState[])
			{
				{
					.blendEnable=Pipeline->Blend,
					.srcColorBlendFactor=Pipeline->SrcColorBlendFactor,
					.dstColorBlendFactor=Pipeline->DstColorBlendFactor,
					.colorBlendOp=Pipeline->ColorBlendOp,
					.srcAlphaBlendFactor=Pipeline->SrcAlphaBlendFactor,
					.dstAlphaBlendFactor=Pipeline->DstAlphaBlendFactor,
					.alphaBlendOp=Pipeline->AlphaBlendOp,
					.colorWriteMask=Pipeline->ColorWriteMask,
				},
			},
		},
		.pDynamicState=&(VkPipelineDynamicStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount=2,
			.pDynamicStates=(VkDynamicState[]){ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		},
		.layout=Pipeline->PipelineLayout,
		.renderPass=Pipeline->RenderPass,
	}, 0, &Pipeline->Pipeline);

	// Done with pipeline creation, delete shader modules
	for(uint32_t i=0;i<Pipeline->NumStages;i++)
		vkDestroyShaderModule(Pipeline->Device, Pipeline->Stages[i].module, 0);

	return Result==VK_SUCCESS?VK_TRUE:VK_FALSE;
}
/////
