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

int Width=1280, Height=720;
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
	MAX_FRAME_COUNT=3,
};

float ModelView[16], Projection[16];
float QuatX[4], QuatY[4], Quat[4];

struct
{
	float mvp[16];
	float eye[4];

	float Light0_Pos[4];
	float Light0_Kd[4];
	float Light1_Pos[4];
	float Light1_Kd[4];
	float Light2_Pos[4];
	float Light2_Kd[4];
} ubo;

VkBuffer uniformBuffer;
VkDeviceMemory uniformBufferMemory;
void *uniformBufferPtr;

VkInstance instance=VK_NULL_HANDLE;

VkDevice device=VK_NULL_HANDLE;
VkPhysicalDeviceMemoryProperties deviceMemProperties;

VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;

VkSurfaceKHR surface=VK_NULL_HANDLE;

uint32_t queueFamilyIndex;
VkQueue queue=VK_NULL_HANDLE;

// Swapchain
VkSwapchainKHR swapchain=VK_NULL_HANDLE;

VkExtent2D swapchainExtent;
VkSurfaceFormatKHR surfaceFormat;
VkFormat depthFormat=VK_FORMAT_D32_SFLOAT;

uint32_t imageCount;

VkImage swapchainImage[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkImageView swapchainImageView[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

VkFramebuffer frameBuffers[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

// Depth buffer handles
VkImage depthImage=VK_NULL_HANDLE;
VkDeviceMemory depthMemory=VK_NULL_HANDLE;
VkImageView depthImageView=VK_NULL_HANDLE;

VkRenderPass renderPass=VK_NULL_HANDLE;

VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;
VkPipeline pipeline=VK_NULL_HANDLE;

VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
VkDescriptorSet descriptorSet[4]={ VK_NULL_HANDLE, };
VkDescriptorSetLayout descriptorSetLayout=VK_NULL_HANDLE;

uint32_t frameIndex=0;

VkCommandPool commandPool=VK_NULL_HANDLE;
VkCommandBuffer commandBuffers[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

VkFence frameFences[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkSemaphore presentCompleteSemaphores[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkSemaphore renderCompleteSemaphores[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

// Shadow cubemap stuff
//
int32_t shadowCubeSize=1024;

VkFramebuffer shadowFrameBuffer;

// Frame buffer color
VkImage shadowColorImage;
VkDeviceMemory shadowColorMemory;
VkImageView shadowColorImageView;

// Frame buffer depth
VkImage shadowDepthImage;
VkDeviceMemory shadowDepthMemory;
VkImageView shadowDepthImageView;

VkPipeline shadowPipeline;
VkPipelineLayout shadowPipelineLayout;

VkRenderPass shadowRenderPass;

// Shadow depth cubemap texture
typedef struct
{
	VkSampler shadowSampler;
	VkImage shadowImage;
	VkDeviceMemory shadowMemory;
	VkImageView shadowImageView;
} ShadowBuffer_t;

ShadowBuffer_t shadowbuf[3];

struct
{
	float mvp[16];
	float Light_Pos[4];
} shadow_ubo;

VkFence shadowFence=VK_NULL_HANDLE;

void initShadowCubeMap(VkSampler *sampler, VkImage *image, VkDeviceMemory *memory, VkImageView *imageView)
{
	VkFormat Format=VK_FORMAT_R32_SFLOAT;
	VkCommandBuffer layoutCmd;

	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, Format, 1, 6, shadowCubeSize, shadowCubeSize,
		image, memory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &layoutCmd);

	vkBeginCommandBuffer(layoutCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(layoutCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=*image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=6,
		},
		.srcAccessMask=0,
		.dstAccessMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	});

	vkEndCommandBuffer(layoutCmd);
		
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&layoutCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &layoutCmd);

	vkCreateSampler(device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy=1.0f,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=1.0f,
		.maxAnisotropy=1.0,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, sampler);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE,
		.format=Format,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6,
		.subresourceRange.levelCount=1,
		.image=*image,
	}, VK_NULL_HANDLE, imageView);
}

void initShadowFramebuffer(void)
{
	VkCommandBuffer copyCmd;
	VkFormat Format=VK_FORMAT_R32_SFLOAT;

	//Color
	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, Format, 1, 1, shadowCubeSize, shadowCubeSize,
		&shadowColorImage, &shadowColorMemory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);

	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=shadowColorImage,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=0,
		.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	});

	vkEndCommandBuffer(copyCmd);
		
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=Format,
		.components=
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=shadowColorImage,
	}, VK_NULL_HANDLE, &shadowColorImageView);

	// Depth
	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, depthFormat, 1, 1, shadowCubeSize, shadowCubeSize,
		&shadowDepthImage, &shadowDepthMemory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=shadowDepthImage,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=0,
		.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	});

	vkEndCommandBuffer(copyCmd);
		
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=depthFormat,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=shadowDepthImage,
	}, VK_NULL_HANDLE, &shadowDepthImageView);

	vkCreateRenderPass(device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=Format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=depthFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
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
	}, 0, &shadowRenderPass);

	vkCreateFramebuffer(device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=shadowRenderPass,
		.attachmentCount=2,
		.pAttachments=(VkImageView[]) { shadowColorImageView, shadowDepthImageView },
		.width=shadowCubeSize,
		.height=shadowCubeSize,
		.layers=1,
	}, 0, &shadowFrameBuffer);
}

int initShadowPipeline(void)
{
	VkShaderModule vertexShader=vkuCreateShaderModule(device, "distance_v.spv");
	VkShaderModule fragmentShader=vkuCreateShaderModule(device, "distance_f.spv");

	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(float)*(16+4),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &shadowPipelineLayout);

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
			.vertexAttributeDescriptionCount=1,
			.pVertexAttributeDescriptions=(VkVertexInputAttributeDescription[])
			{
				{
					.location=0,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=0
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
			.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
		.layout=shadowPipelineLayout,
		.renderPass=shadowRenderPass,
	}, 0, &shadowPipeline);

	vkDestroyShaderModule(device, vertexShader, 0);
	vkDestroyShaderModule(device, fragmentShader, 0);

	return 1;
}
//
// ---

int createFramebuffers(void)
{
	uint32_t i;

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
				.format=depthFormat,
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

	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, depthFormat, 1, 1, Width, Height,
		&depthImage, &depthMemory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=depthImage,
		.format=depthFormat,
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

	for(i=0;i<imageCount;i++)
	{
		vkCreateFramebuffer(device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=renderPass,
			.attachmentCount=2,
			.pAttachments=(VkImageView[]) { swapchainImageView[i], depthImageView },
			.width=swapchainExtent.width,
			.height=swapchainExtent.height,
			.layers=1,
		}, 0, &frameBuffers[i]);
	}

	return 1;
}

int createPipeline(void)
{
	VkShaderModule vertexShader=vkuCreateShaderModule(device, "lighting_v.spv");
	VkShaderModule fragmentShader=vkuCreateShaderModule(device, "lighting_f.spv");

	vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.maxSets=NUM_MODELS,
		.poolSizeCount=2,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=NUM_MODELS,
			},
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=NUM_MODELS*6,
			},
		},
	}, NULL, &descriptorPool);

	vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.bindingCount=6,
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
			{
				.binding=3,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=4,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=5,
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
					.offset=0
				},
				{
					.location=1,
					.binding=0,
					.format=VK_FORMAT_R32G32_SFLOAT,
					.offset=sizeof(float)*3
				},
				{
					.location=2,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*5
				},
				{
					.location=3,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*8
				},
				{
					.location=4,
					.binding=0,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*11
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
				.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+0];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+1];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(device, stagingBufferMemory);

		vkuCopyBuffer(device, queue, commandPool, stagingBuffer, Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);
	}
}

VkCommandBuffer shadowCmd=VK_NULL_HANDLE;
HANDLE shadowThread=NULL;

void shadowUpdateCubemap(VkCommandBuffer commandBuffer, ShadowBuffer_t shadow, float Light_Pos[4]);

DWORD WINAPI RenderShadows(void *arg)
{
	while(!Done)
	{
		vkWaitForFences(device, 1, &shadowFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &shadowFence);

		vkBeginCommandBuffer(shadowCmd, &(VkCommandBufferBeginInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
		});

		shadowUpdateCubemap(shadowCmd, shadowbuf[0], ubo.Light0_Pos);
		shadowUpdateCubemap(shadowCmd, shadowbuf[1], ubo.Light1_Pos);
		shadowUpdateCubemap(shadowCmd, shadowbuf[2], ubo.Light2_Pos);

		vkEndCommandBuffer(shadowCmd);
		
		vkQueueSubmit(queue, 1, &(VkSubmitInfo)
		{
			.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount=1,
			.pCommandBuffers=&shadowCmd,
		}, shadowFence);
	}

	return 0;
}

int Init(void)
{
	uint32_t i;

	// Create primary frame buffers, depth image, and renderpass
	createFramebuffers();

	// Create main render pipeline
	createPipeline();

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
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT], "hellknight.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT_NORMAL], "hellknight_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_PINKY], "pinky.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_PINKY_NORMAL], "pinky_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_FATTY], "fatty.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_FATTY_NORMAL], "fatty_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_LEVEL], "tile.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_LEVEL_NORMAL], "tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP);

	// Uniform data buffer and pointer mapping
	vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
		&uniformBuffer, &uniformBufferMemory,
		sizeof(ubo),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(device, uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &uniformBufferPtr);

	initShadowCubeMap(&shadowbuf[0].shadowSampler, &shadowbuf[0].shadowImage, &shadowbuf[0].shadowMemory, &shadowbuf[0].shadowImageView);
	initShadowCubeMap(&shadowbuf[1].shadowSampler, &shadowbuf[1].shadowImage, &shadowbuf[1].shadowMemory, &shadowbuf[1].shadowImageView);
	initShadowCubeMap(&shadowbuf[2].shadowSampler, &shadowbuf[2].shadowImage, &shadowbuf[2].shadowMemory, &shadowbuf[2].shadowImageView);

	initShadowFramebuffer();
	initShadowPipeline();

	// Allocate and update descriptor sets for each model, each model has a different texture, so different sampler bindings are needed.
	for(i=0;i<NUM_MODELS;i++)
	{
		vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext=NULL,
			.descriptorPool=descriptorPool,
			.descriptorSetCount=1,
			.pSetLayouts=&descriptorSetLayout
		}, &descriptorSet[i]);

		vkUpdateDescriptorSets(device, 6, (VkWriteDescriptorSet[])
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
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=3,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[0].shadowImageView,
					.sampler=shadowbuf[0].shadowSampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=4,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[1].shadowImageView,
					.sampler=shadowbuf[1].shadowSampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=5,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[2].shadowImageView,
					.sampler=shadowbuf[2].shadowSampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
		}, 0, VK_NULL_HANDLE);
	}

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,
		.commandBufferCount=1,
	}, &shadowCmd);

	vkCreateFence(device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &shadowFence);

	shadowThread=CreateThread(NULL, 0, RenderShadows, NULL, 0, NULL);
 
	return TRUE;
}

void shadowUpdateCubemap(VkCommandBuffer commandBuffer, ShadowBuffer_t shadow, float Light_Pos[4])
{
	int i, j, face;

	MatrixIdentity(Projection);
	InfPerspective(90.0f, 1.0f, 0.01f, 1, Projection);

	for(face=0;face<6;face++)
	{
		MatrixIdentity(ModelView);

		QuatX[0]=0.0f; QuatX[1]=0.0f; QuatX[2]=0.0f; QuatX[3]=1.0f;
		QuatY[0]=0.0f; QuatY[1]=0.0f; QuatY[2]=0.0f; QuatY[3]=1.0f;

		switch(face)
		{
			case 0:
				QuatAngle(90.0f, 0.0f, 1.0f, 0.0f, QuatX);
				QuatAngle(180.0f, 1.0f, 0.0f, 0.0f, QuatY);
				break;

			case 1:
				QuatAngle(-90.0f, 0.0f, 1.0f, 0.0f, QuatX);
				QuatAngle(180.0f, 1.0f, 0.0f, 0.0f, QuatY);
				break;

			case 2:
				QuatAngle(90.0f, 1.0f, 0.0f, 0.0f, QuatX);
				break;

			case 3:
				QuatAngle(-90.0f, 1.0f, 0.0f, 0.0f, QuatX); 
				break;

			case 4:
				QuatAngle(0.0f, 1.0f, 0.0f, 0.0f, QuatX);
				break;

			case 5:
				QuatAngle(180.0f, 0.0f, 1.0f, 0.0f, QuatX);
				break;
		}

		QuatMultiply(QuatX, QuatY, Quat);
		QuatMatrix(Quat, ModelView);

		MatrixTranslate(-Light_Pos[0], -Light_Pos[1], -Light_Pos[2], ModelView);

		MatrixMult(ModelView, Projection, shadow_ubo.mvp);

		shadow_ubo.Light_Pos[0]=Light_Pos[0];
		shadow_ubo.Light_Pos[1]=Light_Pos[1];
		shadow_ubo.Light_Pos[2]=Light_Pos[2];
		shadow_ubo.Light_Pos[3]=Light_Pos[3];

		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass=shadowRenderPass,
			.framebuffer=shadowFrameBuffer,
			.clearValueCount=2,
			.pClearValues=(VkClearValue[])
			{
				{ .color.float32[0]=0.0f, .color.float32[1]=0.0f, .color.float32[2]=0.0f, .color.float32[3]=1.0f },
				{ .depthStencil.depth=1.0f, .depthStencil.stencil=0 }
			},
			.renderArea.offset=(VkOffset2D) { .x=0, .y=0 },
			.renderArea.extent=(VkExtent2D)	{ .width=shadowCubeSize, .height=shadowCubeSize },
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdPushConstants(commandBuffer, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*(16+4), &shadow_ubo);

		// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);

		vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)shadowCubeSize, (float)shadowCubeSize, 0.0f, 1.0f });
		vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { shadowCubeSize, shadowCubeSize } });

		// Draw the models
		for(i=0;i<NUM_MODELS;i++)
		{
			// Bind model data buffers and draw the triangles
			for(j=0;j<Model[i].NumMesh;j++)
			{
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
				vkCmdBindIndexBuffer(commandBuffer, Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(commandBuffer, Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(commandBuffer);

		// Change frame buffer image layout to source transfer
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadowColorImage,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=0,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		});

		// Change cubemap texture image face to transfer destination
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadow.shadowImage,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=face,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		});

		// Copy image from framebuffer to cube face
		vkCmdCopyImage(commandBuffer, shadowColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, shadow.shadowImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageCopy)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.srcSubresource.baseArrayLayer=0,
			.srcSubresource.mipLevel=0,
			.srcSubresource.layerCount=1,
			.srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.dstSubresource.baseArrayLayer=face,
			.dstSubresource.mipLevel=0,
			.dstSubresource.layerCount=1,
			.dstOffset={ 0, 0, 0 },
			.extent.width=shadowCubeSize,
			.extent.height=shadowCubeSize,
			.extent.depth=1,
		});

		// Change frame buffer image layout back to color arrachment
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadowColorImage,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=0,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		});

		// Change cubemap texture image face back to shader read-only (for use in the main render shader)
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadow.shadowImage,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=face,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		});
	}
}

void Render(void)
{
	uint32_t index=frameIndex%imageCount;
	uint32_t imageIndex;
	int i, j;

	// Generate the projection matrix
	MatrixIdentity(Projection);
	InfPerspective(90.0f, (float)Width/Height, 0.01f, 1, Projection);

	// Set up the modelview matrix
	MatrixIdentity(ModelView);
	MatrixTranslate(PanX, PanY, Zoom, ModelView);

	QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	QuatMultiply(QuatX, QuatY, Quat);
	QuatMatrix(Quat, ModelView);

	// Generate an inverse modelview matrix (only really need the last 3 from the calculation)
//	MatrixInverse(ModelView, ubo.mvinv);
	ubo.eye[0]=-(ModelView[12]*ModelView[ 0])-(ModelView[13]*ModelView[ 1])-(ModelView[14]*ModelView[ 2]);
	ubo.eye[1]=-(ModelView[12]*ModelView[ 4])-(ModelView[13]*ModelView[ 5])-(ModelView[14]*ModelView[ 6]);
	ubo.eye[2]=-(ModelView[12]*ModelView[ 8])-(ModelView[13]*ModelView[ 9])-(ModelView[14]*ModelView[10]);

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

	// Copy uniform data to the Vulkan UBO buffer
	memcpy(uniformBufferPtr, &ubo, sizeof(ubo));

	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentCompleteSemaphores[index], VK_NULL_HANDLE, &imageIndex);

	vkWaitForFences(device, 1, &frameFences[index], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &frameFences[index]);

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
		.pClearValues=(VkClearValue[])
		{
			{
				.color.float32[0]=0.0f,
				.color.float32[1]=0.0f,
				.color.float32[2]=0.0f,
				.color.float32[3]=1.0f
			},
			{
				.depthStencil.depth=1.0f,
				.depthStencil.stencil=0
			}
		},
		.renderArea.offset=(VkOffset2D)
		{
			.x=0,
			.y=0
		},
		.renderArea.extent=swapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	vkCmdSetViewport(commandBuffers[index], 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)swapchainExtent.width, (float)swapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffers[index], 0, 1, &(VkRect2D) { { 0, 0 }, swapchainExtent});

	// Draw the models
//	i=0;
	for(i=0;i<NUM_MODELS;i++)
	{
		// Bind per-model destriptor set, this changes texture binding
		vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet[i], 0, NULL);

		// Bind model data buffers and draw the triangles
		for(j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindVertexBuffers(commandBuffers[index], 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
			vkCmdBindIndexBuffer(commandBuffers[index], Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(commandBuffers[index], Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
		}
	}
	// ---

	// Should UI overlay stuff have it's own render pass?
	// Maybe even separate thread?
	Font_Print(commandBuffers[index], 0, 0, "FPS: %0.1f", fps);

	vkCmdEndRenderPass(commandBuffers[index]);

	vkEndCommandBuffer(commandBuffers[index]);

	// Sumit command queue
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&presentCompleteSemaphores[index],
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&renderCompleteSemaphores[index],
		.commandBufferCount=1,
		.pCommandBuffers=&commandBuffers[index],
	}, frameFences[index]);

	// And present it to the screen
	vkQueuePresentKHR(queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&renderCompleteSemaphores[index],
		.swapchainCount=1,
		.pSwapchains=&swapchain,
		.pImageIndices=&imageIndex,
	});

	frameIndex++;
}

void createSwapchain(uint32_t width, uint32_t height, int vsync)
{
	uint32_t i, formatCount, presentModeCount;
	uint32_t desiredNumberOfSwapchainImages;
	VkSurfaceFormatKHR *surfaceFormats=NULL;
	VkSurfaceCapabilitiesKHR surfCaps;
	VkPresentModeKHR swapchainPresentMode=VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR *presentModes=NULL;
	VkSurfaceTransformFlagsKHR preTransform;
	VkCompositeAlphaFlagBitsKHR compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[]=
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	VkImageUsageFlags imageUsage=0;

	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, VK_NULL_HANDLE);

	surfaceFormats=(VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR)*formatCount);

	if(surfaceFormats==NULL)
		return;

	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats);

	// If no format is specified, find a 32bit RGBA format
	if(surfaceFormats[0].format==VK_FORMAT_UNDEFINED)
		surfaceFormat.format=VK_FORMAT_R8G8B8A8_SNORM;
	// Otherwise the first format is the current surface format
	else
		surfaceFormat=surfaceFormats[0];

	FREE(surfaceFormats);

	// Get physical device surface properties and formats
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);

	// Get available present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);

	presentModes=(VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR)*presentModeCount);

	if(presentModes==NULL)
		return;

	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

	// Set swapchain extents to the surface width/height
	swapchainExtent.width=width;
	swapchainExtent.height=height;

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if(!vsync)
	{
		for(i=0;i<presentModeCount;i++)
		{
			if(presentModes[i]==VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapchainPresentMode=VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if((swapchainPresentMode!=VK_PRESENT_MODE_MAILBOX_KHR)&&(presentModes[i]==VK_PRESENT_MODE_IMMEDIATE_KHR))
				swapchainPresentMode=VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	FREE(presentModes);

	// Determine the number of images
	desiredNumberOfSwapchainImages=surfCaps.minImageCount+1;

	if((surfCaps.maxImageCount>0)&&(desiredNumberOfSwapchainImages>surfCaps.maxImageCount))
		desiredNumberOfSwapchainImages=surfCaps.maxImageCount;

	// Find the transformation of the surface
	if(surfCaps.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		// We prefer a non-rotated transform
		preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	else
		preTransform=surfCaps.currentTransform;

	// Find a supported composite alpha format (not all devices support alpha opaque)
	// Simply select the first composite alpha format available
	for(i=0;i<4;i++)
	{
		if(surfCaps.supportedCompositeAlpha&compositeAlphaFlags[i])
		{
			compositeAlpha=compositeAlphaFlags[i];
			break;
		}
	}

	// Enable transfer source on swap chain images if supported
	if(surfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Enable transfer destination on swap chain images if supported
	if(surfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext=VK_NULL_HANDLE,
		.surface=surface,
		.minImageCount=desiredNumberOfSwapchainImages,
		.imageFormat=surfaceFormat.format,
		.imageColorSpace=surfaceFormat.colorSpace,
		.imageExtent={ swapchainExtent.width, swapchainExtent.height },
		.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|imageUsage,
		.preTransform=preTransform,
		.imageArrayLayers=1,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=VK_NULL_HANDLE,
		.presentMode=swapchainPresentMode,
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.clipped=VK_TRUE,
		.compositeAlpha=compositeAlpha,
	}, VK_NULL_HANDLE, &swapchain);

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, VK_NULL_HANDLE);

	// Get the swap chain images
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImage);

	// Get the swap chain buffers containing the image and imageview
	for(i=0;i<imageCount;i++)
	{
		vkCreateImageView(device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=VK_NULL_HANDLE,
			.image=swapchainImage[i],
			.format=surfaceFormat.format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
		}, VK_NULL_HANDLE, &swapchainImageView[i]);
	}
}

int CreateVulkan(void)
{
	uint32_t physicalDeviceCount;
	VkPhysicalDevice *deviceHandles=NULL;
	VkQueueFamilyProperties *queueFamilyProperties=NULL;
	uint32_t i, j;
	VkPresentModeKHR presentMode=VK_PRESENT_MODE_FIFO_KHR;

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

	// Get the number of physical devices in the system
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE);

	// Allocate an array of handles
	deviceHandles=(VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice)*physicalDeviceCount);

	if(deviceHandles==NULL)
		return 1;

	// Get the handles to the devices
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

	for(i=0;i<physicalDeviceCount;i++)
	{
		uint32_t queueFamilyCount;

		// Get the number of queue families for this device
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, VK_NULL_HANDLE);

		// Allocate the memory for the structs 
		queueFamilyProperties=(VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties)*queueFamilyCount);

		if(queueFamilyProperties==NULL)
			return 1;

		// Get the queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);

		// Find a queue index on a device that supports both graphics rendering and present support
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

		// Found device?
		if(physicalDevice)
			break;
	}

	// Free allocated handles
	FREE(queueFamilyProperties);
	FREE(deviceHandles);

	// Create the logical device from the physical device and queue index from above
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

	// Create swapchain
	createSwapchain(Width, Height, VK_FALSE);

	// Create command pool
	vkCreateCommandPool(device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex=queueFamilyIndex,
	}, 0, &commandPool);

	// Allocate the command buffers we will be rendering into
	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=imageCount,
	}, commandBuffers);

	for(i=0;i<imageCount;i++)
	{
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &frameFences[i]);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo) { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &presentCompleteSemaphores[i]);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo) { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &renderCompleteSemaphores[i]);
	}

	return 0;
}

void DestroyVulkan(void)
{
	uint32_t i, j;

	vkDeviceWaitIdle(device);

	CloseHandle(shadowThread);
	vkWaitForFences(device, 1, &shadowFence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(device, shadowFence, VK_NULL_HANDLE);

	// Shadow stuff
	vkDestroyPipeline(device, shadowPipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, shadowPipelineLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(device, shadowFrameBuffer, VK_NULL_HANDLE);

	vkDestroyRenderPass(device, shadowRenderPass, VK_NULL_HANDLE);

	// Frame buffer color
	vkDestroyImageView(device, shadowColorImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowColorMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowColorImage, VK_NULL_HANDLE);

	// Frame buffer depth
	vkDestroyImageView(device, shadowDepthImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowDepthMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowDepthImage, VK_NULL_HANDLE);

	// Shadow depth cubemap texture
	vkDestroySampler(device, shadowbuf[0].shadowSampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[0].shadowImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[0].shadowMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[0].shadowImage, VK_NULL_HANDLE);

	vkDestroySampler(device, shadowbuf[1].shadowSampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[1].shadowImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[1].shadowMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[1].shadowImage, VK_NULL_HANDLE);

	vkDestroySampler(device, shadowbuf[2].shadowSampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[2].shadowImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[2].shadowMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[2].shadowImage, VK_NULL_HANDLE);
	// ---

	Font_Destroy();

	for(i=0;i<NUM_TEXTURES;i++)
	{
		vkDestroySampler(device, Textures[i].sampler, VK_NULL_HANDLE);
		vkDestroyImageView(device, Textures[i].view, VK_NULL_HANDLE);
		vkFreeMemory(device, Textures[i].deviceMemory, VK_NULL_HANDLE);
		vkDestroyImage(device, Textures[i].image, VK_NULL_HANDLE);
	}

	for(i=0;i<NUM_MODELS;i++)
	{
		for(j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkFreeMemory(device, Model[i].Mesh[j].BufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(device, Model[i].Mesh[j].Buffer, VK_NULL_HANDLE);

			vkFreeMemory(device, Model[i].Mesh[j].IndexBufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(device, Model[i].Mesh[j].IndexBuffer, VK_NULL_HANDLE);
		}
	}

	vkFreeMemory(device, uniformBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, uniformBuffer, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(device, descriptorPool, VK_NULL_HANDLE);

	vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);

	vkDestroyImageView(device, depthImageView, VK_NULL_HANDLE);
	vkFreeMemory(device, depthMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, depthImage, VK_NULL_HANDLE);

	for(i=0;i<imageCount+1;i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);

		vkDestroyImageView(device, swapchainImageView[i], VK_NULL_HANDLE);

		vkDestroyFence(device, frameFences[i], VK_NULL_HANDLE);

		vkDestroySemaphore(device, presentCompleteSemaphores[i], VK_NULL_HANDLE);
		vkDestroySemaphore(device, renderCompleteSemaphores[i], VK_NULL_HANDLE);
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

	Init();

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
			Render();
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
			Width=max(LOWORD(lParam), 2);
			Height=max(HIWORD(lParam), 2);

			if(device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
			{
				uint32_t i;

				// Wait for the device to complete any pending work
				vkDeviceWaitIdle(device);

				// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
				// This is basically just the swapchain and frame buffers

				for(i=0;i<imageCount;i++)
				{
					// Frame buffers
					vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);

					// Swapchain image views
					vkDestroyImageView(device, swapchainImageView[i], VK_NULL_HANDLE);
				}

				// Depth buffer objects
				vkDestroyImageView(device, depthImageView, VK_NULL_HANDLE);
				vkFreeMemory(device, depthMemory, VK_NULL_HANDLE);
				vkDestroyImage(device, depthImage, VK_NULL_HANDLE);

				// And finally the swapchain
				vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);

				// Recreate the swapchain and frame buffers
				createSwapchain(Width, Height, VK_FALSE);
				createFramebuffers();

				// Does the render pass need to be recreated as well?
				// Validation doesn't complain about it...?
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
