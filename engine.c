#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
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
#include "audio/audio.h"
#include "physics/physics.h"
#include "models.h"
#include "textures.h"
#include "skybox.h"
#include "shadow.h"
#include "nebula.h"
#include "composite.h"
#include "perframe.h"
#include "sounds.h"

// Initial window size
uint32_t Width=1920, Height=1080;

// extern switch from vr.c for if we have VR available
extern bool IsVR;

// Vulkan instance handle and context structs
VkInstance Instance;
VkuContext_t Context;

// Vulkan memory allocator zone
VkuMemZone_t *VkZone;

// Camera data
Camera_t Camera;
matrix ModelView;

// extern timing data from system main
extern float fps, fTimeStep, fTime;

// Main particle system struct
ParticleSystem_t ParticleSystem;

// 3D Model data
BModel_t Models[NUM_MODELS];

// Texture images
VkuImage_t Textures[NUM_TEXTURES];

// Sound sample data
Sample_t Sounds[NUM_SOUNDS];

// Vulkan swapchain helper struct
VkuSwapchain_t Swapchain;

// Multisample anti-alias sample count
VkSampleCountFlags MSAA=VK_SAMPLE_COUNT_4_BIT;

// Colorbuffer image and format
VkFormat ColorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
VkuImage_t ColorImage[2];		// left and right eye color buffer

// Depth buffer image and format
VkFormat DepthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;
VkuImage_t DepthImage[2];		// left and right eye depth buffers

// Primary rendering vulkan stuff
VkuDescriptorSet_t DescriptorSet;
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;
//////

// Asteroid data
VkuBuffer_t Asteroid_Instance;

#define NUM_ASTEROIDS 1000
RigidBody_t Asteroids[NUM_ASTEROIDS];
//////

// Thread stuff
typedef struct
{
	uint32_t Index, Eye;
	struct
	{
		VkDescriptorPool DescriptorPool[2];
		VkCommandPool CommandPool[2];
		VkCommandBuffer SecCommandBuffer[2];
	} PerFrame[VKU_MAX_FRAME_COUNT];
} ThreadData_t;

#define NUM_THREADS 3
ThreadData_t ThreadData[NUM_THREADS];
ThreadWorker_t Thread[NUM_THREADS], ThreadPhysics;
pthread_barrier_t ThreadBarrier, ThreadBarrier_Physics;
//////

void RecreateSwapchain(void);

// Create functions for creating render data for asteroids
bool CreateFramebuffers(uint32_t Eye, uint32_t targetWidth, uint32_t targetHeight)
{
	vkuCreateTexture2D(&Context, &ColorImage[Eye], targetWidth, targetHeight, ColorFormat, MSAA);
	vkuCreateTexture2D(&Context, &DepthImage[Eye], targetWidth, targetHeight, DepthFormat, MSAA);
	vkuCreateTexture2D(&Context, &ColorResolve[Eye], targetWidth, targetHeight, ColorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(CommandBuffer, ColorImage[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(CommandBuffer, DepthImage[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(CommandBuffer, ColorResolve[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, CommandBuffer);

	return true;
}

bool CreatePipeline(void)
{
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuCreateHostBuffer(&Context, &PerFrame[i].Main_UBO_Buffer[0], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Main_UBO_Buffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Main_UBO[0]);

		vkuCreateHostBuffer(&Context, &PerFrame[i].Main_UBO_Buffer[1], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Main_UBO_Buffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Main_UBO[1]);
	}

	vkuInitDescriptorSet(&DescriptorSet, &Context);

	vkuDescriptorSet_AddBinding(&DescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&DescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&DescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&DescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&DescriptorSet, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

	vkuAssembleDescriptorSetLayout(&DescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&DescriptorSet.DescriptorSetLayout,
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

	Pipeline.DepthTest=VK_TRUE;
	Pipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	Pipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
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

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&ColorFormat,
		.depthAttachmentFormat=DepthFormat,
	};

	if(!vkuAssemblePipeline(&Pipeline, &PipelineRenderingCreateInfo))
		return false;

	return true;
}
//////

// Build up random data for skybox and asteroid field
void GenerateSkyParams(void)
{
	// Build a skybox param struct with random values
	Skybox_UBO_t Skybox_UBO;

	Skybox_UBO.uOffset=Vec4(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec4_Normalize(&Skybox_UBO.uOffset);

	Skybox_UBO.uNebulaAColor=Vec3(RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaADensity=RandFloat()*2.0f;

	Skybox_UBO.uNebulaBColor=Vec3(RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaBDensity=RandFloat()*2.0f;

	Skybox_UBO.uSunPosition=Vec4(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec4_Normalize(&Skybox_UBO.uSunPosition);

	const float MaxSun=5.0f;
	Skybox_UBO.uSunColor=Vec4(min(MaxSun, RandFloat()*MaxSun+0.5f), min(MaxSun, RandFloat()*MaxSun+0.5f), min(MaxSun, RandFloat()*MaxSun+0.5f), 0.0f);
	Skybox_UBO.uSunSize=1.0f/(RandFloat()*1000.0f+100.0f);
	Skybox_UBO.uSunFalloff=RandFloat()*16.0f+8.0f;

	Skybox_UBO.uStarsScale=200.0f;
	Skybox_UBO.uStarDensity=8.0f;

	// Copy it out to the other eyes and frames
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		memcpy(PerFrame[i].Skybox_UBO[0], &Skybox_UBO, sizeof(Skybox_UBO_t));
		memcpy(PerFrame[i].Skybox_UBO[1], &Skybox_UBO, sizeof(Skybox_UBO_t));
	}

	// Set up rigid body reps for asteroids
	const float AsteroidFieldMinRadius=50.0f;
	const float AsteroidFieldMaxRadius=4000.0f;
	const float AsteroidMinRadius=0.05f;
	const float AsteroidMaxRadius=40.0f;

	uint32_t i=0, tries=0;

	memset(Asteroids, 0, sizeof(RigidBody_t)*NUM_ASTEROIDS);

	while(i<NUM_ASTEROIDS)
	{
		vec3 RandomDirection=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(&RandomDirection);

		RigidBody_t Asteroid;

		Asteroid.Position=Vec3(
			RandomDirection.x*(RandFloat()*(AsteroidFieldMaxRadius-AsteroidFieldMinRadius))+AsteroidFieldMinRadius,
			RandomDirection.y*(RandFloat()*(AsteroidFieldMaxRadius-AsteroidFieldMinRadius))+AsteroidFieldMinRadius,
			RandomDirection.z*(RandFloat()*(AsteroidFieldMaxRadius-AsteroidFieldMinRadius))+AsteroidFieldMinRadius
		);
		Asteroid.Radius=(RandFloat()*(AsteroidMaxRadius-AsteroidMinRadius))+AsteroidMinRadius;

		bool overlapping=false;

		for(uint32_t j=0;j<i;j++)
		{
			if(Vec3_Distance(Asteroid.Position, Asteroids[j].Position)<Asteroid.Radius+Asteroids[j].Radius)
				overlapping=true;
		}

		if(!overlapping)
			Asteroids[i++]=Asteroid;

		tries++;

		if(tries>NUM_ASTEROIDS*NUM_ASTEROIDS)
			break;
	}
	//////

	// Set up instance data for asteroid rendering
	matrix *Data=NULL;

	if(!Asteroid_Instance.Buffer)
		vkuCreateHostBuffer(&Context, &Asteroid_Instance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, Asteroid_Instance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&Data);

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		const float radiusScale=1.5f;

		Data[i]=MatrixIdentity();
		Data[i]=MatrixMult(Data[i], MatrixTranslatev(Asteroids[i].Position));
		Data[i]=MatrixMult(Data[i], MatrixRotate(RandFloat()*PI*2.0f, 1.0f, 0.0f, 0.0f));
		Data[i]=MatrixMult(Data[i], MatrixRotate(RandFloat()*PI*2.0f, 0.0f, 1.0f, 0.0f));
		Data[i]=MatrixMult(Data[i], MatrixRotate(RandFloat()*PI*2.0f, 0.0f, 0.0f, 1.0f));
		Data[i]=MatrixMult(Data[i], MatrixScale(Asteroids[i].Radius/radiusScale, Asteroids[i].Radius/radiusScale, Asteroids[i].Radius/radiusScale));

		Asteroids[i].Velocity=Vec3b(0.0f);
		Asteroids[i].Force=Vec3b(0.0f);

		Asteroids[i].Mass=(1.0f/3000.0f)*(1.33333333f*PI*Asteroids[i].Radius);
		Asteroids[i].invMass=1.0f/Asteroids[i].Mass;
	}

	vkUnmapMemory(Context.Device, Asteroid_Instance.DeviceMemory);
	//////
}
//////

// General thread contructor for threads using Vulkan
void Thread_Constructor(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	for(uint32_t Frame=0;Frame<Swapchain.NumImages;Frame++)
	{
		for(uint32_t Eye=0;Eye<2;Eye++)
		{
			vkCreateCommandPool(Context.Device, &(VkCommandPoolCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags=0,
				.queueFamilyIndex=Context.QueueFamilyIndex,
			}, VK_NULL_HANDLE, &Data->PerFrame[Frame].CommandPool[Eye]);

			vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool=Data->PerFrame[Frame].CommandPool[Eye],
				.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,
				.commandBufferCount=1,
			}, &Data->PerFrame[Frame].SecCommandBuffer[Eye]);

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
			}, VK_NULL_HANDLE, &Data->PerFrame[Frame].DescriptorPool[Eye]);
		}
	}
}

// General thread destructor for vulkan threads
void Thread_Destructor(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	for(uint32_t Frame=0;Frame<Swapchain.NumImages;Frame++)
	{
		for(uint32_t Eye=0;Eye<2;Eye++)
		{
			vkDestroyCommandPool(Context.Device, Data->PerFrame[Frame].CommandPool[Eye], VK_NULL_HANDLE);
			vkDestroyDescriptorPool(Context.Device, Data->PerFrame[Frame].DescriptorPool[Eye], VK_NULL_HANDLE);
		}
	}
}

// Asteroids render pass thread
void Thread_Main(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetDescriptorPool(Context.Device, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->PerFrame[Data->Index].CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=&(VkCommandBufferInheritanceRenderingInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
				.colorAttachmentCount=1,
				.pColorAttachmentFormats=&ColorFormat,
				.depthAttachmentFormat=DepthFormat,
				.rasterizationSamples=MSAA
			},
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	// Draw the models
	vkCmdBindVertexBuffers(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 1, 1, &Asteroid_Instance.Buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet, 0, &Textures[2*i+0]);
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet, 1, &Textures[2*i+1]);
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet, 2, &ShadowDepth);
		vkuDescriptorSet_UpdateBindingBufferInfo(&DescriptorSet, 3, PerFrame[Data->Index].Main_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
		vkuDescriptorSet_UpdateBindingBufferInfo(&DescriptorSet, 4, PerFrame[Data->Index].Skybox_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&DescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);

		vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

		matrix local=MatrixIdentity();
		local=MatrixMult(local, MatrixRotate(fTime, 1.0f, 0.0f, 0.0f));
		local=MatrixMult(local, MatrixRotate(fTime, 0.0f, 1.0f, 0.0f));

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &local);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &Models[i].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<Models[i].NumMesh;j++)
		{
			vkCmdBindIndexBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Models[i].Mesh[j].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Models[i].Mesh[j].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}

	vkEndCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye]);

	pthread_barrier_wait(&ThreadBarrier);
}

// Skybox render pass thread
void Thread_Skybox(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;

	vkResetDescriptorPool(Context.Device, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->PerFrame[Data->Index].CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=&(VkCommandBufferInheritanceRenderingInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
				.colorAttachmentCount=1,
				.pColorAttachmentFormats=&ColorFormat,
				.depthAttachmentFormat=DepthFormat,
				.rasterizationSamples=MSAA
			},
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&SkyboxDescriptorSet, 0, PerFrame[Data->Index].Main_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&SkyboxDescriptorSet, 1, PerFrame[Data->Index].Skybox_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&SkyboxDescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);

	vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipelineLayout, 0, 1, &SkyboxDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 60, 1, 0, 0);

	vkEndCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye]);
	
	pthread_barrier_wait(&ThreadBarrier);
}

// Particles render pass thread, also has volumetric rendering
void Thread_Particles(void *Arg)
{
	ThreadData_t *Data=(ThreadData_t *)Arg;
	static uint32_t uFrame=0;

	vkResetDescriptorPool(Context.Device, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye], 0);
	vkResetCommandPool(Context.Device, Data->PerFrame[Data->Index].CommandPool[Data->Eye], 0);

	vkBeginCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=&(VkCommandBufferInheritanceRenderingInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
				.colorAttachmentCount=1,
				.pColorAttachmentFormats=&ColorFormat,
				.depthAttachmentFormat=DepthFormat,
				.rasterizationSamples=MSAA
			},
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)rtWidth, (float)rtHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth, rtHeight } });

	matrix Modelview=MatrixMult(PerFrame[Data->Index].Main_UBO[Data->Eye]->modelview, PerFrame[Data->Index].Main_UBO[Data->Eye]->HMD);
	ParticleSystem_Draw(&ParticleSystem, Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Data->PerFrame[Data->Index].DescriptorPool[Data->Eye], Modelview, PerFrame[Data->Index].Main_UBO[Data->Eye]->projection);

	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, VolumePipeline.Pipeline);

	uFrame++;
	vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VolumePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &uFrame);

	vkuDescriptorSet_UpdateBindingImageInfo(&VolumeDescriptorSet, 0, &Textures[TEXTURE_VOLUME]);
	vkuDescriptorSet_UpdateBindingBufferInfo(&VolumeDescriptorSet, 1, PerFrame[Data->Index].Main_UBO_Buffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&VolumeDescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);
	vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, VolumePipelineLayout, 0, 1, &VolumeDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 36, 1, 0, 0);

	vkEndCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye]);

	pthread_barrier_wait(&ThreadBarrier);
}

// Render everything together, per-eye, per-frame index
void EyeRender(VkCommandBuffer CommandBuffer, uint32_t Index, uint32_t Eye, matrix Pose)
{
	// Generate the projection matrix
	PerFrame[Index].Main_UBO[Eye]->projection=EyeProjection[Eye];

	// Set up the modelview matrix
	PerFrame[Index].Main_UBO[Eye]->modelview=ModelView;

	PerFrame[Index].Main_UBO[Eye]->HMD=Pose;

	PerFrame[Index].Main_UBO[Eye]->light_color=PerFrame[Index].Skybox_UBO[Eye]->uSunColor;
	PerFrame[Index].Main_UBO[Eye]->light_direction=PerFrame[Index].Skybox_UBO[Eye]->uSunPosition;
	PerFrame[Index].Main_UBO[Eye]->light_direction.w=PerFrame[Index].Skybox_UBO[Eye]->uSunSize;

	PerFrame[Index].Main_UBO[Eye]->light_mvp=Shadow_UBO.mvp;

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRendering(CommandBuffer, &(VkRenderingInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
		.flags=VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
		.renderArea=(VkRect2D){ { 0, 0 }, { rtWidth, rtHeight } },
		.layerCount=1,
		.colorAttachmentCount=1,
		.pColorAttachments=&(VkRenderingAttachmentInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView=ColorImage[Eye].View,
			.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode=VK_RESOLVE_MODE_AVERAGE_BIT,
			.resolveImageView=ColorResolve[Eye].View,
			.resolveImageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue=(VkClearValue){ .color.float32={ 0.0f, 0.0f, 0.0f, 1.0f } },
		},
		.pDepthAttachment=&(VkRenderingAttachmentInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView=DepthImage[Eye].View,
			.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue=(VkClearValue){ .depthStencil={ 0.0f, 0 } },
		},
	});

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

	pthread_barrier_wait(&ThreadBarrier);

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(CommandBuffer, 3, (VkCommandBuffer[])
	{
		ThreadData[0].PerFrame[Index].SecCommandBuffer[Eye],
		ThreadData[1].PerFrame[Index].SecCommandBuffer[Eye],
		ThreadData[2].PerFrame[Index].SecCommandBuffer[Eye]
	});

	vkCmdEndRendering(CommandBuffer);
}

// Runs anything physics related
void Thread_Physics(void *Arg)
{
	// Get a pointer to the emitter that's providing the positions
	ParticleEmitter_t *Emitter=List_GetPointer(&ParticleSystem.Emitters, 0);

	for(uint32_t i=0;i<Emitter->NumParticles;i++)
	{
		if(Emitter->Particles[i].ID!=Emitter->ID)
		{
			// Get those positions and set the other emitter's positions to those
			ParticleSystem_SetEmitterPosition(&ParticleSystem, Emitter->Particles[i].ID, Emitter->Particles[i].pos);

			// If this particle is dead, delete that emitter and reset it's ID 
			if(Emitter->Particles[i].life<0.0f)
			{
				ParticleSystem_DeleteEmitter(&ParticleSystem, Emitter->Particles[i].ID);
				Emitter->Particles[i].ID=0;
			}
		}
	}

	ParticleSystem_Step(&ParticleSystem, fTimeStep);

	// Loop through objects, integrate and check/resolve collisions
	for(int i=0;i<NUM_ASTEROIDS;i++)
	{
		// Run physics integration on the asteroids
		PhysicsIntegrate(&Asteroids[i], fTimeStep);

		// Check asteroids against other asteroids
		for(int j=i+1;j<NUM_ASTEROIDS;j++)
			PhysicsSphereToSphereCollisionResponse(&Asteroids[i], &Asteroids[j]);

		// Check asteroids against the camera
		PhysicsCameraToSphereCollisionResponse(&Camera, &Asteroids[i]);
	
		// Check asteroids against projectile particles
		// Emitter '0' on the particle system contains particles that drive the projectile physics
		ParticleEmitter_t *Emitter=List_GetPointer(&ParticleSystem.Emitters, 0);
		// Loop through all the possible particles
		for(uint32_t j=0;j<Emitter->NumParticles;j++)
		{
			// If the particle ID matches with the projectile ID, then check collision and respond
			if(Emitter->Particles[j].ID!=Emitter->ID)
				PhysicsParticleToSphereCollisionResponse(&Emitter->Particles[j], &Asteroids[i]);
		}
	}
	//////

	// Update camera and modelview matrix
	ModelView=CameraUpdate(&Camera, fTimeStep);
	Audio_SetListenerOrigin(Camera.Position, Camera.Right);
	//////

	// Update instance matrix translation positions
	matrix *Data=NULL;
	vkMapMemory(Context.Device, Asteroid_Instance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&Data);

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
		Data[i].w=Vec4(Asteroids[i].Position.x, Asteroids[i].Position.y, Asteroids[i].Position.z, 1.0f);

	vkUnmapMemory(Context.Device, Asteroid_Instance.DeviceMemory);
	//////

	// Barrier now that we're done here
	pthread_barrier_wait(&ThreadBarrier_Physics);
}

// Render call from system main event loop
void Render(void)
{
	static uint32_t Index=0;
	uint32_t imageIndex;
	matrix Pose;

	Thread_AddJob(&ThreadPhysics, Thread_Physics, NULL);

	if(!IsVR)
		EyeProjection[0]=MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f);

	Pose=GetHeadPose();

	VkResult Result=vkAcquireNextImageKHR(Context.Device, Swapchain.Swapchain, UINT64_MAX, PerFrame[Index].PresentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
	{
		DBGPRINTF("Swapchain out of date... Rebuilding.\n");
		RecreateSwapchain();
		return;
	}

	vkWaitForFences(Context.Device, 1, &PerFrame[Index].FrameFence, VK_TRUE, UINT64_MAX);

	// Reset the frame fence and command pool (and thus the command buffer)
	vkResetFences(Context.Device, 1, &PerFrame[Index].FrameFence);
	vkResetDescriptorPool(Context.Device, PerFrame[Index].DescriptorPool, 0);
	vkResetCommandPool(Context.Device, PerFrame[Index].CommandPool, 0);

	// Start recording the commands
	vkBeginCommandBuffer(PerFrame[Index].CommandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	// Update shadow depth map
	ShadowUpdateMap(PerFrame[Index].CommandBuffer, Index);

	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// Wait for physics to finish before rendering, particle drawing needs data done first.
	pthread_barrier_wait(&ThreadBarrier_Physics);

	EyeRender(PerFrame[Index].CommandBuffer, Index, 0, Pose);

	if(IsVR)
	{
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		EyeRender(PerFrame[Index].CommandBuffer, Index, 1, Pose);
	}

	// Final drawing compositing
	CompositeDraw(Index, 0);
	//////

	if(IsVR)
	{
		// Other eye compositing
		CompositeDraw(Index, 1);

		// Transition layouts into transfer source for OpenVR
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}

	vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	vkEndCommandBuffer(PerFrame[Index].CommandBuffer);

	// Submit command queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&PerFrame[Index].PresentCompleteSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&PerFrame[Index].RenderCompleteSemaphore,
		.commandBufferCount=1,
		.pCommandBuffers=&PerFrame[Index].CommandBuffer,
	}, PerFrame[Index].FrameFence);

	// And present it to the screen
	Result=vkQueuePresentKHR(Context.Queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&PerFrame[Index].RenderCompleteSemaphore,
		.swapchainCount=1,
		.pSwapchains=&Swapchain.Swapchain,
		.pImageIndices=&imageIndex,
	});

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
	{
		DBGPRINTF("Swapchain out of date... Rebuilding.\n");
		RecreateSwapchain();
		return;
	}

	// Send eye images over to OpenVR
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
			.m_nSampleCount=1,
		};

		struct Texture_t texture={ &vulkanData, ETextureType_TextureType_Vulkan, EColorSpace_ColorSpace_Auto };

		vulkanData.m_nImage=(uint64_t)ColorResolve[EVREye_Eye_Left].Image;
		VRCompositor->Submit(EVREye_Eye_Left, &texture, &bounds, EVRSubmitFlags_Submit_Default);

		vulkanData.m_nImage=(uint64_t)ColorResolve[EVREye_Eye_Right].Image;
		VRCompositor->Submit(EVREye_Eye_Right, &texture, &bounds, EVRSubmitFlags_Submit_Default);
	}

	Index=(Index+1)%Swapchain.NumImages;
}

// Initialization call from system main
bool Init(void)
{
	srand(123);

	Event_Add(EVENT_KEYDOWN, Event_KeyDown);
	Event_Add(EVENT_KEYUP, Event_KeyUp);
	Event_Add(EVENT_MOUSE, Event_Mouse);

	VkZone=vkuMem_Init(&Context, (size_t)(Context.DeviceProperties2.maxMemoryAllocationSize*0.8f));

	if(VkZone==NULL)
		return false;

	CameraInit(&Camera, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));

	if(!Audio_Init())
		return false;

	if(!Audio_LoadStatic("./assets/pew1.wav", &Sounds[SOUND_PEW1]))
		return false;

	if(!Audio_LoadStatic("./assets/pew2.wav", &Sounds[SOUND_PEW2]))
		return false;

	if(!Audio_LoadStatic("./assets/pew3.wav", &Sounds[SOUND_PEW3]))
		return false;

	if(!Audio_LoadStatic("./assets/stone1.wav", &Sounds[SOUND_STONE1]))
		return false;

	if(!Audio_LoadStatic("./assets/stone2.wav", &Sounds[SOUND_STONE2]))
		return false;

	if(!Audio_LoadStatic("./assets/stone3.wav", &Sounds[SOUND_STONE3]))
		return false;

	if(!Audio_LoadStatic("./assets/crash.wav", &Sounds[SOUND_CRASH]))
		return false;

	if(!Audio_LoadStatic("./assets/explode1.wav", &Sounds[SOUND_EXPLODE1]))
		return false;

	if(!Audio_LoadStatic("./assets/explode2.wav", &Sounds[SOUND_EXPLODE2]))
		return false;

	if(!Audio_LoadStatic("./assets/explode3.wav", &Sounds[SOUND_EXPLODE3]))
		return false;

	// Load models
	if(LoadBModel(&Models[MODEL_ASTEROID1], "./assets/asteroid1.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID1]);
	else
		return false;

	if(LoadBModel(&Models[MODEL_ASTEROID2], "./assets/asteroid2.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID2]);
	else
		return false;

	if(LoadBModel(&Models[MODEL_ASTEROID3], "./assets/asteroid3.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID3]);
	else
		return false;

	if(LoadBModel(&Models[MODEL_ASTEROID4], "./assets/asteroid4.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID4]);
	else
		return false;

	uint32_t TriangleCount=0;

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		for(uint32_t j=0;j<Models[i].NumMesh;j++)
			TriangleCount+=Models[i].Mesh[j].NumFace;
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

	GenNebulaVolume(&Context, &Textures[TEXTURE_VOLUME]);

	// Create primary pipeline
	CreatePipeline();

	// Create skybox pipeline
	CreateSkyboxPipeline();
	GenerateSkyParams();

	// Create volumetric rendering pipeline
	CreateVolumePipeline();

	// Create shadow map pipeline
	CreateShadowPipeline();
	CreateShadowMap();

	// Create compositing pipeline
	CreateCompositePipeline();

	// Set up particle system
	if(!ParticleSystem_Init(&ParticleSystem))
		return false;

	ParticleSystem_SetGravity(&ParticleSystem, 0.0f, 0.0f, 0.0f);

	vec3 Zero=Vec3(0.0f, 0.0f, 0.0f);
	ParticleSystem_AddEmitter(&ParticleSystem, Zero, Zero, Zero, 0.0f, 100, true, NULL);

	// Create primary frame buffers, depth image
	CreateFramebuffers(0, rtWidth, rtHeight);
	CreateCompositeFramebuffers(0, rtWidth, rtHeight);

	// Second eye framebuffer for VR
	if(IsVR)
	{
		CreateFramebuffers(1, rtWidth, rtHeight);
		CreateCompositeFramebuffers(1, rtWidth, rtHeight);
	}

	// Other per-frame data
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		// Create needed fence and semaphores for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(Context.Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &PerFrame[i].FrameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &PerFrame[i].PresentCompleteSemaphore);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &PerFrame[i].RenderCompleteSemaphore);

		// Per-frame descriptor pool for main thread
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
		}, VK_NULL_HANDLE, &PerFrame[i].DescriptorPool);

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

	// Set up and initialize threads
	for(uint32_t i=0;i<NUM_THREADS;i++)
	{
		Thread_Init(&Thread[i]);
		Thread_AddConstructor(&Thread[i], Thread_Constructor, (void *)&ThreadData[i]);
		Thread_AddDestructor(&Thread[i], Thread_Destructor, (void *)&ThreadData[i]);
		Thread_Start(&Thread[i]);
	}

	// Synchronization barrier, count is number of threads+main thread
	pthread_barrier_init(&ThreadBarrier, NULL, NUM_THREADS+1);

	// Thread for physics, and sync barrier
	Thread_Init(&ThreadPhysics);
	Thread_Start(&ThreadPhysics);
	pthread_barrier_init(&ThreadBarrier_Physics, NULL, 2);

	return true;
}

// Rebuild vulkan swapchain and related data
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

		// Swapchain, framebuffer, and depthbuffer destruction
		vkuDestroyImageBuffer(&Context, &ColorImage[0]);
		vkuDestroyImageBuffer(&Context, &ColorResolve[0]);
		vkuDestroyImageBuffer(&Context, &ColorBlur[0]);
		vkuDestroyImageBuffer(&Context, &ColorTemp[0]);
		vkuDestroyImageBuffer(&Context, &DepthImage[0]);

		if(IsVR)
		{
			vkuDestroyImageBuffer(&Context, &ColorImage[1]);
			vkuDestroyImageBuffer(&Context, &ColorResolve[1]);
			vkuDestroyImageBuffer(&Context, &ColorBlur[1]);
			vkuDestroyImageBuffer(&Context, &ColorTemp[1]);
			vkuDestroyImageBuffer(&Context, &DepthImage[1]);
		}
			
		// Destroy the swapchain
		vkuDestroySwapchain(&Context, &Swapchain);

		// Recreate the swapchain
		vkuCreateSwapchain(&Context, &Swapchain, Width, Height, VK_TRUE);

		// Recreate the framebuffer
		CreateFramebuffers(0, rtWidth, rtHeight);
		CreateCompositeFramebuffers(0, rtWidth, rtHeight);

		if(IsVR)
		{
			CreateFramebuffers(1, rtWidth, rtHeight);
			CreateCompositeFramebuffers(1, rtWidth, rtHeight);
		}
	}
}

// Destroy call from system main
void Destroy(void)
{
	vkDeviceWaitIdle(Context.Device);

	Audio_Destroy();

	Zone_Free(Zone, Sounds[SOUND_PEW1].data);
	Zone_Free(Zone, Sounds[SOUND_PEW2].data);
	Zone_Free(Zone, Sounds[SOUND_PEW3].data);
	Zone_Free(Zone, Sounds[SOUND_STONE1].data);
	Zone_Free(Zone, Sounds[SOUND_STONE2].data);
	Zone_Free(Zone, Sounds[SOUND_STONE3].data);
	Zone_Free(Zone, Sounds[SOUND_CRASH].data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE1].data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE2].data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE3].data);

	DestroyOpenVR();

	for(uint32_t i=0;i<NUM_THREADS;i++)
		Thread_Destroy(&Thread[i]);

	Thread_Destroy(&ThreadPhysics);

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

	// Compositing pipeline
	DestroyComposite();
	//////////

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
		vkuDestroyBuffer(&Context, &Models[i].VertexBuffer);

		for(uint32_t j=0;j<Models[i].NumMesh;j++)
			vkuDestroyBuffer(&Context, &Models[i].Mesh[j].IndexBuffer);

		FreeBModel(&Models[i]);
	}
	//////////

	// Shadow map destruction
	DestroyShadow();
	//////////

	// Volume rendering
	vkDestroyDescriptorSetLayout(Context.Device, VolumeDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, VolumePipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, VolumePipelineLayout, VK_NULL_HANDLE);
	//////////

	// Main render destruction
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkUnmapMemory(Context.Device, PerFrame[i].Main_UBO_Buffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Main_UBO_Buffer[0]);

		vkUnmapMemory(Context.Device, PerFrame[i].Main_UBO_Buffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Main_UBO_Buffer[1]);
	}

	vkDestroyDescriptorSetLayout(Context.Device, DescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	//vkDestroyRenderPass(Context.Device, RenderPass.RenderPass, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, PipelineLayout, VK_NULL_HANDLE);
	//////////

	// Swapchain, framebuffer, and depthbuffer destruction
	vkuDestroyImageBuffer(&Context, &ColorImage[0]);
	vkuDestroyImageBuffer(&Context, &ColorResolve[0]);
	vkuDestroyImageBuffer(&Context, &ColorBlur[0]);
	vkuDestroyImageBuffer(&Context, &ColorTemp[0]);
	vkuDestroyImageBuffer(&Context, &DepthImage[0]);

	//vkDestroyFramebuffer(Context.Device, Framebuffers[0].Framebuffer, VK_NULL_HANDLE);

	if(IsVR)
	{
		vkuDestroyImageBuffer(&Context, &ColorImage[1]);
		vkuDestroyImageBuffer(&Context, &ColorResolve[1]);
		vkuDestroyImageBuffer(&Context, &ColorBlur[1]);
		vkuDestroyImageBuffer(&Context, &ColorTemp[1]);
		vkuDestroyImageBuffer(&Context, &DepthImage[1]);

		//vkDestroyFramebuffer(Context.Device, Framebuffers[1].Framebuffer, VK_NULL_HANDLE);
	}

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		// Destroy sync objects
		vkDestroyFence(Context.Device, PerFrame[i].FrameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(Context.Device, PerFrame[i].PresentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(Context.Device, PerFrame[i].RenderCompleteSemaphore, VK_NULL_HANDLE);

		// Destroy main thread descriptor pools
		vkDestroyDescriptorPool(Context.Device, PerFrame[i].DescriptorPool, VK_NULL_HANDLE);

		// Destroy command pools
		vkDestroyCommandPool(Context.Device, PerFrame[i].CommandPool, VK_NULL_HANDLE);
	}

	vkuDestroySwapchain(&Context, &Swapchain);
	//////////

	DBGPRINTF(DEBUG_INFO"Remaining Vulkan memory blocks:\n");
	vkuMem_Print(VkZone);
	vkuMem_Destroy(&Context, VkZone);
	DBGPRINTF(DEBUG_NONE);
}
