#include <Windows.h>
#include <sys/types.h>
#include <stdio.h>
#include "vulkan.h"
#include "math.h"
#include "3ds.h"
#include "image.h"
#include "font.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

HWND hWnd=NULL;

char szAppName[]="Vulkan";

int Width=1024, Height=576;
//int Width=1280, Height=720;
int Done=0, Key[256];

float RotateX=0.0f, RotateY=0.0f, PanX=0.0f, PanY=0.0f, Zoom=-200.0f;

unsigned __int64 Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;
int Frames=0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum
{
	MODEL_HELLKNIGHT,
	MODEL_PINKY,
	MODEL_FATTY,
	MODEL_LEVEL,
	NUM_MODELS
};

Model3DS_t Model[NUM_MODELS];

enum
{
	TEXTURE_HELLKNIGHT,
	TEXTURE_HELLKNIGHT_NORMAL,
	TEXTURE_PINKY,
	TEXTURE_PINKY_NORMAL,
	TEXTURE_FATTY,
	TEXTURE_FATTY_NORMAL,
	TEXTURE_LEVEL,
	TEXTURE_LEVEL_NORMAL,
	NUM_TEXTURES
};

Image_t Textures[NUM_TEXTURES];

enum
{
	MAX_DEVICE_COUNT=8,
	MAX_QUEUE_COUNT=4,
	FRAME_COUNT=2,
};

 _inline uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
{
	return value<min?min:(value>max?max:value);
}

float ModelView[16], Projection[16];
float QuatX[4], QuatY[4], Quat[4];

struct
{
	float mvp[16];
	float mvinv[16];

	float Light0_Pos[4];
	float Light0_Kd[4];
	float Light1_Pos[4];
	float Light1_Kd[4];
	float Light2_Pos[4];
	float Light2_Kd[4];
} ubo;

VkInstance instance=VK_NULL_HANDLE;

VkDevice device=VK_NULL_HANDLE;
VkPhysicalDeviceMemoryProperties deviceMemProperties;

VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;

VkSurfaceKHR surface=VK_NULL_HANDLE;

uint32_t queueFamilyIndex;
VkQueue queue=VK_NULL_HANDLE;

// Swapchain
VkSwapchainKHR swapchain;
VkImage swapchainImages[FRAME_COUNT];
VkExtent2D swapchainExtent;
VkSurfaceFormatKHR surfaceFormat;

VkImageView swapchainImageViews[FRAME_COUNT];
VkFramebuffer frameBuffers[FRAME_COUNT];

// Depth buffer handles
VkImage depthImage;
VkDeviceMemory depthMemory;
VkImageView depthImageView;

VkRenderPass renderPass;

VkPipelineLayout pipelineLayout;
VkPipeline pipeline;

VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
VkDescriptorSet descriptorSet[4]={ VK_NULL_HANDLE, };
VkDescriptorSetLayout descriptorSetLayout=VK_NULL_HANDLE;

uint32_t frameIndex=0;

VkCommandPool commandPool;
VkCommandBuffer commandBuffers[FRAME_COUNT];

VkFence frameFences[FRAME_COUNT];
VkSemaphore presentCompleteSemaphores[FRAME_COUNT];

VkBuffer uniformBuffer;
VkDeviceMemory uniformBufferMemory;
void *uniformBufferPtr;

int createRenderPass()
{
	vkCreateRenderPass(device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=surfaceFormat.format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			{
				.format=VK_FORMAT_D32_SFLOAT,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		},
		.subpassCount=1,
		.pSubpasses=&(VkSubpassDescription)
		{
			.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount=1,
			.pColorAttachments=&(VkAttachmentReference)
			{
				.attachment=0,
				.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			.pDepthStencilAttachment=&(VkAttachmentReference)
			{
				.attachment=1,
				.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		},
	}, 0, &renderPass);

	return 1;
}

int createFramebuffers()
{
	uint32_t i;

	vkCreateImage(device, &(VkImageCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.pNext=NULL,
		.imageType=VK_IMAGE_TYPE_2D,
		.format=VK_FORMAT_D32_SFLOAT,
		.extent.width=Width,
		.extent.height=Height,
		.extent.depth=1,
		.mipLevels=1,
		.arrayLayers=1,
		.samples=VK_SAMPLE_COUNT_1_BIT,
		.initialLayout=VK_IMAGE_TILING_OPTIMAL,
		.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=NULL,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.flags=0,
	}, NULL, &depthImage);

	VkMemoryRequirements memoryRequirements;

	vkGetImageMemoryRequirements(device, depthImage, &memoryRequirements);

	vkAllocateMemory(device, &(VkMemoryAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext=NULL,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(deviceMemProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	}, NULL, &depthMemory);

	vkBindImageMemory(device, depthImage, depthMemory, 0);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=depthImage,
		.format=VK_FORMAT_D32_SFLOAT,
		.components.r=VK_COMPONENT_SWIZZLE_R,
		.components.g=VK_COMPONENT_SWIZZLE_G,
		.components.b=VK_COMPONENT_SWIZZLE_B,
		.components.a=VK_COMPONENT_SWIZZLE_A,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &depthImageView);

	for(i=0;i<FRAME_COUNT;i++)
	{
		vkCreateImageView(device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=swapchainImages[i],
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.format=surfaceFormat.format,
			.subresourceRange=
			{
				.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel=0,
				.levelCount=1,
				.baseArrayLayer=0,
				.layerCount=1,
			},
		}, 0, &swapchainImageViews[i]);

		vkCreateFramebuffer(device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=renderPass,
			.attachmentCount=2,
			.pAttachments=(VkImageView[]) { swapchainImageViews[i], depthImageView },
			.width=swapchainExtent.width,
			.height=swapchainExtent.height,
			.layers=1,
		}, 0, &frameBuffers[i]);
	}

	return 1;
}

int createPipeline()
{
	VkShaderModule vertexShader=vkuCreateShaderModule(device, "lighting_v.spv");
	VkShaderModule fragmentShader=vkuCreateShaderModule(device, "lighting_f.spv");

	vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.maxSets=4,
		.poolSizeCount=2,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=4,
			},
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=8,
			},
		},
	}, NULL, &descriptorPool);

	vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.bindingCount=3,
		.pBindings=(VkDescriptorSetLayoutBinding[])
		{
			{
				.binding=0,
				.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=2,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
		},
	}, NULL, &descriptorSetLayout);

	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&descriptorSetLayout,
	}, 0, &pipelineLayout);

	vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount=2,
		.pStages=(VkPipelineShaderStageCreateInfo[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage=VK_SHADER_STAGE_VERTEX_BIT,
				.module=vertexShader,
				.pName="main",
			},
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage=VK_SHADER_STAGE_FRAGMENT_BIT,
				.module=fragmentShader,
				.pName="main",
			},
		},
		.pVertexInputState=&(VkPipelineVertexInputStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount=1,
			.pVertexBindingDescriptions=(VkVertexInputBindingDescription[])
			{
				{
					.binding=0,
					.stride=sizeof(float)*14,
					.inputRate=VK_VERTEX_INPUT_RATE_VERTEX
				}
			},
			.vertexAttributeDescriptionCount=5,
			.pVertexAttributeDescriptions=(VkVertexInputAttributeDescription[])
			{
				{
					.location=0,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*0
				},
				{
					.location=1,
					.binding=0,
					.format=VK_FORMAT_R32G32_SFLOAT,
					.offset=sizeof(float)*(0+3)
				},
				{
					.location=2,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*(0+3+2)
				},
				{
					.location=3,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*(0+3+2+3)
				},
				{
					.location=4,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*(0+3+2+3+3)
				},
			},
		},
		.pInputAssemblyState=&(VkPipelineInputAssemblyStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable=VK_FALSE,
		},
		.pViewportState=&(VkPipelineViewportStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.viewportCount=1,
				.pViewports=0,
				.scissorCount=1,
				.pScissors=0,
		},
		.pRasterizationState=&(VkPipelineRasterizationStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.depthClampEnable=VK_FALSE,
				.rasterizerDiscardEnable=VK_FALSE,
				.polygonMode=VK_POLYGON_MODE_FILL,
				.cullMode=VK_CULL_MODE_BACK_BIT,
				.frontFace=VK_FRONT_FACE_CLOCKWISE,
				.depthBiasEnable=VK_FALSE,
				.depthBiasConstantFactor=0.0f,
				.depthBiasClamp=0.0f,
				.depthBiasSlopeFactor=0.0f,
				.lineWidth=1.0f,
		},
		.pDepthStencilState=&(VkPipelineDepthStencilStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable=VK_TRUE,
			.depthWriteEnable=VK_TRUE,
			.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable=VK_FALSE,
			.stencilTestEnable=VK_FALSE,
			.minDepthBounds=0.0f,
			.maxDepthBounds=0.0f,
			.front=(VkStencilOpState)
			{
				.failOp=VK_STENCIL_OP_KEEP,
				.passOp=VK_STENCIL_OP_KEEP,
				.depthFailOp=VK_STENCIL_OP_KEEP,
				.compareOp=VK_COMPARE_OP_ALWAYS,
				.compareMask=0,
				.writeMask=0,
				.reference=0,
			},
			.back=(VkStencilOpState)
			{
				.failOp=VK_STENCIL_OP_KEEP,
				.passOp=VK_STENCIL_OP_KEEP,
				.depthFailOp=VK_STENCIL_OP_KEEP,
				.compareOp=VK_COMPARE_OP_ALWAYS,
				.compareMask=0,
				.writeMask=0,
				.reference=0,
			},
		},
		.pMultisampleState=&(VkPipelineMultisampleStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable=VK_FALSE,
			.minSampleShading=1.0f,
			.pSampleMask=0,
			.alphaToCoverageEnable=VK_FALSE,
			.alphaToOneEnable=VK_FALSE,
		},
		.pColorBlendState=&(VkPipelineColorBlendStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable=VK_FALSE,
			.attachmentCount=1,
			.pAttachments=(VkPipelineColorBlendAttachmentState[])
			{
				{
					.blendEnable=VK_FALSE,
					.srcColorBlendFactor=VK_BLEND_FACTOR_ONE,
					.dstColorBlendFactor=VK_BLEND_FACTOR_ZERO,
					.colorBlendOp=VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor=VK_BLEND_FACTOR_ONE,
					.dstAlphaBlendFactor=VK_BLEND_FACTOR_ZERO,
					.alphaBlendOp=VK_BLEND_OP_ADD,
					.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT,
				},
			},
		},
		.pDynamicState=&(VkPipelineDynamicStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount=2,
			.pDynamicStates=(VkDynamicState[]) { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		},
		.layout=pipelineLayout,
		.renderPass=renderPass,
	}, 0, &pipeline);

	vkDestroyShaderModule(device, vertexShader, 0);
	vkDestroyShaderModule(device, fragmentShader, 0);

	return pipeline!=0;
}

void BuildMemoryBuffers(Model3DS_t *Model)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	void *Data=NULL;
	int i, j;

	for(i=0;i<Model->NumMesh;i++)
	{
		// Vertex data on device memory
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&Model->Mesh[i].Buffer, &Model->Mesh[i].BufferMemory,
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create staging buffer to transfer from host memory to device memory
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		for(j=0;j<Model->Mesh[i].NumVertex;j++)
		{
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+2];

			*((float *)Data)++=Model->Mesh[i].UV[2*j+0];
			*((float *)Data)++=Model->Mesh[i].UV[2*j+1];

			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+2];

			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+2];

			*((float *)Data)++=Model->Mesh[i].Normal[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Normal[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Normal[3*j+2];
		}

		vkUnmapMemory(device, stagingBufferMemory);

		// Copy to device memory
		vkuCopyBuffer(device, queue, commandPool, stagingBuffer, Model->Mesh[i].Buffer, sizeof(float)*14*Model->Mesh[i].NumVertex);

		// Delete staging data
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);

		// Index data
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&Model->Mesh[i].IndexBuffer, &Model->Mesh[i].IndexBufferMemory,
			sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Staging buffer
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		for(j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+2];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+1];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+0];
		}

		vkUnmapMemory(device, stagingBufferMemory);

		vkuCopyBuffer(device, queue, commandPool, stagingBuffer, Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);
	}
}

int createUploadBuffer()
{
	uint32_t i;

	// Load models
	if(Load3DS(&Model[MODEL_HELLKNIGHT], "hellknight.3ds"))
		BuildMemoryBuffers(&Model[MODEL_HELLKNIGHT]);

	if(Load3DS(&Model[MODEL_PINKY], "pinky.3ds"))
		BuildMemoryBuffers(&Model[MODEL_PINKY]);

	if(Load3DS(&Model[MODEL_FATTY], "fatty.3ds"))
		BuildMemoryBuffers(&Model[MODEL_FATTY]);

	if(Load3DS(&Model[MODEL_LEVEL], "level.3ds"))
		BuildMemoryBuffers(&Model[MODEL_LEVEL]);

	// Load textures
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT], "hellknight.tga", IMAGE_NONE);
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT_NORMAL], "hellknight_n.tga", IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_PINKY], "pinky.tga", IMAGE_NONE);
	Image_Upload(&Textures[TEXTURE_PINKY_NORMAL], "pinky_n.tga", IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_FATTY], "fatty.tga", IMAGE_NONE);
	Image_Upload(&Textures[TEXTURE_FATTY_NORMAL], "fatty_n.tga", IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_LEVEL], "tile.tga", IMAGE_NONE);
	Image_Upload(&Textures[TEXTURE_LEVEL_NORMAL], "tile_b.tga", IMAGE_NORMALMAP|IMAGE_NORMALIZE);

	vkDeviceWaitIdle(device);

	// Uniform data
	vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
		&uniformBuffer, &uniformBufferMemory,
		sizeof(ubo),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(device, uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &uniformBufferPtr);

	for(i=0;i<4;i++)
	{
		vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext=NULL,
			.descriptorPool=descriptorPool,
			.descriptorSetCount=1,
			.pSetLayouts=&descriptorSetLayout
		}, &descriptorSet[i]);

		vkUpdateDescriptorSets(device, 3, (VkWriteDescriptorSet[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.dstBinding=0,
				.pBufferInfo=&(VkDescriptorBufferInfo)
				{
					.buffer=uniformBuffer,
					.offset=0,
					.range=sizeof(ubo),
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=1,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=Textures[2*i+0].view,
					.sampler=Textures[2*i+0].sampler,
					.imageLayout=Textures[2*i+0].imageLayout,
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=2,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=Textures[2*i+1].view,
					.sampler=Textures[2*i+1].sampler,
					.imageLayout=Textures[2*i+1].imageLayout,
				},
				.dstSet=descriptorSet[i],
			},
		}, 0, VK_NULL_HANDLE);
	}

	return TRUE;
}

void draw_frame()
{
	uint32_t index=frameIndex%FRAME_COUNT;
	uint32_t imageIndex;
	int i, j;

	// Generate the projection matrix
	MatrixIdentity(Projection);
	InfPerspective(60.0f, (float)Width/Height, 0.01f, Projection);
	Projection[1*4+1]*=-1.0f;   // Tweak for Vulkan coordnates

	// Set up the modelview matrix
	MatrixIdentity(ModelView);
	MatrixTranslate(PanX, PanY, Zoom, ModelView);

	QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	QuatMultiply(QuatX, QuatY, Quat);
	QuatMatrix(Quat, ModelView);

	// Generate an inverse modelview matrix
	MatrixInverse(ModelView, ubo.mvinv);

	// Generate a modelview+projection matrix
	MatrixMult(ModelView, Projection, ubo.mvp);

	// Set light uniform positions and color
	ubo.Light0_Pos[0]=sinf(fTime)*150.0f;
	ubo.Light0_Pos[1]=-25.0f;
	ubo.Light0_Pos[2]=cosf(fTime)*150.0f;
	ubo.Light0_Pos[3]=1.0f/256.0f;
	ubo.Light0_Kd[0]=1.0f;
	ubo.Light0_Kd[1]=0.0f;
	ubo.Light0_Kd[2]=0.0f;
	ubo.Light0_Kd[3]=1.0f;

	ubo.Light1_Pos[0]=cosf(fTime)*100.0f;
	ubo.Light1_Pos[1]=50.0f;
	ubo.Light1_Pos[2]=sinf(fTime)*100.0f;
	ubo.Light1_Pos[3]=1.0f/256.0f;
	ubo.Light1_Kd[0]=0.0f;
	ubo.Light1_Kd[1]=1.0f;
	ubo.Light1_Kd[2]=0.0f;
	ubo.Light1_Kd[3]=1.0f;

	ubo.Light2_Pos[0]=cosf(fTime)*100.0f;
	ubo.Light2_Pos[1]=-80.0f;
	ubo.Light2_Pos[2]=-15.0f;
	ubo.Light2_Pos[3]=1.0f/256.0f;
	ubo.Light2_Kd[0]=0.0f;
	ubo.Light2_Kd[1]=0.0f;
	ubo.Light2_Kd[2]=1.0f;
	ubo.Light2_Kd[3]=1.0f;

	// Copy uniform data to GPU
	memcpy(uniformBufferPtr, &ubo, sizeof(ubo));

	vkWaitForFences(device, 1, &frameFences[index], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &frameFences[index]);

	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentCompleteSemaphores[index], VK_NULL_HANDLE, &imageIndex);

	// Start recording the commands
	vkBeginCommandBuffer(commandBuffers[index], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(commandBuffers[index], &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=renderPass,
		.framebuffer=frameBuffers[imageIndex],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]) { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0 } },
		.renderArea.offset=(VkOffset2D) { 0, 0 },
		.renderArea.extent=swapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdSetViewport(commandBuffers[index], 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)swapchainExtent.width, (float)swapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffers[index], 0, 1, &(VkRect2D) { { 0, 0 }, swapchainExtent});

	// Draw the models
	for(i=0;i<4;i++)
	{
		// Bind per-model destriptor set, this changes texture binding
		vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet[i], 0, NULL);

		// Bind model data buffers and draw the triangles
		for(j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindVertexBuffers(commandBuffers[index], 0, 1, (VkBuffer[]) { Model[i].Mesh[j].Buffer }, &(VkDeviceSize) { 0 });
			vkCmdBindIndexBuffer(commandBuffers[index], Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(commandBuffers[index], Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
		}
	}
	// ---

	// UI overlay stuff really should have it's own render pass
	// to keep it independent from the main "renderer", but this works for now.
	Font_Print(commandBuffers[index], 0, 0, "FPS: %0.1f", fps);

	vkCmdEndRenderPass(commandBuffers[index]);

	vkEndCommandBuffer(commandBuffers[index]);

	// Sumit command queue
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&presentCompleteSemaphores[index],
		.commandBufferCount=1,
		.pCommandBuffers=&commandBuffers[index],
	}, frameFences[index]);

	// And present it to the screen
	vkQueuePresentKHR(queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.swapchainCount=1,
		.pSwapchains=&swapchain,
		.pImageIndices=&imageIndex,
	});

	frameIndex++;
}

int CreateVulkan(void)
{
	uint32_t physicalDeviceCount;
	VkPhysicalDevice deviceHandles[MAX_DEVICE_COUNT];
	VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_COUNT];
	uint32_t formatCount=1, presentModeCount=1, i, j;
	VkPresentModeKHR presentMode=VK_PRESENT_MODE_FIFO_KHR;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	uint32_t swapchainImageCount=FRAME_COUNT;

	if(vkCreateInstance(&(VkInstanceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo=&(VkApplicationInfo)
		{
			.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName=szAppName,
			.applicationVersion=VK_MAKE_VERSION(1, 0, 0),
			.pEngineName=szAppName,
			.engineVersion=VK_MAKE_VERSION(1, 0, 0),
			.apiVersion=VK_API_VERSION_1_0,
		},
		.enabledExtensionCount=2,
		.ppEnabledExtensionNames=(const char *const []) { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME },
	}, 0, &instance)!=VK_SUCCESS)
		return 1;

	if(vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance=GetModuleHandle(0),
		.hwnd=hWnd,
	}, VK_NULL_HANDLE, &surface)!=VK_SUCCESS)
		return 1;

	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
	physicalDeviceCount=physicalDeviceCount>MAX_DEVICE_COUNT?MAX_DEVICE_COUNT:physicalDeviceCount;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

	for(i=0;i<physicalDeviceCount;i++)
	{
		uint32_t queueFamilyCount=0;

		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, NULL);
		queueFamilyCount=queueFamilyCount>MAX_QUEUE_COUNT?MAX_QUEUE_COUNT:queueFamilyCount;
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);

		for(j=0;j<queueFamilyCount;j++)
		{
			VkBool32 supportsPresent=VK_FALSE;

			vkGetPhysicalDeviceSurfaceSupportKHR(deviceHandles[i], j, surface, &supportsPresent);

			if(supportsPresent&&(queueFamilyProperties[j].queueFlags&VK_QUEUE_GRAPHICS_BIT))
			{
				queueFamilyIndex=j;
				physicalDevice=deviceHandles[i];

				break;
			}
		}

		if(physicalDevice)
			break;
	}

	if(vkCreateDevice(physicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.enabledExtensionCount=1,
		.ppEnabledExtensionNames=(const char *const []) { VK_KHR_SWAPCHAIN_EXTENSION_NAME },
		.queueCreateInfoCount=1,
		.pQueueCreateInfos=(VkDeviceQueueCreateInfo[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex=queueFamilyIndex,
				.queueCount=1,
				.pQueuePriorities=(const float[]) { 1.0f }
			}
		}
	}, VK_NULL_HANDLE, &device)!=VK_SUCCESS)
		return 1;

	// Get device physical memory properties
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemProperties);

	// Get device queue
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	// Calling this by it's self will give a validation warning, I don't care.
	// Get supported surface formats (formatCount already == 1, so this will only return one format)
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, &surfaceFormat);

	// If no format is specified, set it
	if(surfaceFormat.format==VK_FORMAT_UNDEFINED)
		surfaceFormat.format=VK_FORMAT_B8G8R8A8_UNORM;

	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, &presentMode);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

	swapchainExtent=surfaceCapabilities.currentExtent;

	if(swapchainExtent.width==UINT32_MAX)
	{
		swapchainExtent.width=clamp_u32(Width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		swapchainExtent.height=clamp_u32(Height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
	}

	if(vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface=surface,
		.minImageCount=FRAME_COUNT,
		.imageFormat=surfaceFormat.format,
		.imageColorSpace=surfaceFormat.colorSpace,
		.imageExtent=swapchainExtent,
		.imageArrayLayers=1,
		.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.preTransform=surfaceCapabilities.currentTransform,
		.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode=presentMode,
		.clipped=VK_TRUE,
	}, 0, &swapchain)!=VK_SUCCESS)
		return 1;

	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, VK_NULL_HANDLE);
	vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);

	vkCreateCommandPool(device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex=queueFamilyIndex,
	}, 0, &commandPool);

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=FRAME_COUNT,
	}, commandBuffers);

	for(i=0;i<FRAME_COUNT;i++)
	{
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &frameFences[i]);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo) { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &presentCompleteSemaphores[i]);
	}

	createRenderPass();
	createFramebuffers();
	createPipeline();
	createUploadBuffer();

	return 0;
}

void DestroyVulkan(void)
{
	uint32_t i, j;

	vkDeviceWaitIdle(device);

	Font_Destroy();

	for(i=0;i<8;i++)
	{
		vkDestroySampler(device, Textures[i].sampler, VK_NULL_HANDLE);
		vkDestroyImageView(device, Textures[i].view, VK_NULL_HANDLE);
		vkDestroyImage(device, Textures[i].image, VK_NULL_HANDLE);
		vkFreeMemory(device, Textures[i].deviceMemory, VK_NULL_HANDLE);
	}

	for(i=0;i<4;i++)
	{
		for(j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkDestroyBuffer(device, Model[i].Mesh[j].Buffer, VK_NULL_HANDLE);
			vkFreeMemory(device, Model[i].Mesh[j].BufferMemory, VK_NULL_HANDLE);

			vkDestroyBuffer(device, Model[i].Mesh[j].IndexBuffer, VK_NULL_HANDLE);
			vkFreeMemory(device, Model[i].Mesh[j].IndexBufferMemory, VK_NULL_HANDLE);
		}
	}

	vkDestroyBuffer(device, uniformBuffer, VK_NULL_HANDLE);
	vkFreeMemory(device, uniformBufferMemory, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(device, descriptorPool, VK_NULL_HANDLE);

	vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);

	vkDestroyImageView(device, depthImageView, VK_NULL_HANDLE);
	vkDestroyImage(device, depthImage, VK_NULL_HANDLE);
	vkFreeMemory(device, depthMemory, VK_NULL_HANDLE);

	for(i=0;i<FRAME_COUNT;i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);
		vkDestroyImageView(device, swapchainImageViews[i], VK_NULL_HANDLE);

		vkDestroyFence(device, frameFences[i], VK_NULL_HANDLE);

		vkDestroySemaphore(device, presentCompleteSemaphores[i], VK_NULL_HANDLE);
	}

	vkDestroyRenderPass(device, renderPass, VK_NULL_HANDLE);

	vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);

	vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);
	vkDestroyDevice(device, VK_NULL_HANDLE);
	vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);
	vkDestroyInstance(instance, VK_NULL_HANDLE);
}

//----------------------------------------------------------

// Windows junk beyond here:

unsigned __int64 rdtsc(void)
{
	return __rdtsc();
}

unsigned __int64 GetFrequency(void)
{
	unsigned __int64 TimeStart, TimeStop, TimeFreq;
	unsigned __int64 StartTicks, StopTicks;
	volatile unsigned __int64 i;

	QueryPerformanceFrequency((LARGE_INTEGER *)&TimeFreq);

	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStart);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStop);

	return (StopTicks-StartTicks)*TimeFreq/(TimeStop-TimeStart);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	RECT Rect;

	wc.style=CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wc.lpfnWndProc=WndProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szAppName;

	RegisterClass(&wc);

	SetRect(&Rect, 0, 0, Width, Height);
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);

	hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);

	Frequency=GetFrequency();

	if(CreateVulkan())
	{
		MessageBox(hWnd, "Vulkan init failed.", "Error", MB_OK);
		return -1;
	}

	while(!Done)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				Done=1;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			StartTime=rdtsc();
			draw_frame();
			EndTime=rdtsc();

			fTimeStep=(float)(EndTime-StartTime)/Frequency;
			fTime+=fTimeStep;
			avgfps+=1.0f/fTimeStep;

			if(Frames++>100)
			{
				fps=avgfps/Frames;
				avgfps=0.0f;
				Frames=0;
			}
		}
	}

	DestroyVulkan();

	DestroyWindow(hWnd);

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static POINT old;
	POINT pos, delta;

	switch(uMsg)
	{
		case WM_CREATE:
			break;

		case WM_CLOSE:
			PostQuitMessage(0);
			break;

		case WM_DESTROY:
			break;

		case WM_SIZE:
			Width=LOWORD(lParam);
			Height=HIWORD(lParam);

			// Kinda feels like jank, but WM_SIZE pops up on window creation, before Vulkan is initialized.
			if(device)
			{
				VkPresentModeKHR presentMode=VK_PRESENT_MODE_FIFO_KHR;
				uint32_t presentModeCount=1, i;
				VkSurfaceCapabilitiesKHR surfaceCapabilities;
				uint32_t swapchainImageCount=FRAME_COUNT;

				vkDeviceWaitIdle(device);

				vkDestroyImage(device, depthImage, VK_NULL_HANDLE);
				vkFreeMemory(device, depthMemory, VK_NULL_HANDLE);
				vkDestroyImageView(device, depthImageView, VK_NULL_HANDLE);

				for(i=0;i<FRAME_COUNT;i++)
				{
					vkDestroyFramebuffer(device, frameBuffers[i], NULL);
					vkDestroyImageView(device, swapchainImageViews[i], NULL);
				}

				vkDestroySwapchainKHR(device, swapchain, 0);

				vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, &presentMode);

				vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

				swapchainExtent=surfaceCapabilities.currentExtent;

				if(swapchainExtent.width==UINT32_MAX)
				{
					swapchainExtent.width=clamp_u32(Width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
					swapchainExtent.height=clamp_u32(Height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
				}

				if(vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR)
				{
					.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
					.surface=surface,
					.minImageCount=FRAME_COUNT,
					.imageFormat=surfaceFormat.format,
					.imageColorSpace=surfaceFormat.colorSpace,
					.imageExtent=swapchainExtent,
					.imageArrayLayers=1,
					.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
					.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
					.preTransform=surfaceCapabilities.currentTransform,
					.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
					.presentMode=presentMode,
					.clipped=VK_TRUE,
				}, 0, &swapchain)!=VK_SUCCESS)
					return 0;

				vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, VK_NULL_HANDLE);
				vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);

				createFramebuffers();
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			ShowCursor(FALSE);

			GetCursorPos(&pos);
			old.x=pos.x;
			old.y=pos.y;
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			ShowCursor(TRUE);
			ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			GetCursorPos(&pos);

			if(!wParam)
			{
				old.x=pos.x;
				old.y=pos.y;
				break;
			}

			delta.x=pos.x-old.x;
			delta.y=old.y-pos.y;

			if(!delta.x&&!delta.y)
				break;

			SetCursorPos(old.x, old.y);

			switch(wParam)
			{
				case MK_LBUTTON:
					RotateX+=delta.x;
					RotateY-=delta.y;
					break;

				case MK_MBUTTON:
					PanX+=delta.x;
					PanY+=delta.y;
					break;

				case MK_RBUTTON:
					Zoom+=delta.y;
					break;
			}
			break;

		case WM_KEYDOWN:
			Key[wParam]=1;

			switch(wParam)
			{
				case VK_ESCAPE:
					PostQuitMessage(0);
					break;

				default:
					break;
			}
			break;

		case WM_KEYUP:
			Key[wParam]=0;
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
