#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "camera/camera.h"
#include "model/bmodel.h"
#include "image/image.h"
#include "utils/list.h"
#include "lights/lights.h"
#include "utils/event.h"
#include "utils/input.h"
#include "particle/particle.h"
#include "threads/threads.h"
#include "vr/vr.h"
#include "font/font.h"
#include "models.h"
#include "textures.h"
#include "skybox.h"
#include "shadow.h"
#include "perframe.h"

uint32_t Width=1440, Height=720;

extern bool IsVR;

VkInstance Instance;
VkuContext_t Context;

VkuMemZone_t *VkZone;

#ifdef _DEBUG
VkDebugUtilsMessengerEXT debugMessenger;
#endif

Camera_t Camera;

extern float fps, fTimeStep, fTime;

ParticleSystem_t ParticleSystem;

BModel_t Model[NUM_MODELS];
VkuImage_t Textures[NUM_TEXTURES];

// Swapchain
VkuSwapchain_t Swapchain;

// Multisample anti-alias sample count
VkSampleCountFlags MSAA=VK_SAMPLE_COUNT_2_BIT;

// Colorbuffer image
VkFormat ColorFormat=VK_FORMAT_B8G8R8A8_SRGB;
VkuImage_t ColorImage[2];		// left and right eye color buffer
VkuImage_t ColorResolve[2];		// left and right eye MSAA resolve color buffer

// Depth buffer image
VkFormat DepthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;
VkuImage_t DepthImage[2];		// left and right eye depth buffers

// Primary framebuffers
VkFramebuffer FrameBuffers[2];	// left and right eye frame buffers

// Primary RenderPass
VkRenderPass RenderPass;

// Primary rendering pipeline stuff
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;
//////

// Asteroid data
VkuBuffer_t Asteroid_Instance;

typedef struct
{
	vec3 Position;
	float Radius;
	vec3 Rotate;
} Asteroid_t;

#define NUM_ASTEROIDS 300
Asteroid_t Asteroids[NUM_ASTEROIDS];
//////

// Some debugging object pipelines
VkPipelineLayout LinePipelineLayout;
VkuPipeline_t LinePipeline;

VkPipelineLayout SpherePipelineLayout;
VkuPipeline_t SpherePipeline;
//////

typedef struct
{
	uint32_t Index, Eye;
	VkCommandPool CommandPool[VKU_MAX_FRAME_COUNT];
	VkDescriptorPool DescriptorPool[VKU_MAX_FRAME_COUNT];
	VkCommandBuffer SecCommandBuffer[VKU_MAX_FRAME_COUNT];
	volatile bool Done;
} ThreadData_t;

ThreadWorker_t Thread[4];
ThreadData_t ThreadData[4];

void RecreateSwapchain(void);

bool CreateFramebuffers(uint32_t Eye, uint32_t targetWidth, uint32_t targetHeight)
{
	vkuCreateImageBuffer(&Context, &ColorImage[Eye],
						 VK_IMAGE_TYPE_2D, ColorFormat, 1, 1, targetWidth, targetHeight, 1,
						 MSAA, VK_IMAGE_TILING_OPTIMAL,
						 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						 0);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=ColorImage[Eye].Image,
		.format=ColorFormat,
		.components.r=VK_COMPONENT_SWIZZLE_R,
		.components.g=VK_COMPONENT_SWIZZLE_G,
		.components.b=VK_COMPONENT_SWIZZLE_B,
		.components.a=VK_COMPONENT_SWIZZLE_A,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &ColorImage[Eye].View);

	vkuCreateImageBuffer(&Context, &ColorResolve[Eye],
						 VK_IMAGE_TYPE_2D, ColorFormat, 1, 1, rtWidth, rtHeight, 1,
						 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
						 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	vkuCreateImageBuffer(&Context, &DepthImage[Eye],
						 VK_IMAGE_TYPE_2D, DepthFormat, 1, 1, targetWidth, targetHeight, 1,
						 MSAA, VK_IMAGE_TILING_OPTIMAL,
						 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						 0);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=DepthImage[Eye].Image,
		.format=DepthFormat,
		.components.r=VK_COMPONENT_SWIZZLE_R,
		.components.g=VK_COMPONENT_SWIZZLE_G,
		.components.b=VK_COMPONENT_SWIZZLE_B,
		.components.a=VK_COMPONENT_SWIZZLE_A,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &DepthImage[Eye].View);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=RenderPass,
		.attachmentCount=2,
		.pAttachments=(VkImageView[]) { ColorImage[Eye].View, DepthImage[Eye].View },
		.width=targetWidth,
		.height=targetHeight,
		.layers=1,
	}, 0, &FrameBuffers[Eye]);

	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(CommandBuffer, ColorImage[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkuTransitionLayout(CommandBuffer, ColorResolve[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkuTransitionLayout(CommandBuffer, DepthImage[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, CommandBuffer);

	return true;
}

bool CreatePipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=ColorFormat,
				.samples=MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=DepthFormat,
				.samples=MSAA,
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
		}
	}, 0, &RenderPass);

	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		vkuCreateHostBuffer(&Context, &PerFrame[i].uboBuffer[0], sizeof(UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].uboBuffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Main_UBO[0]);

		vkuCreateHostBuffer(&Context, &PerFrame[i].uboBuffer[1], sizeof(UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].uboBuffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Main_UBO[1]);

		for(uint32_t j=0;j<NUM_MODELS;j++)
		{
			vkuInitDescriptorSet(&PerFrame[i].DescriptorSet[j], &Context);

			vkuDescriptorSet_AddBinding(&PerFrame[i].DescriptorSet[j], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			vkuDescriptorSet_AddBinding(&PerFrame[i].DescriptorSet[j], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			vkuDescriptorSet_AddBinding(&PerFrame[i].DescriptorSet[j], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
			vkuDescriptorSet_AddBinding(&PerFrame[i].DescriptorSet[j], 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

			vkuAssembleDescriptorSetLayout(&PerFrame[i].DescriptorSet[j]);
		}
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&PerFrame[0].DescriptorSet[0].DescriptorSetLayout, // Just use the first in the set, they're all the same layout
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &PipelineLayout);

	vkuInitPipeline(&Pipeline, &Context);

	vkuPipeline_SetPipelineLayout(&Pipeline, PipelineLayout);
	vkuPipeline_SetRenderPass(&Pipeline, RenderPass);

	Pipeline.DepthTest=VK_TRUE;
	Pipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	Pipeline.RasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&Pipeline, "./shaders/lighting.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&Pipeline, "./shaders/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&Pipeline, 0, sizeof(vec4)*5, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&Pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&Pipeline, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&Pipeline, 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);
	vkuPipeline_AddVertexAttribute(&Pipeline, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*4);

	vkuPipeline_AddVertexBinding(&Pipeline, 1, sizeof(matrix), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&Pipeline, 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&Pipeline, 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&Pipeline, 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);

	if(!vkuAssemblePipeline(&Pipeline))
		return false;

	return true;
}

bool CreateLinePipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+(sizeof(vec4)*2),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &LinePipelineLayout);

	vkuInitPipeline(&LinePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&LinePipeline, LinePipelineLayout);
	vkuPipeline_SetRenderPass(&LinePipeline, RenderPass);

	LinePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	LinePipeline.DepthTest=VK_TRUE;
	LinePipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&LinePipeline, "./shaders/line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&LinePipeline, "./shaders/line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&LinePipeline))
		return false;

	return true;
}

bool CreateSpherePipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+sizeof(vec4),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &SpherePipelineLayout);

	vkuInitPipeline(&SpherePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&SpherePipeline, SpherePipelineLayout);
	vkuPipeline_SetRenderPass(&SpherePipeline, RenderPass);

	SpherePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	SpherePipeline.DepthTest=VK_TRUE;
	SpherePipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	SpherePipeline.PolygonMode=VK_POLYGON_MODE_LINE;

	if(!vkuPipeline_AddStage(&SpherePipeline, "./shaders/sphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&SpherePipeline, "./shaders/sphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&SpherePipeline))
		return false;

	return true;
}

float RandFloat(void)
{
	return (float)rand()/RAND_MAX;
}

bool SphereSphereIntersect(vec3 PositionA, float RadiusA, vec3 PositionB, float RadiusB)
{
	const float distance=sqrtf(
		(PositionA[0]-PositionB[0])*(PositionA[0]-PositionB[0])+
		(PositionA[1]-PositionB[1])*(PositionA[1]-PositionB[1])+
		(PositionA[2]-PositionB[2])*(PositionA[2]-PositionB[2])
	);

	return distance<RadiusA+RadiusB;
}

void GenerateSkyParams(void)
{
	// Build a skybox param struct with random values
	Skybox_UBO_t Skybox_UBO;

	Vec4_Set(Skybox_UBO.uOffset, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(Skybox_UBO.uOffset);

	Vec3_Set(Skybox_UBO.uNebulaAColor, RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaADensity=RandFloat()*2.0f;

	Vec3_Set(Skybox_UBO.uNebulaBColor, RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaBDensity=RandFloat()*2.0f;

	Vec4_Set(Skybox_UBO.uSunPosition, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(Skybox_UBO.uSunPosition);

	Vec4_Set(Skybox_UBO.uSunColor, min(1.0f, RandFloat()+0.5f), min(1.0f, RandFloat()+0.5f), min(1.0f, RandFloat()+0.5f), 0.0f);
	Skybox_UBO.uSunSize=1.0f/(RandFloat()*1000.0f+100.0f);
	Skybox_UBO.uSunFalloff=RandFloat()*16.0f+8.0f;

	Skybox_UBO.uStarsScale=200.0f;
	Skybox_UBO.uStarDensity=8.0f;

	// Copy it out to the other eyes and frames
	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		memcpy(PerFrame[i].Skybox_UBO[0], &Skybox_UBO, sizeof(Skybox_UBO_t));
		memcpy(PerFrame[i].Skybox_UBO[1], &Skybox_UBO, sizeof(Skybox_UBO_t));
	}

	float *Data=NULL;

	if(!Asteroid_Instance.Buffer)
		vkuCreateHostBuffer(&Context, &Asteroid_Instance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, Asteroid_Instance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&Data);

	uint32_t i=0, tries=0;

	memset(Asteroids, 0, sizeof(Asteroid_t)*NUM_ASTEROIDS);

	while(i<NUM_ASTEROIDS)
	{
		vec3 RandomVec;
		Vec3_Set(RandomVec, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(RandomVec);

		Asteroid_t Asteroid;
		Vec3_Set(Asteroid.Position, RandomVec[0]*(RandFloat()*10000.0f+500.0f), RandomVec[1]*(RandFloat()*10000.0f+500.0f), RandomVec[2]*(RandFloat()*10000.0f+500.0f));
		Vec3_Set(Asteroid.Rotate, RandFloat()*PI*2.0f, RandFloat()*PI*2.0f, RandFloat()*PI*2.0f);
		Asteroid.Radius=(RandFloat()*1000.0f+10.0f)*2.0f;

		bool overlapping=false;

		for(uint32_t j=0;j<i;j++)
		{
			if(SphereSphereIntersect(Asteroid.Position, Asteroid.Radius, Asteroids[j].Position, Asteroids[j].Radius))
				overlapping=true;
		}

		if(!overlapping)
			Asteroids[i++]=Asteroid;

		tries++;

		if(tries>NUM_ASTEROIDS*NUM_ASTEROIDS)
			break;
	}

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		MatrixIdentity(&Data[16*i]);
		MatrixTranslatev(Asteroids[i].Position, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[0], 1.0f, 0.0f, 0.0f, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[1], 0.0f, 1.0f, 0.0f, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[2], 0.0f, 0.0f, 1.0f, &Data[16*i]);
		MatrixScale(Asteroids[i].Radius/2.0f, Asteroids[i].Radius/2.0f, Asteroids[i].Radius/2.0f, &Data[16*i]);
	}

	vkUnmapMemory(Context.Device, Asteroid_Instance.DeviceMemory);
}

void Thread_Constructor(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	for(uint32_t i=0;i<2;i++)
	{
		vkCreateCommandPool(Context.Device, &(VkCommandPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags=0,
			.queueFamilyIndex=Context.QueueFamilyIndex,
		}, VK_NULL_HANDLE, &Data->CommandPool[i]);

		vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=Data->CommandPool[i],
			.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			.commandBufferCount=1,
		}, &Data->SecCommandBuffer[i]);

		// Create a large descriptor pool, so I don't have to worry about readjusting for exactly what I have
		vkCreateDescriptorPool(Context.Device, &(VkDescriptorPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
			.poolSizeCount=4,
			.pPoolSizes=(VkDescriptorPoolSize[])
			{
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount=1024, // Max number of this descriptor type that can be in each descriptor set?
				},
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount=1024,
				},
			},
		}, VK_NULL_HANDLE, &Data->DescriptorPool[i]);
	}

	Data->Done=false;
}

void Thread_Destructor(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	for(uint32_t i=0;i<2;i++)
	{
		vkDestroyCommandPool(Context.Device, Data->CommandPool[i], VK_NULL_HANDLE);
		vkDestroyDescriptorPool(Context.Device, Data->DescriptorPool[i], VK_NULL_HANDLE);
	}
}

void Thread_Main(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetDescriptorPool(Context.Device, Data->DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=RenderPass,
			.framebuffer=FrameBuffers[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(Data->SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	// Draw the models
	vkCmdBindVertexBuffers(Data->SecCommandBuffer[Data->Eye], 1, 1, &Asteroid_Instance.Buffer, &(VkDeviceSize) { 0 });
	
	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Data->Index].DescriptorSet[i], 0, &Textures[2*i+0]);
		vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Data->Index].DescriptorSet[i], 1, &Textures[2*i+1]);
		vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Data->Index].DescriptorSet[i], 2, &ShadowDepth);
		vkuDescriptorSet_UpdateBindingBufferInfo(&PerFrame[Data->Index].DescriptorSet[i], 3, PerFrame[Data->Index].uboBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&PerFrame[Data->Index].DescriptorSet[i], Data->DescriptorPool[Data->Eye]);

		vkCmdBindDescriptorSets(Data->SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &PerFrame[Data->Index].DescriptorSet[i].DescriptorSet, 0, VK_NULL_HANDLE);

		matrix local;
		MatrixIdentity(local);
		MatrixRotate(fTime, 1.0f, 0.0f, 0.0f, local);
		MatrixRotate(fTime, 0.0f, 1.0f, 0.0f, local);

		vkCmdPushConstants(Data->SecCommandBuffer[Data->Eye], PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &local);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(Data->SecCommandBuffer[Data->Eye], 0, 1, &Model[i].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindIndexBuffer(Data->SecCommandBuffer[Data->Eye], Model[i].Mesh[j].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(Data->SecCommandBuffer[Data->Eye], Model[i].Mesh[j].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}

	vkEndCommandBuffer(Data->SecCommandBuffer[Data->Eye]);

	Data->Done=true;
}

void Thread_Skybox(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetDescriptorPool(Context.Device, Data->DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=RenderPass,
			.framebuffer=FrameBuffers[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	vkCmdBindPipeline(Data->SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&PerFrame[Data->Index].SkyboxDescriptorSet, 0, PerFrame[Data->Index].Skybox_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&PerFrame[Data->Index].SkyboxDescriptorSet, Data->DescriptorPool[Data->Eye]);

	vkCmdBindDescriptorSets(Data->SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipelineLayout, 0, 1, &PerFrame[Data->Index].SkyboxDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(Data->SecCommandBuffer[Data->Eye], 60, 1, 0, 0);

	vkEndCommandBuffer(Data->SecCommandBuffer[Data->Eye]);

	Data->Done=true;
}

void Thread_Font(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetCommandPool(Context.Device, Data->CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=RenderPass,
			.framebuffer=FrameBuffers[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	Font_Print(Data->SecCommandBuffer[Data->Eye], 0.0f, 0.0f, "FPS: %0.1f", fps);

	vkEndCommandBuffer(Data->SecCommandBuffer[Data->Eye]);

	Data->Done=true;
}


void Thread_Particles(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetDescriptorPool(Context.Device, Data->DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=RenderPass,
			.framebuffer=FrameBuffers[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	matrix Modelview;
	MatrixMult(PerFrame[0].Main_UBO[Data->Eye]->modelview, PerFrame[0].Main_UBO[Data->Eye]->HMD, Modelview);
	ParticleSystem_Draw(&ParticleSystem, Data->SecCommandBuffer[Data->Eye], Data->DescriptorPool[Data->Eye], Modelview, PerFrame[0].Main_UBO[Data->Eye]->projection);

	vkEndCommandBuffer(Data->SecCommandBuffer[Data->Eye]);

	Data->Done=true;
}

matrix HMDMatrix;

void EyeRender(VkCommandBuffer CommandBuffer, uint32_t Index, uint32_t Eye, matrix Pose)
{
	// Generate the projection matrix
	memcpy(PerFrame[Index].Main_UBO[Eye]->projection, &EyeProjection[Eye], sizeof(matrix));

	// Set up the modelview matrix
	MatrixIdentity(PerFrame[Index].Main_UBO[Eye]->modelview);
	CameraUpdate(&Camera, fTimeStep, PerFrame[Index].Main_UBO[Eye]->modelview);

	memcpy(PerFrame[Index].Main_UBO[Eye]->HMD, Pose, sizeof(matrix));

	memcpy(PerFrame[Index].Skybox_UBO[Eye]->HMD, PerFrame[Index].Main_UBO[Eye]->HMD, sizeof(matrix));

	memcpy(PerFrame[Index].Skybox_UBO[Eye]->modelview, PerFrame[Index].Main_UBO[Eye]->modelview, sizeof(matrix));
	memcpy(PerFrame[Index].Skybox_UBO[Eye]->projection, PerFrame[Index].Main_UBO[Eye]->projection, sizeof(matrix));

	Vec3_Setv(PerFrame[Index].Main_UBO[Eye]->light_color, PerFrame[Index].Skybox_UBO[Eye]->uSunColor);
	Vec3_Setv(PerFrame[Index].Main_UBO[Eye]->light_direction, PerFrame[Index].Skybox_UBO[Eye]->uSunPosition);
	PerFrame[Index].Main_UBO[Eye]->light_direction[3]=PerFrame[Index].Skybox_UBO[Eye]->uSunSize;

	memcpy(PerFrame[Index].Main_UBO[Eye]->light_mvp, Shadow_UBO.mvp, sizeof(matrix));

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=RenderPass,
		.framebuffer=FrameBuffers[Eye],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]) { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0 } },
		.renderArea=(VkRect2D){ { 0, 0 }, { rtWidth, rtHeight } },
	}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Set per thread data and add the job to that worker thread
	ThreadData[0].Index=Index;
	ThreadData[0].Eye=Eye;
	Thread_AddJob(&Thread[0], Thread_Main, (void *)&ThreadData[0]);

	ThreadData[1].Index=Index;
	ThreadData[1].Eye=Eye;
	Thread_AddJob(&Thread[1], Thread_Skybox, (void *)&ThreadData[1]);

	ThreadData[2].Index=Index;
	ThreadData[2].Eye=Eye;
	Thread_AddJob(&Thread[2], Thread_Particles, (void *)&ThreadData[2]);

	ThreadData[3].Index=Index;
	ThreadData[3].Eye=Eye;
	Thread_AddJob(&Thread[3], Thread_Font, (void *)&ThreadData[3]);

	// Wait for the threads to finish
	while(!(
		ThreadData[0].Done&&
		ThreadData[1].Done&&
		ThreadData[2].Done&&
		ThreadData[3].Done
	));

	// Reset done flag
	ThreadData[0].Done=false;
	ThreadData[1].Done=false;
	ThreadData[2].Done=false;
	ThreadData[3].Done=false;

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(CommandBuffer, 4, (VkCommandBuffer[]) {
		ThreadData[0].SecCommandBuffer[Eye],
		ThreadData[1].SecCommandBuffer[Eye],
		ThreadData[2].SecCommandBuffer[Eye],
		ThreadData[3].SecCommandBuffer[Eye]
	});

	vkCmdEndRenderPass(CommandBuffer);
}

void Render(void)
{
	static uint32_t OldIndex=0;
	uint32_t Index=OldIndex;
	matrix Pose;

	// Get a pointer to the emitter that's providing the positions
	ParticleEmitter_t *Emitter=List_GetPointer(&ParticleSystem.Emitters, 0);

	for(uint32_t i=0;i<Emitter->NumParticles;i++)
	{
		if(Emitter->Particles[i].ID!=Emitter->ID)
		{
			// Get those positions and set the other emitter's positions to those
			ParticleSystem_SetEmitterPosition(&ParticleSystem, Emitter->Particles[i].ID, Emitter->Particles[i].pos);

			// If this particle is dead, delete that emitter's ID
			if(Emitter->Particles[i].life<0.0f)
				ParticleSystem_DeleteEmitter(&ParticleSystem, Emitter->Particles[i].ID);
		}
	}

	ParticleSystem_Step(&ParticleSystem, fTimeStep);

	MatrixIdentity(Pose);

	if(IsVR)
		GetHeadPose(Pose);

	VkResult Result=vkAcquireNextImageKHR(Context.Device, Swapchain.Swapchain, UINT64_MAX, PerFrame[Index].PresentCompleteSemaphore, VK_NULL_HANDLE, &OldIndex);

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
	{
		DBGPRINTF("Swapchain out of date... Rebuilding.\n");
		RecreateSwapchain();
		return;
	}

	// Reset the frame fence and command pool (and thus the command buffer)
	vkResetFences(Context.Device, 1, &PerFrame[Index].FrameFence);
	vkResetCommandPool(Context.Device, PerFrame[Index].CommandPool, 0);

	// Start recording the commands
	vkBeginCommandBuffer(PerFrame[Index].CommandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	// Update shadow depth map
	ShadowUpdateMap(PerFrame[Index].CommandBuffer, Index);

	if(IsVR)
	{
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		EyeRender(PerFrame[Index].CommandBuffer, Index, 0, Pose);
		EyeRender(PerFrame[Index].CommandBuffer, Index, 1, Pose);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vkCmdResolveImage(PerFrame[Index].CommandBuffer, ColorImage[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ColorResolve[0].Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageResolve)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1, .srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1, .dstOffset={ 0, 0, 0 },
			.extent={ rtWidth, rtHeight, 1 },
		});

		vkCmdResolveImage(PerFrame[Index].CommandBuffer, ColorImage[1].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ColorResolve[1].Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageResolve)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1, .srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1, .dstOffset={ 0, 0, 0 },
			.extent={ rtWidth, rtHeight, 1 },
		});

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkCmdBlitImage(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Swapchain.Image[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageBlit)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1,
			.srcOffsets[0]={ 0, 0, 0 }, .srcOffsets[1]={ rtWidth, rtHeight, 1 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1,
			.dstOffsets[0]={ 0, 0, 0 }, .dstOffsets[1]={ Width/2, Height, 1 },
		}, VK_FILTER_LINEAR);

		vkCmdBlitImage(PerFrame[Index].CommandBuffer, ColorResolve[1].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Swapchain.Image[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageBlit)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1,
			.srcOffsets[0]={ 0, 0, 0 }, .srcOffsets[1]={ rtWidth, rtHeight, 1 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1,
			.dstOffsets[0]={ Width/2, 0, 0 }, .dstOffsets[1]={ Width, Height, 1 },
		}, VK_FILTER_LINEAR);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}
	else
	{
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		EyeRender(PerFrame[Index].CommandBuffer, Index, 0, Pose);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorImage[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vkCmdResolveImage(PerFrame[Index].CommandBuffer, ColorImage[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, ColorResolve[0].Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageResolve)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1, .srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1, .dstOffset={ 0, 0, 0 },
			.extent={ rtWidth, rtHeight, 1 },
		});

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkCmdBlitImage(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Swapchain.Image[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageBlit)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .srcSubresource.mipLevel=0, .srcSubresource.baseArrayLayer=0, .srcSubresource.layerCount=1,
			.srcOffsets[0]={ 0, 0, 0 }, .srcOffsets[1]={ rtWidth, rtHeight, 1 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .dstSubresource.mipLevel=0, .dstSubresource.baseArrayLayer=0, .dstSubresource.layerCount=1,
			.dstOffsets[0]={ 0, 0, 0 }, .dstOffsets[1]={ Width, Height, 1 },
		}, VK_FILTER_LINEAR);

		vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	vkEndCommandBuffer(PerFrame[Index].CommandBuffer);

	// Sumit command queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&PerFrame[Index].PresentCompleteSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&PerFrame[Index].RenderCompleteSemaphore,
		.commandBufferCount=1,
		.pCommandBuffers=(VkCommandBuffer[]){ PerFrame[Index].CommandBuffer },
	}, PerFrame[Index].FrameFence);

	vkWaitForFences(Context.Device, 1, &PerFrame[Index].FrameFence, VK_TRUE, UINT64_MAX);

	// And present it to the screen
	Result=vkQueuePresentKHR(Context.Queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&PerFrame[Index].RenderCompleteSemaphore,
		.swapchainCount=1,
		.pSwapchains=&Swapchain.Swapchain,
		.pImageIndices=&OldIndex,
	});
	
	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
	{
		DBGPRINTF("Swapchain out of date... Rebuilding.\n");
		RecreateSwapchain();
		return;
	}

	if(IsVR)
	{
		VRTextureBounds_t bounds={ 0.0f, 0.0f, 1.0f, 1.0f };

		VRVulkanTextureData_t vulkanData=
		{
			.m_pDevice=(struct VkDevice_T *)Context.Device,
			.m_pPhysicalDevice=(struct VkPhysicalDevice_T *)Context.PhysicalDevice,
			.m_pInstance=(struct VkInstance_T *)Instance,
			.m_pQueue=(struct VkQueue_T *)Context.Queue,
			.m_nQueueFamilyIndex=Context.QueueFamilyIndex,

			.m_nWidth=rtWidth,
			.m_nHeight=rtHeight,
			.m_nFormat=ColorFormat,
			.m_nSampleCount=MSAA,
		};

		struct Texture_t texture={ &vulkanData, ETextureType_TextureType_Vulkan, EColorSpace_ColorSpace_Auto };

		vulkanData.m_nImage=(uint64_t)ColorImage[EVREye_Eye_Left].Image,
			VRCompositor->Submit(EVREye_Eye_Left, &texture, &bounds, EVRSubmitFlags_Submit_Default);

		vulkanData.m_nImage=(uint64_t)ColorImage[EVREye_Eye_Right].Image;
		VRCompositor->Submit(EVREye_Eye_Right, &texture, &bounds, EVRSubmitFlags_Submit_Default);
	}
}

#ifdef _DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		DBGPRINTF(DEBUG_ERROR, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		DBGPRINTF(DEBUG_INFO, "\n%s\n", pCallbackData->pMessage);
	else
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);

	return VK_FALSE;
}
#endif

void EmitterCallback(uint32_t Index, uint32_t NumParticles, Particle_t *Particle)
{
	Vec3_Sets(Particle->pos, 0.0f);

	// Simple -1.0 to 1.0 random spherical pattern, scaled by 100, fairly short lifespan.
	Vec3_Set(Particle->vel, ((float)rand()/RAND_MAX)*2.0f-1.0f, ((float)rand()/RAND_MAX)*2.0f-1.0f, ((float)rand()/RAND_MAX)*2.0f-1.0f);
	Vec3_Normalize(Particle->vel);
	Vec3_Muls(Particle->vel, 100.0f);

	Particle->life=((float)rand()/RAND_MAX)*2.5f+0.01f;
}

bool Init(void)
{
	Event_Add(EVENT_KEYDOWN, Event_KeyDown);
	Event_Add(EVENT_KEYUP, Event_KeyUp);
	Event_Add(EVENT_MOUSE, Event_Mouse);

#ifdef _DEBUG
	if(vkCreateDebugUtilsMessengerEXT(Instance, &(VkDebugUtilsMessengerCreateInfoEXT)
	{
		.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback=debugCallback
	}, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
		return false;
#endif

	VkZone=vkuMem_Init(&Context, (size_t)(Context.DeviceProperties2.maxMemoryAllocationSize*0.8f));

	if(VkZone==NULL)
		return false;

	CameraInit(&Camera, (float[]) { 0.0f, 0.0f, 200.0f }, (float[]) { -1.0f, 0.0f, 0.0f }, (float[3]) { 0.0f, 1.0f, 0.0f });

	// Load models
	if(LoadBModel(&Model[MODEL_ASTEROID1], "./assets/asteroid1.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID1]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID2], "./assets/asteroid2.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID2]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID3], "./assets/asteroid3.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID3]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID4], "./assets/asteroid4.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID4]);
	else
		return false;

	uint32_t TriangleCount=0;

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		for(uint32_t j=0;j<Model[i].NumMesh;j++)
			TriangleCount+=Model[i].Mesh[j].NumFace;
	}

	TriangleCount*=NUM_ASTEROIDS;

	DBGPRINTF(DEBUG_ERROR, "\nNot an error, just a total triangle count: %d\n\n", TriangleCount);

	// Load textures
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1], "./assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1_NORMAL], "./assets/asteroid1_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2], "./assets/asteroid2.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2_NORMAL], "./assets/asteroid2_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3], "./assets/asteroid3.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3_NORMAL], "./assets/asteroid3_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4], "./assets/asteroid4.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4_NORMAL], "./assets/asteroid4_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);

	// Create primary pipeline and renderpass
	CreatePipeline();

	// Create skybox pipeline (uses renderpass from main pipeline)
	CreateSkyboxPipeline();
	GenerateSkyParams();

	// Create debug line pipeline
	//CreateLinePipeline();

	// Create debug sphere pipeline
	//CreateSpherePipeline();

	InitShadowPipeline();
	InitShadowMap();

	if(!ParticleSystem_Init(&ParticleSystem))
		return false;

	vec3 Zero={ 0.0f, 0.0f, 0.0f };
	ParticleSystem_AddEmitter(&ParticleSystem, Zero, Zero, Zero, 0.0f, 100, true, NULL);

	// Create primary frame buffers, depth image
	// This needs to be done after the render pass is created
	CreateFramebuffers(0, rtWidth, rtHeight);

	if(IsVR)
		CreateFramebuffers(1, rtWidth, rtHeight);

	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		// Create needed fence and semaphores for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(Context.Device, &(VkFenceCreateInfo) {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &PerFrame[i].FrameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = VK_NULL_HANDLE }, VK_NULL_HANDLE, &PerFrame[i].PresentCompleteSemaphore);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = VK_NULL_HANDLE }, VK_NULL_HANDLE, &PerFrame[i].RenderCompleteSemaphore);

		// Create per-frame command pools
		vkCreateCommandPool(Context.Device, &(VkCommandPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags=0,
			.queueFamilyIndex=Context.QueueFamilyIndex,
		}, VK_NULL_HANDLE, &PerFrame[i].CommandPool);

		// Allocate the command buffers we will be rendering into
		vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=PerFrame[i].CommandPool,
			.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount=1,
		}, &PerFrame[i].CommandBuffer);
	}

	for(uint32_t i=0;i<4;i++)
	{
		Thread_Init(&Thread[i]);
		Thread_AddConstructor(&Thread[i], Thread_Constructor, (void *)&ThreadData[i]);
		Thread_AddDestructor(&Thread[i], Thread_Destructor, (void *)&ThreadData[i]);
		Thread_Start(&Thread[i]);
	}

	if(!IsVR)
	{
		MatrixIdentity(EyeProjection[0]);
		MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f, true, EyeProjection[0]);
	}

	return true;
}

void RecreateSwapchain(void)
{
	if(Context.Device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
	{
		rtWidth=Width;
		rtHeight=Height;

		// Wait for the device to complete any pending work
		vkDeviceWaitIdle(Context.Device);

		// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
		// This is basically just the swapchain, framebuffers, and depthbuffer.

		// Destroy the color buffer
		vkDestroyImageView(Context.Device, ColorImage[0].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, ColorImage[0].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, ColorImage[0].DeviceMemory);

		// Destroy the color resolve buffer
		vkDestroyImageView(Context.Device, ColorResolve[0].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, ColorResolve[0].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, ColorResolve[0].DeviceMemory);

		// Destroy the depth buffer
		vkDestroyImageView(Context.Device, DepthImage[0].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, DepthImage[0].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, DepthImage[0].DeviceMemory);

		// Destroy the framebuffers
		vkDestroyFramebuffer(Context.Device, FrameBuffers[0], VK_NULL_HANDLE);

		// Destroy the other eye as well, only if VR is enabled
		if(IsVR)
		{
			vkDestroyImageView(Context.Device, ColorImage[1].View, VK_NULL_HANDLE);
			vkDestroyImage(Context.Device, ColorImage[1].Image, VK_NULL_HANDLE);
			vkuMem_Free(VkZone, ColorImage[1].DeviceMemory);

			vkDestroyImageView(Context.Device, ColorResolve[1].View, VK_NULL_HANDLE);
			vkDestroyImage(Context.Device, ColorResolve[1].Image, VK_NULL_HANDLE);
			vkuMem_Free(VkZone, ColorResolve[1].DeviceMemory);

			vkDestroyImageView(Context.Device, DepthImage[1].View, VK_NULL_HANDLE);
			vkDestroyImage(Context.Device, DepthImage[1].Image, VK_NULL_HANDLE);
			vkuMem_Free(VkZone, DepthImage[1].DeviceMemory);

			vkDestroyFramebuffer(Context.Device, FrameBuffers[1], VK_NULL_HANDLE);
		}

		// Destroy the swapchain
		vkuDestroySwapchain(&Context, &Swapchain);

		// Recreate the swapchain
		vkuCreateSwapchain(&Context, &Swapchain, Width, Height, VK_TRUE);

		// Recreate the framebuffer
		CreateFramebuffers(0, rtWidth, rtHeight);

		if(IsVR)
			CreateFramebuffers(1, rtWidth, rtHeight);
		else
		{
			MatrixIdentity(EyeProjection[0]);
			MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f, true, EyeProjection[0]);
		}
	}
}

void Destroy(void)
{
	vkDeviceWaitIdle(Context.Device);

	DestroyOpenVR();

	for(uint32_t i=0;i<4;i++)
		Thread_Destroy(&Thread[i]);

	if(Context.PipelineCache)
	{
		DBGPRINTF(DEBUG_INFO, "\nWriting pipeline cache to disk...\n");

		size_t PipelineCacheSize=0;
		vkGetPipelineCacheData(Context.Device, Context.PipelineCache, &PipelineCacheSize, VK_NULL_HANDLE);

		uint8_t *PipelineCacheData=(uint8_t *)Zone_Malloc(Zone, PipelineCacheSize);

		if(PipelineCacheData)
		{
			FILE *Stream=fopen("pipelinecache.bin", "wb");

			if(Stream)
			{
				vkGetPipelineCacheData(Context.Device, Context.PipelineCache, &PipelineCacheSize, PipelineCacheData);
				fwrite(PipelineCacheData, 1, PipelineCacheSize, Stream);
				fclose(Stream);
				Zone_Free(Zone, PipelineCacheData);
			}
			else
				DBGPRINTF(DEBUG_ERROR, "Failed to open file handle to write pipeline cache data.\n");
		}
		else
			DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for pipeline cache data.\n");
	}

	// Particle system destruction
	ParticleSystem_Destroy(&ParticleSystem);
	//////////

	// Skybox destruction
	DestroySkybox();
	//////////

	// Font destruction
	Font_Destroy();
	//////////

	// Asteroid instance buffer destruction
	vkuDestroyBuffer(&Context, &Asteroid_Instance);
	//////////

	// Textures destruction
	for(uint32_t i=0;i<NUM_TEXTURES;i++)
		vkuDestroyImageBuffer(&Context, &Textures[i]);
	//////////

	// 3D Model destruction
	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDestroyBuffer(&Context, &Model[i].VertexBuffer);

		for(uint32_t j=0;j<Model[i].NumMesh;j++)
			vkuDestroyBuffer(&Context, &Model[i].Mesh[j].IndexBuffer);

		FreeBModel(&Model[i]);
	}
	//////////

	// Shadow map destruction
	DestroyShadow();
	//////////

	// Main render destruction
	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		vkUnmapMemory(Context.Device, PerFrame[i].uboBuffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].uboBuffer[0]);

		vkUnmapMemory(Context.Device, PerFrame[i].uboBuffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].uboBuffer[1]);

		for(uint32_t j=0;j<NUM_MODELS;j++)
			vkDestroyDescriptorSetLayout(Context.Device, PerFrame[i].DescriptorSet[j].DescriptorSetLayout, VK_NULL_HANDLE);
	}

	vkDestroyRenderPass(Context.Device, RenderPass, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, PipelineLayout, VK_NULL_HANDLE);
	//////////

	// Swapchain, framebuffer, and depthbuffer destruction
	vkDestroyImageView(Context.Device, ColorImage[0].View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ColorImage[0].Image, VK_NULL_HANDLE);
	vkuMem_Free(VkZone, ColorImage[0].DeviceMemory);

	vkDestroyImageView(Context.Device, ColorResolve[0].View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ColorResolve[0].Image, VK_NULL_HANDLE);
	vkuMem_Free(VkZone, ColorResolve[0].DeviceMemory);

	vkDestroyImageView(Context.Device, DepthImage[0].View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, DepthImage[0].Image, VK_NULL_HANDLE);
	vkuMem_Free(VkZone, DepthImage[0].DeviceMemory);

	vkDestroyFramebuffer(Context.Device, FrameBuffers[0], VK_NULL_HANDLE);

	if(IsVR)
	{
		vkDestroyImageView(Context.Device, ColorImage[1].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, ColorImage[1].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, ColorImage[1].DeviceMemory);

		vkDestroyImageView(Context.Device, ColorResolve[1].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, ColorResolve[1].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, ColorResolve[1].DeviceMemory);

		vkDestroyImageView(Context.Device, DepthImage[1].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, DepthImage[1].Image, VK_NULL_HANDLE);
		vkuMem_Free(VkZone, DepthImage[1].DeviceMemory);

		vkDestroyFramebuffer(Context.Device, FrameBuffers[1], VK_NULL_HANDLE);
	}


	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		vkDestroyFence(Context.Device, PerFrame[i].FrameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(Context.Device, PerFrame[i].PresentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(Context.Device, PerFrame[i].RenderCompleteSemaphore, VK_NULL_HANDLE);
	}

	vkuDestroySwapchain(&Context, &Swapchain);
	//////////

	// Destroy command pools
	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
		vkDestroyCommandPool(Context.Device, PerFrame[i].CommandPool, VK_NULL_HANDLE);

	DBGPRINTF(DEBUG_INFO"Remaining Vulkan memory blocks:\n");
	vkuMem_Print(VkZone);
	vkuMem_Destroy(&Context, VkZone);
	DBGPRINTF(DEBUG_NONE);

#ifdef _DEBUG
	vkDestroyDebugUtilsMessengerEXT(Instance, debugMessenger, VK_NULL_HANDLE);
#endif
}
