#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stdatomic.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "camera/camera.h"
#include "model/bmodel.h"
#include "image/image.h"
#include "utils/list.h"
#include "lights/lights.h"
#include "utils/event.h"
#include "particle/particle.h"
#include "system/threads.h"
#include "vr/vr.h"
#include "font/font.h"
#include "audio/audio.h"
#include "physics/physics.h"
#include "ui/ui.h"
#include "console/console.h"

#include "models.h"
#include "textures.h"
#include "line.h"
#include "sphere.h"
#include "lighting.h"
#include "skybox.h"
#include "shadow.h"
#include "volume.h"
#include "composite.h"
#include "perframe.h"
#include "sounds.h"
#include "sfx.h"
#include "music.h"

//#include <cvmarkers.h>
//PCV_PROVIDER provider;
//PCV_MARKERSERIES flagSeries;

extern bool isDone;

// Render size
uint32_t renderWidth=1920, renderHeight=1080;

// external switch from system for if VR was initialized
extern bool isVR;
XruContext_t xrContext;

// Vulkan instance handle and context structs
VkInstance vkInstance;
VkuContext_t vkContext;

// Camera data
Camera_t camera, enemy;
matrix modelView, projection[2], headPose;

// extern timing data from system main
extern float fps, fTimeStep, fTime;

// Main particle system struct
ParticleSystem_t particleSystem;

// Particle system emitters as rigid bodies with life and ID hashmaps
#define MAX_EMITTERS 1000
RigidBody_t particleEmitters[MAX_EMITTERS];
uint32_t particleEmittersID[MAX_EMITTERS];
float particleEmittersLife[MAX_EMITTERS];

// 3D Model data
BModel_t models[NUM_MODELS];

// Texture images
VkuImage_t textures[NUM_TEXTURES];

// Sound sample data
Sample_t sounds[NUM_SOUNDS];

// Vulkan swapchain helper struct
VkuSwapchain_t swapchain;

// Multisample anti-alias sample count
VkSampleCountFlagBits MSAA=VK_SAMPLE_COUNT_4_BIT;

// Colorbuffer image and format
VkFormat colorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
VkuImage_t colorImage[2];		// left and right eye color buffer

// Depth buffer image and format
VkFormat depthFormat=VK_FORMAT_D32_SFLOAT;
VkuImage_t depthImage[2];		// left and right eye depth buffers

// Renderpass for primary renderer
VkRenderPass renderPass;
// Framebuffers, per-eye
VkFramebuffer framebuffer[2];

// Asteroid data
#define NUM_ASTEROIDS 1000
RigidBody_t asteroids[NUM_ASTEROIDS];

VkuBuffer_t asteroidInstance;
matrix *asteroidInstancePtr;
//////

// Thread stuff
typedef struct
{
	uint32_t index, eye;
	struct
	{
		VkDescriptorPool descriptorPool[2];
		VkCommandPool commandPool[2];
		VkCommandBuffer secCommandBuffer[2];
	} perFrame[VKU_MAX_FRAME_COUNT];
} ThreadData_t;

#define NUM_THREADS 1
ThreadData_t threadData[NUM_THREADS];
ThreadWorker_t thread[NUM_THREADS], threadPhysics;

ThreadBarrier_t threadBarrier;
ThreadBarrier_t physicsThreadBarrier;
//////

// UI Stuff
Font_t Fnt; // Fnt instead of Font, because Xlib is dumb and declares a type Font *rolley-eyes*
UI_t UI;

uint32_t volumeID=UINT32_MAX;
uint32_t faceID=UINT32_MAX;
uint32_t cursorID=UINT32_MAX;
uint32_t colorShiftID=UINT32_MAX;
//////

Console_t console;

XrPosef leftHand, rightHand;
float leftTrigger, rightTrigger;
float leftGrip, rightGrip;
vec2 leftThumbstick, rightThumbstick;

bool isTargeted=false;
bool isControlPressed=false;

bool pausePhysics=false;

void RecreateSwapchain(void);
bool CreateFramebuffers(uint32_t eye);

// Build up random data for skybox and asteroid field
void GenerateSkyParams(void)
{
	// Build a skybox param struct with random values
	Skybox_UBO_t Skybox_UBO={ 0 };

	Skybox_UBO.uOffset=Vec4(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec4_Normalize(&Skybox_UBO.uOffset);

	Skybox_UBO.uNebulaAColor=Vec3(RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaADensity=RandFloat()*2.0f;

	Skybox_UBO.uNebulaBColor=Vec3(RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO.uNebulaBDensity=RandFloat()*2.0f;

	Skybox_UBO.uSunPosition=Vec4(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec4_Normalize(&Skybox_UBO.uSunPosition);

	const float minSunBrightness=0.5f;
	const float maxSunBrightness=5.0f;
	const float minSunSize=100.0f;
	const float maxSunSize=1000.0f;
	const float minSunFalloff=8.0f;
	const float maxSunFalloff=16.0f;

	Skybox_UBO.uSunColor=Vec4(
		RandFloatRange(minSunBrightness, maxSunBrightness),
		RandFloatRange(minSunBrightness, maxSunBrightness),
		RandFloatRange(minSunBrightness, maxSunBrightness),
		0.0f
	);
	Skybox_UBO.uSunSize=1.0f/RandFloatRange(minSunSize, maxSunSize);
	Skybox_UBO.uSunFalloff=RandFloatRange(minSunFalloff, maxSunFalloff);

	Skybox_UBO.uStarsScale=200.0f;
	Skybox_UBO.uStarDensity=8.0f;

	// Copy it out to the other eyes and frames
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		memcpy(perFrame[i].skyboxUBO[0], &Skybox_UBO, sizeof(Skybox_UBO_t));
		memcpy(perFrame[i].skyboxUBO[1], &Skybox_UBO, sizeof(Skybox_UBO_t));
	}

	// Set up rigid body reps for asteroids
	const float asteroidFieldMinRadius=50.0f;
	const float asteroidFieldMaxRadius=1000.0f;
	const float asteroidMinRadius=0.05f;
	const float asteroidMaxRadius=40.0f;

	uint32_t i=0, tries=0;

	memset(asteroids, 0, sizeof(RigidBody_t)*NUM_ASTEROIDS);

	// Randomly place asteroids in a sphere without any otherlapping.
	while(i<NUM_ASTEROIDS)
	{
		vec3 randomDirection=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(&randomDirection);

		RigidBody_t asteroid={ 0 };

		asteroid.position=Vec3(
			randomDirection.x*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius),
			randomDirection.y*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius),
			randomDirection.z*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius)
		);
		asteroid.radius=RandFloatRange(asteroidMinRadius, asteroidMaxRadius);

		bool overlapping=false;

		for(uint32_t j=0;j<i;j++)
		{
			if(Vec3_Distance(asteroid.position, asteroids[j].position)<asteroid.radius+asteroids[j].radius)
				overlapping=true;
		}

		if(!overlapping)
			asteroids[i++]=asteroid;

		tries++;

		if(tries>NUM_ASTEROIDS*NUM_ASTEROIDS)
			break;
	}
	//////

	// Set up instance data for asteroid rendering
	if(!asteroidInstance.buffer)
	{
		vkuCreateHostBuffer(&vkContext, &asteroidInstance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		asteroidInstancePtr=(matrix *)asteroidInstance.memory->mappedPointer;
	}

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		vec3 randomDirection=Vec3(
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f)
		);
		Vec3_Normalize(&randomDirection);

		asteroids[i].velocity=Vec3_Muls(randomDirection, RandFloat());
		asteroids[i].force=Vec3b(0.0f);

		asteroids[i].orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		asteroids[i].angularVelocity=Vec3_Muls(randomDirection, RandFloat());

		asteroids[i].mass=(1.0f/3000.0f)*(1.33333333f*PI*asteroids[i].radius);
		asteroids[i].invMass=1.0f/asteroids[i].mass;

		asteroids[i].inertia=0.4f*asteroids[i].mass*(asteroids[i].radius*asteroids[i].radius);
		asteroids[i].invInertia=1.0f/asteroids[i].inertia;
	}
	//////

	vec3 randomDirection=Vec3(
		RandFloatRange(-1.0f, 1.0f),
		RandFloatRange(-1.0f, 1.0f),
		RandFloatRange(-1.0f, 1.0f)
	);
	Vec3_Normalize(&randomDirection);

	CameraInit(&enemy, Vec3_Muls(randomDirection, 1200.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
}
//////

void DrawPlayer(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, Camera_t player)
{
	struct
	{
		matrix mvp;
		vec4 color, start, end;
	} linePC;

	vec4 position=Vec4_Vec3(player.position, 1.0f);
	vec4 forward=Vec4_Vec3(player.forward, 1.0f);
	vec4 up=Vec4_Vec3(player.up, 1.0f);
	vec4 right=Vec4_Vec3(player.right, 1.0f);

	matrix local=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	linePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	linePC.start=position;

	linePC.color=Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(forward, 15.0f));
	DrawLinePushConstant(commandBuffer, sizeof(linePC), &linePC);

	linePC.color=Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(up, 15.0f));
	DrawLinePushConstant(commandBuffer, sizeof(linePC), &linePC);

	linePC.color=Vec4(0.0f, 0.0f, 1.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(right, 15.0f));
	DrawLinePushConstant(commandBuffer, sizeof(linePC), &linePC);

	struct
	{
		matrix mvp;
		vec4 color;
	} spherePC;

	local=MatrixMult(
		MatrixScale(player.radius, player.radius, player.radius),
		MatrixInverse(MatrixLookAt(player.position, Vec3_Addv(player.position, player.forward), player.up))
	);

	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->HMD);
	spherePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);

	if(isTargeted)
		spherePC.color=Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	else
		spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	DrawSpherePushConstant(commandBuffer, index, sizeof(spherePC), &spherePC);
}

// General thread constructor for threads using Vulkan
void Thread_Constructor(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	for(uint32_t Frame=0;Frame<swapchain.numImages;Frame++)
	{
		for(uint32_t eye=0;eye<2;eye++)
		{
			vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.flags=0,
				.queueFamilyIndex=vkContext.graphicsQueueIndex,
			}, VK_NULL_HANDLE, &data->perFrame[Frame].commandPool[eye]);

			vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool=data->perFrame[Frame].commandPool[eye],
				.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,
				.commandBufferCount=1,
			}, &data->perFrame[Frame].secCommandBuffer[eye]);

			// Create a large descriptor pool, so I don't have to worry about readjusting for exactly what I have
			vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
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
			}, VK_NULL_HANDLE, &data->perFrame[Frame].descriptorPool[eye]);
		}
	}
}

// General thread destructor for vulkan threads
void Thread_Destructor(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	for(uint32_t Frame=0;Frame<swapchain.numImages;Frame++)
	{
		for(uint32_t eye=0;eye<2;eye++)
		{
			vkDestroyCommandPool(vkContext.device, data->perFrame[Frame].commandPool[eye], VK_NULL_HANDLE);
			vkDestroyDescriptorPool(vkContext.device, data->perFrame[Frame].descriptorPool[eye], VK_NULL_HANDLE);
		}
	}
}

void Thread_Main(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	vkResetDescriptorPool(vkContext.device, data->perFrame[data->index].descriptorPool[data->eye], 0);
	vkResetCommandPool(vkContext.device, data->perFrame[data->index].commandPool[data->eye], 0);

	vkBeginCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		//.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		//{
		//	.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		//	.pNext=&(VkCommandBufferInheritanceRenderingInfo)
		//	{
		//		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
		//		.colorAttachmentCount=1,
		//		.pColorAttachmentFormats=&ColorFormat,
		//		.depthAttachmentFormat=DepthFormat,
		//		.rasterizationSamples=MSAA
		//	},
		//}
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=renderPass,
			.framebuffer=framebuffer[data->eye]
		}
	});

	vkCmdSetViewport(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

	////// Skybox/skysphere
	DrawSkybox(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye, data->perFrame[data->index].descriptorPool[data->eye]);
	//////

	////// Asteroids
	DrawLighting(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye, data->perFrame[data->index].descriptorPool[data->eye]);
	//////

#if 1
	if(isControlPressed)
	{
		for(uint32_t i=0;i<10;i++)
		{
			const float distance=10000000000.0f;
			const float val=0.005f;
			const vec3 randVec=Vec3(RandFloatRange(-val, val), RandFloatRange(-val, val), RandFloatRange(-val, val));

			DrawLine(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye,
						Vec3_Subv(camera.position, Vec3_Subv(camera.up, camera.right)),
						Vec3_Addv(camera.position, Vec3_Muls(Vec3_Addv(camera.forward, randVec), distance)),
						Vec4(1.0f+RandFloatRange(10.0f, 500.0f), 1.0f, 1.0f, 1.0f));

			DrawLine(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye,
						Vec3_Subv(camera.position, Vec3_Addv(camera.up, camera.right)),
						Vec3_Addv(camera.position, Vec3_Muls(Vec3_Subv(camera.forward, randVec), distance)),
						Vec4(1.0f+RandFloatRange(10.0f, 500.0f), 1.0f, 1.0f, 1.0f));
		}
	}
#endif

#if 0
	if(isTargeted)
	{
		for(uint32_t i=0;i<10;i++)
		{
			const float distance=10000000000.0f;
			const float val=0.001f;
			const vec3 randVec=Vec3(RandFloatRange(-val, val), RandFloatRange(-val, val), RandFloatRange(-val, val));

			DrawLine(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye,
					 enemy.position,
					 Vec3_Addv(enemy.position, Vec3_Muls(Vec3_Addv(enemy.forward, randVec), distance)),
					 Vec4(1.0f+RandFloatRange(10.0f, 500.0f), 1.0f, 1.0f, 1.0f));
		}
	}
#endif

	// Draw VR "hands"
	if(isVR)
	{
		struct
		{
			matrix mvp;
			vec4 color;
		} spherePC;

		spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		vec3 leftPos=Vec3(leftHand.position.x, leftHand.position.y, leftHand.position.z);
		vec4 leftRot=Vec4(leftHand.orientation.x, leftHand.orientation.y, leftHand.orientation.z, leftHand.orientation.w);
		matrix local=MatrixMult(MatrixMult(QuatToMatrix(leftRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		DrawSpherePushConstant(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, sizeof(spherePC), &spherePC);

		vec3 rightPos=Vec3(rightHand.position.x, rightHand.position.y, rightHand.position.z);
		vec4 rightRot=Vec4(rightHand.orientation.x, rightHand.orientation.y, rightHand.orientation.z, rightHand.orientation.w);
		local=MatrixMult(MatrixMult(QuatToMatrix(rightRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		DrawSpherePushConstant(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, sizeof(spherePC), &spherePC);

		struct
		{
			matrix mvp;
			vec4 color;
			vec4 Verts[2];
		} linePC;

		linePC.Verts[0]=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		linePC.Verts[1]=Vec4(0.0f, 0.0f, -1.0f, 1.0f);

		linePC.color=Vec4(leftTrigger*100.0f+1.0f, 1.0f, leftGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatToMatrix(leftRot), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		linePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		DrawLinePushConstant(data->perFrame[data->index].secCommandBuffer[data->eye], sizeof(linePC), &linePC);

		linePC.color=Vec4(rightTrigger*100.0f+1.0f, 1.0f, rightGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatToMatrix(rightRot), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		linePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		DrawLinePushConstant(data->perFrame[data->index].secCommandBuffer[data->eye], sizeof(linePC), &linePC);
		}

	// Draw enemy
	DrawPlayer(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye, enemy);

#if 0
	// Draw some simple geometry to represent other client cameras
	for(uint32_t i=0;i<connectedClients;i++)
	{
		// Only draw others, not ourselves
		if(i==clientID)
			continue;

		// Note: These draw calls don't have any external geometry attached, it's in the vertex shader.

		struct
		{
			matrix mvp;
			vec4 color, start, end;
		} line_ubo={ 0 };

		vec4 position=Vec4(netCameras[i].position.x, netCameras[i].position.y, netCameras[i].position.z, 1.0f);
		vec4 forward=Vec4(netCameras[i].forward.x, netCameras[i].forward.y, netCameras[i].forward.z, 1.0f);
		vec4 up=Vec4(netCameras[i].up.x, netCameras[i].up.y, netCameras[i].up.z, 1.0f);
		netCameras[i].Right=Vec3_Cross(netCameras[i].forward, netCameras[i].up);
		vec4 right=Vec4(netCameras[i].Right.x, netCameras[i].Right.y, netCameras[i].Right.z, 1.0f);

		line_ubo.mvp=MatrixMult(perFrame[data->index].Main_UBO[data->eye]->modelview, perFrame[data->index].Main_UBO[data->eye]->projection);
		line_ubo.start=position;

		vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.mainPipeline);

		line_ubo.color=Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(forward, 15.0f));
		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 2, 1, 0, 0);

		line_ubo.color=Vec4(0.0f, 1.0f, 0.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(up, 15.0f));
		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 2, 1, 0, 0);

		line_ubo.color=Vec4(0.0f, 0.0f, 1.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(right, 15.0f));
		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 2, 1, 0, 0);

		struct
		{
			matrix mvp;
			vec4 color;
		} sphere_ubo={ 0 };

		matrix local=MatrixIdentity();
		local=MatrixMult(local, MatrixScale(10.0f, 10.0f, 10.0f));
		local=MatrixMult(local, MatrixInverse(MatrixLookAt(netCameras[i].position, Vec3_Addv(netCameras[i].position, netCameras[i].forward), netCameras[i].up)));

		local=MatrixMult(local, perFrame[data->index].Main_UBO[data->eye]->modelview);
		sphere_ubo.mvp=MatrixMult(local, perFrame[data->index].Main_UBO[data->eye]->projection);

		sphere_ubo.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.mainPipeline);
		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(sphere_ubo), &sphere_ubo);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 60, 1, 0, 0);
	}
	//////
#endif

	vkEndCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye]);

	ThreadBarrier_Wait(&threadBarrier);
}

void Thread_Particles(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	vkResetDescriptorPool(vkContext.device, data->perFrame[data->index].descriptorPool[data->eye], 0);
	vkResetCommandPool(vkContext.device, data->perFrame[data->index].commandPool[data->eye], 0);

	vkBeginCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		//.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		//{
		//	.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
		//	.pNext=&(VkCommandBufferInheritanceRenderingInfo)
		//	{
		//		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO,
		//		.colorAttachmentCount=1,
		//		.pColorAttachmentFormats=&ColorFormat,
		//		.depthAttachmentFormat=DepthFormat,
		//		.rasterizationSamples=MSAA
		//	},
		//}
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=renderPass,
			.framebuffer=framebuffer[data->eye]
		}
	});

	vkCmdSetViewport(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

	matrix Modelview=MatrixMult(perFrame[data->index].mainUBO[data->eye]->modelView, perFrame[data->index].mainUBO[data->eye]->HMD);
	ParticleSystem_Draw(&particleSystem, data->perFrame[data->index].secCommandBuffer[data->eye], data->perFrame[data->index].descriptorPool[data->eye], Modelview, perFrame[data->index].mainUBO[data->eye]->projection);

	vkEndCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye]);

	ThreadBarrier_Wait(&threadBarrier);
}

// Render everything together, per-eye, per-frame index
void EyeRender(uint32_t index, uint32_t eye, matrix headPose)
{
	// Copy projection matrix
	perFrame[index].mainUBO[eye]->projection=projection[eye];

	// Copy modelview matrix
	perFrame[index].mainUBO[eye]->modelView=modelView;

	perFrame[index].mainUBO[eye]->HMD=headPose;

	perFrame[index].mainUBO[eye]->lightColor=perFrame[index].skyboxUBO[eye]->uSunColor;
	perFrame[index].mainUBO[eye]->lightDirection=perFrame[index].skyboxUBO[eye]->uSunPosition;
	perFrame[index].mainUBO[eye]->lightDirection.w=perFrame[index].skyboxUBO[eye]->uSunSize;

	perFrame[index].mainUBO[eye]->lightMVP=shadowMVP;

	// Start a render pass and clear the frame/depth buffer
	//vkCmdBeginRendering(perFrame[index]., &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.flags=VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth, renderHeight } },
	//	.layerCount=1,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=colorImage[Eye].imageView,
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.resolveMode=VK_RESOLVE_MODE_AVERAGE_BIT,
	//		.resolveImageView=colorResolve[Eye].imageView,
	//		.resolveImageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//		.clearValue=(VkClearValue){ .color.float32={ 0.0f, 0.0f, 0.0f, 1.0f } },
	//	},
	//	.pDepthAttachment=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=depthImage[Eye].imageView,
	//		.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//		.clearValue=(VkClearValue){ .depthStencil={ 0.0f, 0 } },
	//	},
	//});
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=renderPass,
		.framebuffer=framebuffer[eye],
		.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth, renderHeight } },
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}, {{{ 0.0f, 0 }}} },
	}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Set per thread data and add the job to that worker thread
	threadData[0].index=index;
	threadData[0].eye=eye;
	Thread_AddJob(&thread[0], Thread_Main, (void *)&threadData[0]);

	//threadData[1].index=index;
	//threadData[1].eye=eye;
	//Thread_AddJob(&thread[1], Thread_Particles, (void *)&threadData[1]);

	ThreadBarrier_Wait(&threadBarrier);

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(perFrame[index].commandBuffer, 1, (VkCommandBuffer[])
	{
		threadData[0].perFrame[index].secCommandBuffer[eye],
		threadData[1].perFrame[index].secCommandBuffer[eye]
	});

	vkCmdNextSubpass(perFrame[index].commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	////// Volume cloud
	DrawVolume(perFrame[index].commandBuffer, index, eye, perFrame[index].descriptorPool);
	//////

	matrix Modelview=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	ParticleSystem_Draw(&particleSystem, perFrame[index].commandBuffer, perFrame[index].descriptorPool, Modelview, perFrame[index].mainUBO[eye]->projection);

	//vkCmdEndRendering(perFrame[index].commandBuffer);
	vkCmdEndRenderPass(perFrame[index].commandBuffer);
}

int planeSphereIntersection(vec4 plane, RigidBody_t sphere, vec3 *intersectionA, vec3 *intersectionB)
{
	const vec3 planeVec3=Vec3(plane.x, plane.y, plane.z);
	const float planeSphereSqDist=Vec3_Dot(planeVec3, sphere.position)+plane.w;
	const float planeSqLength=Vec3_Dot(planeVec3, planeVec3);

	const float distance=fabsf(planeSphereSqDist)/sqrtf(planeSqLength);

	if(distance>sphere.radius)
		return 0;

	const float projectionFactor=-planeSphereSqDist/planeSqLength;

	const vec3 projection=Vec3_Addv(sphere.position, Vec3_Muls(planeVec3, projectionFactor));

	const float distanceToIntersection=sqrtf(sphere.radius*sphere.radius-distance*distance);

	if(intersectionA)
		*intersectionA=Vec3_Addv(projection, Vec3_Muls(planeVec3, distanceToIntersection));
	
	if(intersectionB)
		*intersectionB=Vec3_Subv(projection, Vec3_Muls(planeVec3, distanceToIntersection));

	return (distance==sphere.radius)?1:2;
}

void ExplodeEmitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle)
{
	particle->position=Vec3b(0.0f);

	particle->velocity=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
	Vec3_Normalize(&particle->velocity);
	particle->velocity=Vec3_Muls(particle->velocity, RandFloat()*50.0f);

	particle->life=RandFloat()*0.5f+0.01f;
}

// Runs anything physics related
void Thread_Physics(void *arg)
{
	//CvWriteFlag(flagSeries, "physics start");

	if(!pausePhysics)
	{
		ParticleSystem_Step(&particleSystem, fTimeStep);

		// Run integration for particle emitter objects
		for(uint32_t i=0;i<MAX_EMITTERS;i++)
		{
			// If it's alive reduce the life and integrate,
			// otherwise if it's dead and still has an ID assigned, delete/unassign it.
			if(particleEmittersLife[i]>0.0f)
			{
				particleEmittersLife[i]-=fTimeStep;
				PhysicsIntegrate(&particleEmitters[i], fTimeStep);

				for(uint32_t j=0;j<List_GetCount(&particleSystem.emitters);j++)
				{
					ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&particleSystem.emitters, j);

					if(emitter->ID==particleEmittersID[i])
					{
						emitter->position=particleEmitters[i].position;

						if(particleEmittersLife[i]<5.0f)
							emitter->particleSize*=fmaxf(0.0f, fminf(1.0f, particleEmittersLife[i]/5.0f));

						break;
					}
				}
			}
			else if(particleEmittersID[i]!=UINT32_MAX)
			{
				ParticleSystem_DeleteEmitter(&particleSystem, particleEmittersID[i]);
				particleEmittersID[i]=UINT32_MAX;
			}
		}

		// Loop through objects, integrate and check/resolve collisions
		for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
		{
			// Run physics integration on the asteroids
			PhysicsIntegrate(&asteroids[i], fTimeStep);

			// Check asteroids against other asteroids
			for(uint32_t j=i+1;j<NUM_ASTEROIDS;j++)
			{
				const float mag=PhysicsSphereToSphereCollisionResponse(&asteroids[i], &asteroids[j]);

				if(mag>1.0f)
					Audio_PlaySample(&sounds[RandRange(SOUND_STONE1, SOUND_STONE3)], false, mag/50.0f, &asteroids[i].position);
			}

			// Check asteroids against the camera
			RigidBody_t cameraBody;

			cameraBody.position=camera.position;
			cameraBody.force=Vec3b(0.0f);

			const matrix cameraOrientation=
			{
				.x=Vec4(camera.right.x, camera.up.x, camera.forward.x, 0.0f),
				.y=Vec4(camera.right.y, camera.up.y, camera.forward.y, 0.0f),
				.z=Vec4(camera.right.z, camera.up.z, camera.forward.z, 0.0f),
				.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
			};
			cameraBody.velocity=Matrix3x3MultVec3(camera.velocity, MatrixTranspose(cameraOrientation));

			cameraBody.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
			cameraBody.angularVelocity=Vec3b(0.0f);

			cameraBody.radius=camera.radius;

			cameraBody.mass=(1.0f/3000.0f)*(1.33333333f*PI*cameraBody.radius);
			cameraBody.invMass=1.0f/cameraBody.mass;

			cameraBody.inertia=0.4f*cameraBody.mass*(cameraBody.radius*cameraBody.radius);
			cameraBody.invInertia=1.0f/cameraBody.inertia;

			const float mag=PhysicsSphereToSphereCollisionResponse(&cameraBody, &asteroids[i]);

			//camera.position=cameraBody.position;
			//camera.velocity=cameraBody.velocity;

			if(mag>1.0f)
				Audio_PlaySample(&sounds[SOUND_CRASH], false, mag/50.0f, &camera.position);

#if 0
			for(uint32_t j=0;j<connectedClients;j++)
			{
				// Don't check for collision with our own net camera
				if(j!=clientID)
					PhysicsCameraToSphereCollisionResponse(&netCameras[j], &asteroids[i]);
			}
#endif

			// Check asteroids against projectile emitters
			for(uint32_t j=0;j<MAX_EMITTERS;j++)
			{
				if(particleEmittersLife[j]>0.0f)
				{
					if(PhysicsSphereToSphereCollisionResponse(&particleEmitters[j], &asteroids[i])>1.0f)
					{
						// It collided, kill it.
						// Setting this directly to <0.0 seems to cause emitters that won't get removed,
						//     so setting it to nearly 0.0 allows the natural progression kill it off.
						particleEmittersLife[j]=0.001f;

						Audio_PlaySample(&sounds[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)], false, 1.0f, &particleEmitters[j].position);

						ParticleSystem_AddEmitter(&particleSystem,
												  particleEmitters[j].position,	// Position
												  Vec3(100.0f, 12.0f, 5.0f),		// Start color
												  Vec3(0.0f, 0.0f, 0.0f),			// End color
												  5.0f,							// Radius of particles
												  1000,							// Number of particles in system
												  PARTICLE_EMITTER_ONCE,			// Type?
												  ExplodeEmitterCallback			// Callback for particle generation
						);

						// Silly radius reduction on hit
						//body->radius=fmaxf(body->radius-10.0f, 0.0f);
					}
				}
			}

#if 0
			if(isTargeted)
			{
				float distance=raySphereIntersect(enemy.position, enemy.forward, asteroids[i].position, asteroids[i].radius);

				if(distance>0.0f)
				{
					RigidBody_t particleBody;

					particleBody.position=Vec3_Addv(enemy.position, Vec3_Muls(enemy.forward, distance));
					particleBody.velocity=Vec3_Muls(enemy.forward, 300.0f);
					particleBody.force=Vec3b(0.0f);

					particleBody.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
					particleBody.angularVelocity=Vec3b(0.0f);

					particleBody.radius=2.0f;

					particleBody.mass=(1.0f/3000.0f)*(1.33333333f*PI*particleBody.radius)*10000.0f;
					particleBody.invMass=1.0f/particleBody.mass;

					particleBody.inertia=0.4f*particleBody.mass*(particleBody.radius*particleBody.radius);
					particleBody.invInertia=1.0f/particleBody.inertia;

					if(PhysicsSphereToSphereCollisionResponse(&particleBody, &asteroids[i])>1.0f)
					{
						Audio_PlaySample(&sounds[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)], false, 1.0f, &particleBody.position);

						// FIXME: Is this causing derelict emitters that never go away?
						//			I don't think it is, but need to check.
						ParticleSystem_AddEmitter
						(
							&particleSystem,
							particleBody.position,		// Position
							Vec3(100.0f, 12.0f, 5.0f),	// Start color
							Vec3(0.0f, 0.0f, 0.0f),		// End color
							5.0f,						// Radius of particles
							1000,						// Number of particles in system
							PARTICLE_EMITTER_ONCE,		// Type?
							ExplodeEmitterCallback		// Callback for particle generation
						);

						// Silly radius reduction on hit
						//body->radius=fmaxf(body->radius-10.0f, 0.0f);
					}
				}
			}
#endif

#if 1
			if(isControlPressed)
			{
				float distance=raySphereIntersect(camera.position, camera.forward, asteroids[i].position, asteroids[i].radius);

				if(distance>0.0f)
				{
					RigidBody_t particleBody;

					particleBody.position=Vec3_Addv(camera.position, Vec3_Muls(camera.forward, distance));
					particleBody.velocity=Vec3_Muls(camera.forward, 300.0f);
					particleBody.force=Vec3b(0.0f);

					particleBody.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
					particleBody.angularVelocity=Vec3b(0.0f);

					particleBody.radius=2.0f;

					particleBody.mass=(1.0f/3000.0f)*(1.33333333f*PI*particleBody.radius)*10000.0f;
					particleBody.invMass=1.0f/particleBody.mass;

					particleBody.inertia=0.4f*particleBody.mass*(particleBody.radius*particleBody.radius);
					particleBody.invInertia=1.0f/particleBody.inertia;

					if(PhysicsSphereToSphereCollisionResponse(&particleBody, &asteroids[i])>1.0f)
					{
						Audio_PlaySample(&sounds[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)], false, 1.0f, &particleBody.position);

						// FIXME: Is this causing derelict emitters that never go away?
						//			I don't think it is, but need to check.
						ParticleSystem_AddEmitter
						(
							&particleSystem,
							particleBody.position,		// Position
							Vec3(100.0f, 12.0f, 5.0f),	// Start color
							Vec3(0.0f, 0.0f, 0.0f),		// End color
							5.0f,						// Radius of particles
							1000,						// Number of particles in system
							PARTICLE_EMITTER_ONCE,		// Type?
							ExplodeEmitterCallback		// Callback for particle generation
						);

						// Silly radius reduction on hit
						//body->radius=fmaxf(body->radius-10.0f, 0.0f);
					}
				}
			}
#endif
		}

#if 0
		for(uint32_t i=0;i<connectedClients;i++)
		{
			// Don't check for collision with our own net camera
			if(i!=clientID)
				PhysicsCameraToCameraCollisionResponse(&camera, &netCameras[i]);
		}
		//////
#endif

		// Update instance matrix data
		for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
		{
			float radiusScale=0.666667f;

			matrix local=MatrixScale(asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale);
			local=MatrixMult(local, QuatToMatrix(asteroids[i].orientation));
			asteroidInstancePtr[i]=MatrixMult(local, MatrixTranslatev(asteroids[i].position));
		}
		//////

		//ClientNetwork_SendStatus();
	}

	// Update camera and modelview matrix
	modelView=CameraUpdate(&camera, fTimeStep);
	CameraUpdate(&enemy, fTimeStep);
	//////

	//CvWriteFlag(flagSeries, "physics end");

	// Barrier now that we're done here
	ThreadBarrier_Wait(&physicsThreadBarrier);
}

extern vec2 mousePosition;

void FireParticleEmitter(vec3 position, vec3 direction);

bool leftTriggerOnce=true;
bool rightTriggerOnce=true;

static vec3 lastLeftPosition={ 0.0f, 0.0f, 0.0f };

// Render call from system main event loop
void Render(void)
{
	static uint32_t index=0, lastImageIndex=0, imageIndex=2;

	//CvWriteFlag(flagSeries, "frame start");

	Thread_AddJob(&threadPhysics, Thread_Physics, NULL);
	//Thread_Physics(NULL);

	vkWaitForFences(vkContext.device, 1, &perFrame[index].frameFence, VK_TRUE, UINT64_MAX);

	// Handle VR frame start
	if(isVR)
	{
		if(!VR_StartFrame(&xrContext))
			return;

		projection[0]=VR_GetEyeProjection(&xrContext, 0);
		projection[1]=VR_GetEyeProjection(&xrContext, 1);

		headPose=VR_GetHeadPose(&xrContext);

		leftHand=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.leftHandSpace, 0);
		leftTrigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 0);
		leftGrip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 0);
		leftThumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 0);

		rightHand=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.rightHandSpace, 1);
		rightTrigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 1);
		rightGrip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 1);
		rightThumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 1);

		const float speed=400.0f;
		const float rotation=0.1f;

		camera.velocity.x-=leftThumbstick.x*speed*fTimeStep;
		camera.velocity.z+=leftThumbstick.y*speed*fTimeStep;
		camera.yaw-=rightThumbstick.x*rotation*fTimeStep;
		camera.pitch+=rightThumbstick.y*rotation*fTimeStep;

		if(leftTrigger>0.1f)
		{
			vec3 deltaLeftPosition=Vec3_Muls(Vec3_Subv(Vec3(leftHand.position.x, leftHand.position.y, leftHand.position.z), lastLeftPosition), 100.0f);
			MouseEvent_t MouseEvent={ .dx=(int32_t)deltaLeftPosition.x, .dy=(int32_t)deltaLeftPosition.y };

			Event_Trigger(EVENT_MOUSEMOVE, &MouseEvent);

			if(leftTrigger>0.75f&&leftTriggerOnce)
			{
				leftTriggerOnce=false;
				MouseEvent.button|=MOUSE_BUTTON_LEFT;
				Event_Trigger(EVENT_MOUSEDOWN, &MouseEvent);
			}
			else
				MouseEvent.button&=~MOUSE_BUTTON_LEFT;


			if(leftTrigger<0.25f&&!leftTriggerOnce)
				leftTriggerOnce=true;
		}
		else
			lastLeftPosition=Vec3(leftHand.position.x, leftHand.position.y, leftHand.position.z);

		if(rightTrigger>0.75f&&rightTriggerOnce)
		{
			rightTriggerOnce=false;

			vec4 rightOrientation=Vec4(rightHand.orientation.x, rightHand.orientation.y, rightHand.orientation.z, rightHand.orientation.w);
			vec3 direction=Matrix3x3MultVec3(Vec3(0.0f, 0.0f, -1.0f), MatrixMult(QuatToMatrix(rightOrientation), MatrixInverse(modelView)));

			FireParticleEmitter(Vec3_Addv(camera.position, Vec3_Muls(direction, camera.radius)), direction);
			Audio_PlaySample(&sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &camera.position);
		}

		if(rightTrigger<0.25f&&!rightTriggerOnce)
			rightTriggerOnce=true;
	}
	else
	// Handle non-VR frame start
	{
		lastImageIndex=imageIndex;
		VkResult Result=vkAcquireNextImageKHR(vkContext.device, swapchain.swapchain, UINT64_MAX, perFrame[index].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

		if(imageIndex!=((lastImageIndex+1)%swapchain.numImages))
			DBGPRINTF(DEBUG_ERROR, "FRAME OUT OF ORDER: last Index=%d Index=%d\n", lastImageIndex, imageIndex);

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date... Rebuilding.\n");
			RecreateSwapchain();
			return;
		}

		projection[0]=MatrixInfPerspective(90.0f, (float)renderWidth/renderHeight, 0.01f);
		headPose=MatrixIdentity();
	}

	//////
	// Do a simple dumb "seek out the player" thing
	//CameraSeekTarget(&enemy, camera.position, camera.radius, asteroids, NUM_ASTEROIDS);
	CameraSeekTargetCamera(&enemy, camera, asteroids, NUM_ASTEROIDS);
	isTargeted=CameraIsTargetInFOV(enemy, camera.position, deg2rad(15.0f));

#if 0
	// Test for if the player is in a 10 degree view cone, if so fire at them once every 2 seconds.
	static float fireTime=0.0f;
	fireTime+=fTimeStep;

	if(isTargeted&&fireTime>2.0f)
	{
		fireTime=0.0f;

		Audio_PlaySample(&sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &enemy.position);
		FireParticleEmitter(Vec3_Addv(enemy.position, Vec3_Muls(enemy.forward, enemy.radius)), enemy.forward);
	}
	//////
#endif

	Console_Draw(&console);

	Audio_SetStreamVolume(0, UI_GetBarGraphValue(&UI, volumeID));

	Font_Print(&Fnt, 16.0f, renderWidth-400.0f, renderHeight-50.0f-16.0f, "Current track: %s", musicList[currentMusic].string);

	// Reset the frame fence and command pool (and thus the command buffer)
	vkResetFences(vkContext.device, 1, &perFrame[index].frameFence);
	vkResetDescriptorPool(vkContext.device, perFrame[index].descriptorPool, 0);
	vkResetCommandPool(vkContext.device, perFrame[index].commandPool, 0);

	// Start recording the commands
	vkBeginCommandBuffer(perFrame[index].commandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	// Update shadow depth map
	ShadowUpdateMap(perFrame[index].commandBuffer, index);

	vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[0].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	EyeRender(index, 0, headPose);

	if(isVR)
	{
		vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[1].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		EyeRender(index, 1, headPose);
	}

	// Final drawing compositing
	CompositeDraw(index, 0);
	//////

	// Other eye compositing
	if(isVR)
		CompositeDraw(index, 1);

	// Reset the font text collection for the next frame
	Font_Reset(&Fnt);

	vkEndCommandBuffer(perFrame[index].commandBuffer);

	// Wait for physics to finish before submitting frame
	ThreadBarrier_Wait(&physicsThreadBarrier);

	// Submit command queue
	VkSubmitInfo SubmitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&perFrame[index].presentCompleteSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&perFrame[index].renderCompleteSemaphore,
		.commandBufferCount=1,
		.pCommandBuffers=&perFrame[index].commandBuffer,
	};

	if(isVR)
	{
		SubmitInfo.waitSemaphoreCount=0;
		SubmitInfo.pWaitSemaphores=VK_NULL_HANDLE;
		SubmitInfo.signalSemaphoreCount=0;
		SubmitInfo.pSignalSemaphores=VK_NULL_HANDLE;
	}

	vkQueueSubmit(vkContext.graphicsQueue, 1, &SubmitInfo, perFrame[index].frameFence);

	// Handle VR frame end
	if(isVR)
	{
		VR_EndFrame(&xrContext);
		index=(index+1)%xrContext.swapchain[0].numImages;
	}
	else
	// Handle non-VR frame end
	{
		// And present it to the screen
		VkResult Result=vkQueuePresentKHR(vkContext.graphicsQueue, &(VkPresentInfoKHR)
		{
			.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount=1,
			.pWaitSemaphores=&perFrame[index].renderCompleteSemaphore,
			.swapchainCount=1,
			.pSwapchains=&swapchain.swapchain,
			.pImageIndices=&imageIndex,
		});

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date... Rebuilding.\n");
			RecreateSwapchain();
			return;
		}

		index=(index+1)%swapchain.numImages;
	}
}

void Console_CmdQuit(Console_t *Console, const char *Param)
{
	isDone=true;
}

bool vkuMemAllocator_Init(VkuContext_t *context);

// Initialization call from system main
bool Init(void)
{
	RandomSeed(42069);

	//CvInitProvider(&CvDefaultProviderGuid, &provider);
	//CvCreateMarkerSeries(provider, "flag series", &flagSeries);

	// TODO: This is a hack, fix it proper.
	if(isVR)
		swapchain.numImages=xrContext.swapchain[0].numImages;

	Console_Init(&console, 80, 25);
	Console_AddCommand(&console, "quit", Console_CmdQuit);

	vkuMemAllocator_Init(&vkContext);

	CameraInit(&camera, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
	CameraInit(&enemy, Vec3(0.0f, 0.0f, 1200.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));

	if(!Audio_Init())
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Audio_Init failed.\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew1.wav", &sounds[SOUND_PEW1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew2.wav", &sounds[SOUND_PEW2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew3.wav", &sounds[SOUND_PEW3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew3.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone1.wav", &sounds[SOUND_STONE1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone2.wav", &sounds[SOUND_STONE2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone3.wav", &sounds[SOUND_STONE3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone3.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/crash.wav", &sounds[SOUND_CRASH]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/crash.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode1.wav", &sounds[SOUND_EXPLODE1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode2.wav", &sounds[SOUND_EXPLODE2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode3.wav", &sounds[SOUND_EXPLODE3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode3.wav\n");
		return false;
	}

	SFX_Init();
	Music_Init();

	// Load models
	if(LoadBModel(&models[MODEL_ASTEROID1], "assets/asteroid1.bmodel"))
		BuildMemoryBuffersBModel(&vkContext, &models[MODEL_ASTEROID1]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid1.bmodel\n");
		return false;
	}

	if(LoadBModel(&models[MODEL_ASTEROID2], "assets/asteroid2.bmodel"))
		BuildMemoryBuffersBModel(&vkContext, &models[MODEL_ASTEROID2]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid2.bmodel\n");
		return false;
	}

	if(LoadBModel(&models[MODEL_ASTEROID3], "assets/asteroid3.bmodel"))
		BuildMemoryBuffersBModel(&vkContext, &models[MODEL_ASTEROID3]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid3.bmodel\n");
		return false;
	}

	if(LoadBModel(&models[MODEL_ASTEROID4], "assets/asteroid4.bmodel"))
		BuildMemoryBuffersBModel(&vkContext, &models[MODEL_ASTEROID4]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid4.bmodel\n");
		return false;
	}

	uint32_t TriangleCount=0;

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		for(uint32_t j=0;j<models[i].numMesh;j++)
			TriangleCount+=models[i].mesh[j].numFace;
	}

	TriangleCount*=NUM_ASTEROIDS;

	DBGPRINTF(DEBUG_ERROR, "\nNot an error, just a total triangle count: %d\n\n", TriangleCount);

	// Load textures
	
	// TODO: For some reason on Linux/Android, the first loaded QOI here has corruption and I'm not sure why.
	//			If I upload a temp image and delete it afterwards, it's all good.
	VkuImage_t temp;
	Image_Upload(&vkContext, &temp, "assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);

	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID1], "assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID1_NORMAL], "assets/asteroid1_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID2], "assets/asteroid2.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID2_NORMAL], "assets/asteroid2_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID3], "assets/asteroid3.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID3_NORMAL], "assets/asteroid3_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID4], "assets/asteroid4.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&vkContext, &textures[TEXTURE_ASTEROID4_NORMAL], "assets/asteroid4_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&vkContext, &textures[TEXTURE_CROSSHAIR], "assets/crosshair.qoi", IMAGE_NONE);

	vkuDestroyImageBuffer(&vkContext, &temp);

	GenNebulaVolume(&textures[TEXTURE_VOLUME]);

	// Create primary pipeline
	CreateLightingPipeline();

	// Create skybox pipeline
	CreateSkyboxPipeline();
	GenerateSkyParams();

	CreateSpherePipeline();
	CreateLinePipeline();

	// Create volumetric rendering pipeline
	CreateVolumePipeline();

	// Create shadow map pipeline
	CreateShadowPipeline();
	CreateShadowMap();

	// Create compositing pipeline
	CreateCompositePipeline();

	// Create primary frame buffers, depth image
	CreateFramebuffers(0);
	CreateCompositeFramebuffers(0);

	// Second eye framebuffer for VR
	if(isVR)
	{
		CreateFramebuffers(1);
		CreateCompositeFramebuffers(1);
	}

	// Set up particle system
	if(!ParticleSystem_Init(&particleSystem))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: ParticleSystem_Init failed.\n");
		return false;
	}
	
	ParticleSystem_SetGravity(&particleSystem, 0.0f, 0.0f, 0.0f);

	ParticleSystem_AddEmitter(&particleSystem, Vec3b(0.0f), Vec3b(0.0f), Vec3b(0.0f), 0.0f, 1, PARTICLE_EMITTER_BURST, NULL);

	// Set up emitter initial state and projectial rigid body parameters
	for(uint32_t i=0;i<MAX_EMITTERS;i++)
	{
		particleEmittersID[i]=UINT32_MAX;	// No assigned particle ID
		particleEmittersLife[i]=-1.0f;		// No life

		RigidBody_t particleBody;

		particleBody.position=Vec3b(0.0f);
		particleBody.velocity=Vec3b(0.0f);
		particleBody.force=Vec3b(0.0f);

		particleBody.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		particleBody.angularVelocity=Vec3b(0.0f);

		particleBody.radius=2.0f;

		particleBody.mass=(1.0f/3000.0f)*(1.33333333f*PI*particleBody.radius)*10.0f;
		particleBody.invMass=1.0f/particleBody.mass;

		particleBody.inertia=0.4f*particleBody.mass*(particleBody.radius*particleBody.radius);
		particleBody.invInertia=1.0f/particleBody.inertia;

		particleEmitters[i]=particleBody;
	}

	Font_Init(&Fnt);

	UI_Init(&UI, Vec2(0.0f, 0.0f), Vec2((float)renderWidth, (float)renderHeight));

#ifdef ANDROID
	UI_AddButton(&UI, Vec2(0.0f, renderHeight-50.0f), Vec2(100.0f, 50.0f), Vec3(0.25f, 0.25f, 0.25f), "Random", (UIControlCallback)GenerateSkyParams);
#endif

	UI_AddButton(&UI,
				 Vec2(renderWidth-400.0f, renderHeight-50.0f),	// Position
				 Vec2(100.0f, 50.0f),					// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 "Play",								// Title text
				 StartStreamCallback);					// Callback
	UI_AddButton(&UI,
				 Vec2(renderWidth-300.0f, renderHeight-50.0f),	// Position
				 Vec2(100.0f, 50.0f),					// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 "Pause",								// Title text
				 StopStreamCallback);					// Callback
	UI_AddButton(&UI,
				 Vec2(renderWidth-200.0f, renderHeight-50.0f),	// Position
				 Vec2(100.0f, 50.0f),					// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 "Prev",								// Title text
				 PrevTrackCallback);					// Callback
	UI_AddButton(&UI,
				 Vec2(renderWidth-100.0f, renderHeight-50.0f),	// Position
				 Vec2(100.0f, 50.0f),					// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 "Next",								// Title text
				 NextTrackCallback);					// Callback
	volumeID=UI_AddBarGraph(&UI,
							Vec2(renderWidth-400.0f, renderHeight-50.0f-16.0f-30.0f),// Position
							Vec2(400.0f, 30.0f),		// Size
							Vec3(0.25f, 0.25f, 0.25f),	// Color
							"Volume",					// Title text
							false,						// Read-only
							0.0f, 1.0f, 0.125f);		// min/max/initial value

	colorShiftID=UI_AddBarGraph(&UI,
							Vec2(renderWidth-400.0f, renderHeight-50.0f-16.0f-30.0f-50.0f),// Position
							Vec2(400.0f, 30.0f),		// Size
							Vec3(0.25f, 0.25f, 0.25f),	// Color
							"Cloud Color Shift",		// Title text
							false,						// Read-only
							0.0f, 1.0f, 0.45f);			// min/max/initial value

	UI_AddSprite(&UI, Vec2((float)renderWidth/2.0f, (float)renderHeight/2.0f), Vec2(50.0f, 50.0f), Vec3(1.0f, 1.0f, 1.0f), &textures[TEXTURE_CROSSHAIR], 0.0f);

	cursorID=UI_AddCursor(&UI, Vec2(0.0f, 0.0f), 16.0f, Vec3(1.0f, 1.0f, 1.0f));

	// Other per-frame data
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		// Create needed fence and semaphores for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].presentCompleteSemaphore);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].renderCompleteSemaphore);

		// Per-frame descriptor pool for main thread
		vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
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
		}, VK_NULL_HANDLE, &perFrame[i].descriptorPool);

		// Create per-frame command pools
		vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags=0,
			.queueFamilyIndex=vkContext.graphicsQueueIndex,
		}, VK_NULL_HANDLE, &perFrame[i].commandPool);

		// Allocate the command buffers we will be rendering into
		vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=perFrame[i].commandPool,
			.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount=1,
		}, &perFrame[i].commandBuffer);

		vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=perFrame[i].commandPool,
			.level=VK_COMMAND_BUFFER_LEVEL_SECONDARY,
			.commandBufferCount=2,
		}, perFrame[i].secCommandBuffer);
	}

	// Set up and initialize threads
	for(uint32_t i=0;i<NUM_THREADS;i++)
	{
		Thread_Init(&thread[i]);
		Thread_AddConstructor(&thread[i], Thread_Constructor, (void *)&threadData[i]);
		Thread_AddDestructor(&thread[i], Thread_Destructor, (void *)&threadData[i]);
		Thread_Start(&thread[i]);
	}

	// Synchronization barrier, count is number of threads+main thread
	ThreadBarrier_Init(&threadBarrier, NUM_THREADS+1);

	// Thread for physics, and sync barrier
	Thread_Init(&threadPhysics);
	Thread_Start(&threadPhysics);

	ThreadBarrier_Init(&physicsThreadBarrier, 2);

	//ClientNetwork_Init();

	if(!Zone_VerifyHeap(zone))
		exit(-1);

	DBGPRINTF(DEBUG_WARNING, "\nCurrent vulkan zone memory allocations:\n");
	vkuMemAllocator_Print();

	return true;
}

bool CreateFramebuffers(uint32_t eye)
{
	VkImageFormatProperties imageFormatProps;
	VkResult result;

	depthFormat=VK_FORMAT_D32_SFLOAT;
	result=vkGetPhysicalDeviceImageFormatProperties(vkContext.physicalDevice,
													depthFormat,
													VK_IMAGE_TYPE_2D,
													VK_IMAGE_TILING_OPTIMAL,
													VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
													0,
													&imageFormatProps);

	if(result!=VK_SUCCESS)
	{
		depthFormat=VK_FORMAT_D24_UNORM_S8_UINT;
		result=vkGetPhysicalDeviceImageFormatProperties(vkContext.physicalDevice, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &imageFormatProps);

		if(result!=VK_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "CreateFramebuffers: No suitable depth format found.\n");
			return false;
		}
	}

	vkuCreateTexture2D(&vkContext, &colorImage[eye], renderWidth, renderHeight, colorFormat, MSAA);
	vkuCreateTexture2D(&vkContext, &depthImage[eye], renderWidth, renderHeight, depthFormat, MSAA);
	vkuCreateTexture2D(&vkContext, &colorResolve[eye], renderWidth, renderHeight, colorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, colorImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(commandBuffer, depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, colorResolve[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=renderPass,
		.attachmentCount=3,
		.pAttachments=(VkImageView[]){ colorImage[eye].imageView, depthImage[eye].imageView, colorResolve[eye].imageView },
		.width=renderWidth,
		.height=renderHeight,
		.layers=1,
	}, 0, &framebuffer[eye]);

	return true;
}

// Rebuild Vulkan swapchain and related data
void RecreateSwapchain(void)
{
	cnd_broadcast(&physicsThreadBarrier.cond);

	// Wait for the device to complete any pending work
	vkDeviceWaitIdle(vkContext.device);

	// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
	// This is basically just the swapchain, framebuffers, and depth buffer.

	// swapchain, framebuffer, and depth buffer destruction
	vkuDestroyImageBuffer(&vkContext, &colorImage[0]);
	vkuDestroyImageBuffer(&vkContext, &colorResolve[0]);
	vkuDestroyImageBuffer(&vkContext, &colorBlur[0]);
	vkuDestroyImageBuffer(&vkContext, &colorTemp[0]);
	vkuDestroyImageBuffer(&vkContext, &depthImage[0]);

	vkDestroyFramebuffer(vkContext.device, framebuffer[0], VK_NULL_HANDLE);

	if(isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorImage[1]);
		vkuDestroyImageBuffer(&vkContext, &colorResolve[1]);
		vkuDestroyImageBuffer(&vkContext, &colorBlur[1]);
		vkuDestroyImageBuffer(&vkContext, &colorTemp[1]);
		vkuDestroyImageBuffer(&vkContext, &depthImage[1]);

		vkDestroyFramebuffer(vkContext.device, framebuffer[1], VK_NULL_HANDLE);
	}

	// Recreate the swapchain, vkuCreateSwapchain will see that there is an existing swapchain and deal accordingly.
	vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE);

	renderWidth=max(2, swapchain.extent.width);
	renderHeight=max(2, swapchain.extent.height);

	UI.size.x=(float)renderWidth;
	UI.size.y=(float)renderHeight;

	// Recreate the framebuffer
	CreateFramebuffers(0);
	CreateCompositeFramebuffers(0);

	if(isVR)
	{
		CreateFramebuffers(1);
		CreateCompositeFramebuffers(1);
	}
}

// Destroy call from system main
void Destroy(void)
{
	vkDeviceWaitIdle(vkContext.device);

	//ClientNetwork_Destroy();

	Audio_Destroy();
	SFX_Destroy();
	Music_Destroy();

	Zone_Free(zone, sounds[SOUND_PEW1].data);
	Zone_Free(zone, sounds[SOUND_PEW2].data);
	Zone_Free(zone, sounds[SOUND_PEW3].data);
	Zone_Free(zone, sounds[SOUND_STONE1].data);
	Zone_Free(zone, sounds[SOUND_STONE2].data);
	Zone_Free(zone, sounds[SOUND_STONE3].data);
	Zone_Free(zone, sounds[SOUND_CRASH].data);
	Zone_Free(zone, sounds[SOUND_EXPLODE1].data);
	Zone_Free(zone, sounds[SOUND_EXPLODE2].data);
	Zone_Free(zone, sounds[SOUND_EXPLODE3].data);

	if(isVR)
		VR_Destroy(&xrContext);

	for(uint32_t i=0;i<NUM_THREADS;i++)
		Thread_Destroy(&thread[i]);

	Thread_Destroy(&threadPhysics);

	if(vkContext.pipelineCache)
	{
		DBGPRINTF(DEBUG_INFO, "\nWriting pipeline cache to disk...\n");

		size_t PipelineCacheSize=0;
		vkGetPipelineCacheData(vkContext.device, vkContext.pipelineCache, &PipelineCacheSize, VK_NULL_HANDLE);

		uint8_t *PipelineCacheData=(uint8_t *)Zone_Malloc(zone, PipelineCacheSize);

		if(PipelineCacheData)
		{
			FILE *Stream=fopen("pipelinecache.bin", "wb");

			if(Stream)
			{
				vkGetPipelineCacheData(vkContext.device, vkContext.pipelineCache, &PipelineCacheSize, PipelineCacheData);
				fwrite(PipelineCacheData, 1, PipelineCacheSize, Stream);
				fclose(Stream);
				Zone_Free(zone, PipelineCacheData);
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
	ParticleSystem_Destroy(&particleSystem);
	//////////

	// Skybox destruction
	DestroySkybox();
	//////////

	// Lighting pipeline destruction
	DestroyLighting();
	//////////

	// Debug object pipeline destruction
	DestroyLine();
	DestroySphere();
	//////////

	// UI destruction
	UI_Destroy(&UI);
	//////////

	// Font destruction
	Font_Destroy(&Fnt);
	//////////

	// Asteroid instance buffer destruction
	vkuDestroyBuffer(&vkContext, &asteroidInstance);
	//////////

	// Textures destruction
	for(uint32_t i=0;i<NUM_TEXTURES;i++)
		vkuDestroyImageBuffer(&vkContext, &textures[i]);
	//////////

	// 3D Model destruction
	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDestroyBuffer(&vkContext, &models[i].vertexBuffer);

		for(uint32_t j=0;j<models[i].numMesh;j++)
			vkuDestroyBuffer(&vkContext, &models[i].mesh[j].indexBuffer);

		FreeBModel(&models[i]);
	}
	//////////

	// Shadow map destruction
	DestroyShadow();
	//////////

	// Volume rendering
	DestroyVolume();
	//////////

	vkDestroyRenderPass(vkContext.device, renderPass, VK_NULL_HANDLE);

	// swapchain, framebuffer, and depth buffer destruction
	vkuDestroyImageBuffer(&vkContext, &colorImage[0]);
	vkuDestroyImageBuffer(&vkContext, &colorResolve[0]);
	vkuDestroyImageBuffer(&vkContext, &colorBlur[0]);
	vkuDestroyImageBuffer(&vkContext, &colorTemp[0]);
	vkuDestroyImageBuffer(&vkContext, &depthImage[0]);

	vkDestroyFramebuffer(vkContext.device, framebuffer[0], VK_NULL_HANDLE);

	if(isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorImage[1]);
		vkuDestroyImageBuffer(&vkContext, &colorResolve[1]);
		vkuDestroyImageBuffer(&vkContext, &colorBlur[1]);
		vkuDestroyImageBuffer(&vkContext, &colorTemp[1]);
		vkuDestroyImageBuffer(&vkContext, &depthImage[1]);

		vkDestroyFramebuffer(vkContext.device, framebuffer[1], VK_NULL_HANDLE);
	}

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		// Destroy sync objects
		vkDestroyFence(vkContext.device, perFrame[i].frameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(vkContext.device, perFrame[i].presentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(vkContext.device, perFrame[i].renderCompleteSemaphore, VK_NULL_HANDLE);

		// Destroy main thread descriptor pools
		vkDestroyDescriptorPool(vkContext.device, perFrame[i].descriptorPool, VK_NULL_HANDLE);

		// Destroy command pools
		vkDestroyCommandPool(vkContext.device, perFrame[i].commandPool, VK_NULL_HANDLE);
	}

	vkuDestroySwapchain(&vkContext, &swapchain);
	//////////

	DBGPRINTF(DEBUG_INFO, "Remaining Vulkan memory blocks:\n");
	vkuMemAllocator_Print();
	vkuMemAllocator_Destroy();
}
