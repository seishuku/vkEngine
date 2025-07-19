#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stdatomic.h>
#include "audio/audio.h"
#include "audio/music.h"
#include "audio/sfx.h"
#include "camera/camera.h"
#include "console/console.h"
#include "font/font.h"
#include "image/image.h"
#include "math/math.h"
#include "model/bmodel.h"
#include "network/network.h"
#include "network/client_network.h"
#include "physics/particle.h"
#include "physics/physics.h"
#include "physics/physicslist.h"
#include "pipelines/composite.h"
#include "pipelines/lighting.h"
#include "pipelines/line.h"
#include "pipelines/linegraph.h"
#include "pipelines/shadow.h"
#include "pipelines/skybox.h"
#include "pipelines/sphere.h"
#include "pipelines/volume.h"
#include "system/system.h"
#include "system/threads.h"
#include "ui/ui.h"
#include "utils/event.h"
#include "utils/list.h"
#include "utils/spatialhash.h"
#include "vr/vr.h"
#include "vulkan/vulkan.h"
#include "assetmanager.h"
#include "enemy.h"
#include "loadingscreen.h"
#include "perframe.h"

extern bool isDone;

// VR context (if running in VR mode)
XruContext_t xrContext;

// Vulkan instance handle and context structs
VkInstance vkInstance;
VkuContext_t vkContext;

// Per-frame data
PerFrame_t perFrame[FRAMES_IN_FLIGHT];

// Camera data
Camera_t camera;
matrix modelView, projection[2], headPose;

// extern timing data from system main
extern float fps, fTimeStep, fTime, audioTime;

float physicsTime=0.0f;

// Main particle system struct
ParticleSystem_t particleSystem;

// Particle system emitters as rigid bodies with life and ID hashmaps
#define MAX_EMITTERS 1000
typedef struct
{
	RigidBody_t body;
	uint32_t ID;
	float life;
} PhyParticleEmitter_t;

PhyParticleEmitter_t emitters[MAX_EMITTERS]={ 0 };

uint32_t fighterTexture=0;

//#define NUM_CUBE 14
RigidBody_t cubeBody[NUM_CUBE];

LoadingScreen_t loadingScreen;

// Vulkan swapchain helper struct
VkuSwapchain_t swapchain;

// Color buffer left and right images
VkuImage_t colorImage[2];

// Depth buffer left and right images
VkuImage_t depthImage[2];

// Framebuffers, per-eye
VkFramebuffer framebuffer[2];

// Asteroid data
//#define NUM_ASTEROIDS 5000
RigidBody_t asteroids[NUM_ASTEROIDS];
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

#define NUM_THREADS 2
ThreadData_t threadData[NUM_THREADS];
ThreadWorker_t thread[NUM_THREADS], threadPhysics;

ThreadBarrier_t threadBarrier;
ThreadBarrier_t physicsThreadBarrier;
//////

// UI Stuff
Font_t font;
UI_t UI;

uint32_t volumeID=UINT32_MAX;
uint32_t cursorID=UINT32_MAX;
uint32_t colorShiftID=UINT32_MAX;
uint32_t consoleBackground=UINT32_MAX;
uint32_t currentTrack=UINT32_MAX;
uint32_t windowID=UINT32_MAX;
uint32_t editWindowID=UINT32_MAX;
uint32_t editControl=UINT32_MAX;
//////

Console_t console;

XrPosef leftHand, rightHand;
float leftTrigger, rightTrigger;
float leftGrip, rightGrip;
vec2 leftThumbstick, rightThumbstick;

bool isControlPressed=false;

bool pausePhysics=false;

extern vec2 lStick, rStick;
extern bool buttons[4];

#define NUM_ENEMY 1
Camera_t enemy[NUM_ENEMY];
Enemy_t enemyAI[NUM_ENEMY];

LineGraph_t frameTimes, audioTimes, physicsTimes;

void RecreateSwapchain(void);
bool CreateFramebuffers(uint32_t eye);

void ResetPhysicsCubes(void)
{
	const float radius=20.0f;
	const float mass=(1.0f/3000.0f)*(1.33333333f*PI*radius)*10.0f;
	const float inertia=0.4f*mass*(radius*radius);

	const uint32_t layers=4;
	const uint32_t cubesPerLayer=4;
	const float spacing=radius*1.01f;

	uint32_t index=0;
	for(uint32_t y=0;y<layers;y++)
	{
		for(uint32_t x=0;x<cubesPerLayer-y; x++)
		{
			for(uint32_t z=0;z<cubesPerLayer-y;z++)
			{
				if(index>=NUM_CUBE) break;

				float offsetX=(cubesPerLayer-y-1)*spacing*0.5f;
				float offsetZ=(cubesPerLayer-y-1)*spacing*0.5f;

				cubeBody[index++]=(RigidBody_t)
				{
					.position=Vec3(
						x*spacing-offsetX,
						(y*spacing)-(layers*radius/2),
						z*spacing-offsetZ
					),

					.velocity=Vec3b(0.0f),
					.force=Vec3b(0.0f),
					.mass=mass*10,
					.invMass=1.0f/mass,

					.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f),
					.angularVelocity=Vec3b(0.0f),
					.inertia=inertia,
					.invInertia=1.0f/inertia,

					.type=RIGIDBODY_OBB,
					.size=Vec3(radius/2, radius/2, radius/2),
				};
			}
		}
	}
}

// Build up random data for skybox and asteroid field
void GenerateWorld(void)
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
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		memcpy(perFrame[i].skyboxUBO[0], &Skybox_UBO, sizeof(Skybox_UBO_t));
		memcpy(perFrame[i].skyboxUBO[1], &Skybox_UBO, sizeof(Skybox_UBO_t));
	}

	// Set up rigid body reps for asteroids
	const float asteroidFieldMinRadius=50.0f;
	const float asteroidFieldMaxRadius=fminf((float)NUM_ASTEROIDS/2, 2000.0f);
	const float asteroidMinRadius=0.05f;
	const float asteroidMaxRadius=40.0f;

	uint32_t i=0, tries=0;

	memset(asteroids, 0, sizeof(RigidBody_t)*NUM_ASTEROIDS);

	// Randomly place asteroids in a sphere without any overlapping.
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
	if(!perFrame[0].asteroidInstance.buffer)
	{
		for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
		{
			vkuCreateHostBuffer(&vkContext, &perFrame[i].asteroidInstance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			perFrame[i].asteroidInstancePtr=(matrix *)perFrame[i].asteroidInstance.memory->mappedPointer;
		}
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

		asteroids[i].type=RIGIDBODY_SPHERE;
	}
	//////

	for(uint32_t i=0;i<NUM_ENEMY;i++)
	{
		vec3 randomDirection=Vec3(
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f)
		);
		Vec3_Normalize(&randomDirection);

		fighterTexture=RandRange(0, 7);

		if(!perFrame[0].fighterInstance.buffer)
		{
			for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
			{
				vkuCreateHostBuffer(&vkContext, &perFrame[i].fighterInstance, sizeof(matrix)*(2+MAX_CLIENTS), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
				perFrame[i].fighterInstancePtr=(matrix *)perFrame[i].fighterInstance.memory->mappedPointer;
			}
		}

		CameraInit(&enemy[i], Vec3_Muls(randomDirection, 1200.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
		InitEnemy(&enemyAI[i], &enemy[i], camera);
	}

	if(!perFrame[0].cubeInstance.buffer)
	{
		for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
		{
			vkuCreateHostBuffer(&vkContext, &perFrame[i].cubeInstance, sizeof(matrix)*NUM_CUBE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			perFrame[i].cubeInstancePtr=(matrix *)perFrame[i].cubeInstance.memory->mappedPointer;
		}
	}

	ResetPhysicsCubes();
}
//////

static void DrawCameraAxes(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, Camera_t camera)
{
	struct
	{
		matrix mvp;
		vec4 color, start, end;
	} linePC;

	vec4 position=Vec4_Vec3(camera.body.position, 1.0f);
	vec4 forward=Vec4_Vec3(camera.forward, 1.0f);
	vec4 up=Vec4_Vec3(camera.up, 1.0f);
	vec4 right=Vec4_Vec3(camera.right, 1.0f);

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
}

static void DrawPlayer(VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool, uint32_t index, uint32_t eye)
{
	//DrawCameraAxes(commandBuffer, index, eye, camera);
	for(uint32_t i=0;i<NUM_ENEMY;i++)
		DrawCameraAxes(commandBuffer, index, eye, enemy[i]);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipeline.pipeline);

	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[index].fighterInstance.buffer, &(VkDeviceSize) { 0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 0, assets[assetIndices[TEXTURE_FIGHTER1+fighterTexture]].image.sampler, assets[assetIndices[TEXTURE_FIGHTER1+fighterTexture]].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 1, assets[assetIndices[TEXTURE_FIGHTER1_NORMAL+fighterTexture]].image.sampler, assets[assetIndices[TEXTURE_FIGHTER1_NORMAL+fighterTexture]].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&mainPipeline.descriptorSet, descriptorPool);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipelineLayout, 0, 1, &mainPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &assets[assetIndices[MODEL_FIGHTER]].model.vertexBuffer.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<assets[assetIndices[MODEL_FIGHTER]].model.numMesh;i++)
	{
		vkCmdBindIndexBuffer(commandBuffer, assets[assetIndices[MODEL_FIGHTER]].model.mesh[i].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		if(clientSocket!=-1)
		{
			for(uint32_t j=0;j<connectedClients;j++)
			{
				if(j!=clientID)
					vkCmdDrawIndexed(commandBuffer, assets[assetIndices[MODEL_FIGHTER]].model.mesh[i].numFace*3, 1, 0, 0, j);
			}
		}
		else
			vkCmdDrawIndexed(commandBuffer, assets[assetIndices[MODEL_FIGHTER]].model.mesh[i].numFace*3, NUM_ENEMY, 0, 0, 1);
	}
}

// General thread constructor for threads using Vulkan
void Thread_Constructor(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	for(uint32_t Frame=0;Frame<FRAMES_IN_FLIGHT;Frame++)
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

	for(uint32_t Frame=0;Frame<FRAMES_IN_FLIGHT;Frame++)
	{
		for(uint32_t eye=0;eye<2;eye++)
		{
			vkDestroyCommandPool(vkContext.device, data->perFrame[Frame].commandPool[eye], VK_NULL_HANDLE);
			vkDestroyDescriptorPool(vkContext.device, data->perFrame[Frame].descriptorPool[eye], VK_NULL_HANDLE);
		}
	}
}

Spring_t testSpring={ 0 };

void Thread_Main(void *arg)
{
	ThreadData_t *data=(ThreadData_t *)arg;

	vkResetDescriptorPool(vkContext.device, data->perFrame[data->index].descriptorPool[data->eye], 0);
	vkResetCommandPool(vkContext.device, data->perFrame[data->index].commandPool[data->eye], 0);

	vkBeginCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=renderPass,
			.framebuffer=framebuffer[data->eye]
		}
	});

	vkCmdSetViewport(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)config.renderWidth, (float)config.renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth, config.renderHeight } });

	////// Skybox/skysphere
	DrawSkybox(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye, data->perFrame[data->index].descriptorPool[data->eye]);
	//////

	////// Asteroids
	DrawLighting(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye, data->perFrame[data->index].descriptorPool[data->eye]);
	//////

	// Draw sprung object
	{
		struct
		{
			matrix mvp;
			vec4 color;
		} spherePC;

		spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		matrix local=MatrixMult(MatrixScale(1.0f, 1.0f, 1.0f), MatrixTranslatev(testSpring.position));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->modelView);
		spherePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		DrawSpherePushConstant(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, sizeof(spherePC), &spherePC);
	}

#if 1
	if(isControlPressed)
	{
		for(uint32_t i=0;i<10;i++)
		{
			const float distance=10000000000.0f;
			const float val=0.005f;
			const vec3 randVec=Vec3(RandFloatRange(-val, val), RandFloatRange(-val, val), RandFloatRange(-val, val));

			DrawLine(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye,
						Vec3_Subv(camera.body.position, Vec3_Subv(camera.up, camera.right)),
						Vec3_Addv(camera.body.position, Vec3_Muls(Vec3_Addv(camera.forward, randVec), distance)),
						Vec4(1.0f+RandFloatRange(10.0f, 500.0f), 1.0f, 1.0f, 1.0f));

			DrawLine(data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye,
						Vec3_Subv(camera.body.position, Vec3_Addv(camera.up, camera.right)),
						Vec3_Addv(camera.body.position, Vec3_Muls(Vec3_Subv(camera.forward, randVec), distance)),
						Vec4(1.0f+RandFloatRange(10.0f, 500.0f), 1.0f, 1.0f, 1.0f));
		}
	}
#endif

	// Draw VR "hands"
	if(config.isVR)
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

	// Draw player models
	DrawPlayer(data->perFrame[data->index].secCommandBuffer[data->eye], data->perFrame[data->index].descriptorPool[data->eye], data->index, data->eye);

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
		.pInheritanceInfo=&(VkCommandBufferInheritanceInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
			.pNext=NULL,
			.renderPass=renderPass,
			.framebuffer=framebuffer[data->eye]
		}
	});

	vkCmdSetViewport(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)config.renderWidth, (float)config.renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth, config.renderHeight } });

	ParticleSystem_Draw(&particleSystem, data->perFrame[data->index].secCommandBuffer[data->eye], data->index, data->eye);

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

	vkuTransitionLayout(perFrame[index].commandBuffer, colorImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(perFrame[index].commandBuffer, depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=renderPass,
		.framebuffer=framebuffer[eye],
		.renderArea=(VkRect2D){ { 0, 0 }, { config.renderWidth, config.renderHeight } },
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}, {{{ 0.0f, 0 }}} },
	}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Set per thread data and add the job to that worker thread
	threadData[0].index=index;
	threadData[0].eye=eye;
	Thread_AddJob(&thread[0], Thread_Main, (void *)&threadData[0]);

	threadData[1].index=index;
	threadData[1].eye=eye;
	Thread_AddJob(&thread[1], Thread_Particles, (void *)&threadData[1]);

	ThreadBarrier_Wait(&threadBarrier);

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(perFrame[index].commandBuffer, 2, (VkCommandBuffer[])
	{
		threadData[0].perFrame[index].secCommandBuffer[eye],
		threadData[1].perFrame[index].secCommandBuffer[eye]
	});

	// TODO:
	//     Android has issues with depth readback and subsequent depth->eye/world transform, this also affects volumetrics in the compositing shader.
//#ifndef ANDROID
	vkCmdNextSubpass(perFrame[index].commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	//matrix Modelview=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	//ParticleSystem_Draw(&particleSystem, perFrame[index].commandBuffer, perFrame[index].descriptorPool, Modelview, perFrame[index].mainUBO[eye]->projection);
	
	//////// Volume cloud
	DrawVolume(perFrame[index].commandBuffer, index, eye, perFrame[index].descriptorPool);
	////////
//#endif

	vkCmdEndRenderPass(perFrame[index].commandBuffer);

	vkuTransitionLayout(perFrame[index].commandBuffer, colorImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(perFrame[index].commandBuffer, depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ExplodeEmitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle)
{
	particle->position=Vec3b(0.0f);

	particle->velocity=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
	Vec3_Normalize(&particle->velocity);
	particle->velocity=Vec3_Muls(particle->velocity, RandFloat()*50.0f);

	particle->life=RandFloat()*0.5f+0.01f;
}

void TestCollision(void *a, void *b)
{
	PhysicsObject_t *objA=(PhysicsObject_t *)a, *objB=(PhysicsObject_t *)b;

	if(PhysicsCollisionResponse(objA->rigidBody, objB->rigidBody)>1.0f)
	{
		// If both objects are asteroids
		if(objA->objectType==PHYSICSOBJECTTYPE_FIELD&&objB->objectType==PHYSICSOBJECTTYPE_FIELD)
		{
			Audio_PlaySample(&assets[assetIndices[RandRange(SOUND_STONE1, SOUND_STONE3)]].sound, false, 1.0f, objB->rigidBody->position);
		}
		// If one is an asteroid and one is a player
		else if((objA->objectType==PHYSICSOBJECTTYPE_FIELD&&objB->objectType==PHYSICSOBJECTTYPE_PLAYER)||
				(objA->objectType==PHYSICSOBJECTTYPE_PLAYER&&objB->objectType==PHYSICSOBJECTTYPE_FIELD))
		{
			Audio_PlaySample(&assets[assetIndices[SOUND_CRASH]].sound, false, 1.0f, objB->rigidBody->position);
		}
		// If both objects are players
		else if(objA->objectType==PHYSICSOBJECTTYPE_PLAYER&&objB->objectType==PHYSICSOBJECTTYPE_PLAYER)
		{
			Audio_PlaySample(&assets[assetIndices[SOUND_CRASH]].sound, false, 1.0f, objB->rigidBody->position);
		}
		// If it was a projectile colliding with anything
		else if(objA->objectType==PHYSICSOBJECTTYPE_PROJECTILE||objB->objectType==PHYSICSOBJECTTYPE_PROJECTILE)
		{
			PhysicsObject_t *whichProjectile=NULL;
			PhysicsObject_t *otherObject=NULL;

			if(objB->objectType==PHYSICSOBJECTTYPE_PROJECTILE)
			{
				whichProjectile=objB;
				otherObject=objA;
			}
			else if(objA->objectType==PHYSICSOBJECTTYPE_PROJECTILE)
			{
				whichProjectile=objA;
				otherObject=objB;
			}
			else
				return; // SHOULD NEVER BE HERE.

			if(otherObject->objectType==PHYSICSOBJECTTYPE_PLAYER)
			{
				for(uint32_t i=0;i<NUM_ENEMY;i++)
				{
					if(&enemyAI[i].camera->body==otherObject->rigidBody)
					{
						enemyAI[i].health-=10.0f;
						break;
					}
				}
			}

			// It collided, kill it.
			// Setting this directly to <0.0 seems to cause emitters that won't get removed,
			//     so setting it to nearly 0.0 allows the natural progression kill it off.
			// Need to find the source emitter first:
			for(uint32_t k=0;k<MAX_EMITTERS;k++)
			{
				if(emitters[k].life>0.0f)
				{
					if(whichProjectile->rigidBody==&emitters[k].body)
					{
						emitters[k].life=0.001f;
						break;
					}
				}
			}

			Audio_PlaySample(&assets[assetIndices[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)]].sound, false, 1.0f, whichProjectile->rigidBody->position);

			ParticleSystem_AddEmitter(&particleSystem,
									  whichProjectile->rigidBody->position,	// Position
									  Vec3(100.0f, 12.0f, 5.0f),	// Start color
									  Vec3(0.0f, 0.0f, 0.0f),		// End color
									  5.0f,						// Radius of particles
									  1000,						// Number of particles in system
									  PARTICLE_EMITTER_ONCE,		// Type?
									  ExplodeEmitterCallback		// Callback for particle generation
			);
		}
	}
}

SpatialHash_t spatialHash;

// Runs anything physics related
void Thread_Physics(void *arg)
{
	const uint32_t index=*((uint32_t *)arg);
	double startTime=GetClock();

	SpringIntegrate(&testSpring, camera.body.position, fTimeStep);

	// Set up physics objects to be processed
	ResetPhysicsObjectList();

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
		AddPhysicsObject(&asteroids[i], PHYSICSOBJECTTYPE_FIELD);

	AddPhysicsObject(&camera.body, PHYSICSOBJECTTYPE_PLAYER);

	for(uint32_t i=0;i<NUM_ENEMY;i++)
		AddPhysicsObject(&enemy[i].body, PHYSICSOBJECTTYPE_PLAYER);

	if(clientSocket!=-1)
	{
		for(uint32_t i=0;i<connectedClients;i++)
		{
			// Don't check for collision with our own net camera
			if(i!=clientID)
				AddPhysicsObject(&netCameras[i].body, PHYSICSOBJECTTYPE_PLAYER);
		}
	}

	for(uint32_t i=0;i<NUM_CUBE;i++)
		AddPhysicsObject(&cubeBody[i], PHYSICSOBJECTTYPE_FIELD);
	//////

	SpatialHash_Clear(&spatialHash);

	for(uint32_t i=0;i<numPhysicsObjects;i++)
		SpatialHash_AddObject(&spatialHash, physicsObjects[i].rigidBody->position, &physicsObjects[i]);

	if(!pausePhysics)
	{
		// Run particle system simlation
		ParticleSystem_Step(&particleSystem, fTimeStep);

		// Run lifetime check for particle emitter objects
		for(uint32_t i=0;i<MAX_EMITTERS;i++)
		{
			// If it's alive reduce the life and integrate,
			// otherwise if it's dead and still has an ID assigned, delete/unassign it.
			if(emitters[i].life>0.0f)
			{
				// Add particle emitter to physics list
				AddPhysicsObject(&emitters[i].body, PHYSICSOBJECTTYPE_PROJECTILE);

				emitters[i].life-=fTimeStep;

				for(uint32_t j=0;j<List_GetCount(&particleSystem.emitters);j++)
				{
					ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&particleSystem.emitters, j);

					if(emitter->ID==emitters[i].ID)
					{
						emitter->position=emitters[i].body.position;

						if(emitters[i].life<5.0f)
							emitter->particleSize*=fmaxf(0.0f, fminf(1.0f, emitters[i].life/5.0f));

						break;
					}
				}
			}
			else if(emitters[i].ID!=UINT32_MAX)
			{
				ParticleSystem_DeleteEmitter(&particleSystem, emitters[i].ID);
				emitters[i].ID=UINT32_MAX;
			}
		}

		// Run through the physics object list, run integration step and check for collisions against all other objects
		for(uint32_t i=0;i<numPhysicsObjects;i++)
		{
			PhysicsIntegrate(physicsObjects[i].rigidBody, fTimeStep);

			SpatialHash_TestObjects(&spatialHash, physicsObjects[i].rigidBody->position, &physicsObjects[i], TestCollision);

#if 1
			// Fire "laser beam"
			if(isControlPressed)
			{
				float distance=0.0f;

				if(physicsObjects[i].rigidBody->type==RIGIDBODY_SPHERE)
					distance=raySphereIntersect(camera.body.position, camera.forward, physicsObjects[i].rigidBody->position, physicsObjects[i].rigidBody->radius);
				else if(physicsObjects[i].rigidBody->type==RIGIDBODY_OBB)
					distance=rayOBBIntersect(camera.body.position, camera.forward, physicsObjects[i].rigidBody->position, physicsObjects[i].rigidBody->size, physicsObjects[i].rigidBody->orientation);

				if(distance>0.0f)
				{
					// This is a bit hacky, can't add particleBody as a physics object due to scope lifetime, so test in place.
					RigidBody_t particleBody;

					particleBody.position=Vec3_Addv(camera.body.position, Vec3_Muls(camera.forward, distance*0.99f));
					particleBody.velocity=Vec3_Muls(camera.forward, 300.0f);

					particleBody.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
					particleBody.angularVelocity=Vec3b(0.0f);

					particleBody.type=RIGIDBODY_SPHERE;
					particleBody.radius=2.0f;

					particleBody.mass=(1.0f/3000.0f)*(1.33333333f*PI*particleBody.radius)*1000.0f;
					particleBody.invMass=1.0f/particleBody.mass;

					particleBody.inertia=0.4f*particleBody.mass*(particleBody.radius*particleBody.radius);
					particleBody.invInertia=1.0f/particleBody.inertia;

					if(PhysicsCollisionResponse(&particleBody, physicsObjects[i].rigidBody)>1.0f)
					{
						Audio_PlaySample(&assets[assetIndices[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)]].sound, false, 1.0f, particleBody.position);

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
					}
				}
			}
#endif
		}
	}

	// Update enemy player model instance data
	if(clientSocket==-1)
	{
		for(uint32_t i=0;i<NUM_ENEMY;i++)
		{
			//UpdateEnemy(&enemyAI[i], camera);

			const float scale=(1.0f/assets[assetIndices[MODEL_FIGHTER]].model.radius)*enemy[i].body.radius;
			matrix local=MatrixScale(scale, scale, scale);
			local=MatrixMult(local, MatrixRotate(PI/2.0f, 0.0f, 1.0f, 0.0));
			local=MatrixMult(local, MatrixTranslatev(assets[assetIndices[MODEL_FIGHTER]].model.center));
			local=MatrixMult(local, QuatToMatrix(enemy[i].body.orientation));
			perFrame[index].fighterInstancePtr[1+i]=MatrixMult(local, MatrixTranslatev(enemy[i].body.position));
		}
	}

	// Update instance matrix data
	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		float radiusScale=0.666667f;

		matrix local=MatrixScale(asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale);
		local=MatrixMult(local, QuatToMatrix(asteroids[i].orientation));
		perFrame[index].asteroidInstancePtr[i]=MatrixMult(local, MatrixTranslatev(asteroids[i].position));
	}
	//////

	ClientNetwork_SendStatus();

	// Always run physics on the camera's body
	if(pausePhysics)
		PhysicsIntegrate(&camera.body, fTimeStep);

	// Update player model instance data
	if(clientSocket!=-1)
	{
		for(uint32_t i=0;i<connectedClients;i++)
		{
			const float scale=(1.0f/assets[assetIndices[MODEL_FIGHTER]].model.radius)*netCameras[i].body.radius;
			matrix local=MatrixScale(scale, scale, scale);
			local=MatrixMult(local, MatrixRotate(PI/2.0f, 0.0f, 1.0f, 0.0));
			local=MatrixMult(local, MatrixTranslatev(assets[assetIndices[MODEL_FIGHTER]].model.center));
			local=MatrixMult(local, QuatToMatrix(netCameras[i].body.orientation));
			perFrame[index].fighterInstancePtr[i]=MatrixMult(local, MatrixTranslatev(netCameras[i].body.position));
		}
	}
	else
	{
		const float scale=(1.0f/assets[assetIndices[MODEL_FIGHTER]].model.radius)*camera.body.radius;
		matrix local=MatrixScale(scale, scale, scale);
		local=MatrixMult(local, MatrixRotate(PI/2.0f, 0.0f, 1.0f, 0.0));
		local=MatrixMult(local, MatrixTranslatev(assets[assetIndices[MODEL_FIGHTER]].model.center));
		local=MatrixMult(local, QuatToMatrix(camera.body.orientation));
		perFrame[index].fighterInstancePtr[0]=MatrixMult(local, MatrixTranslatev(camera.body.position));
	}

	for(uint32_t i=0;i<NUM_CUBE;i++)
	{
		matrix local=MatrixScalev(cubeBody[i].size);
		local=MatrixMult(local, MatrixRotate(PI/2.0f, 0.0f, 1.0f, 0.0));
		local=MatrixMult(local, MatrixTranslatev(assets[assetIndices[MODEL_CUBE]].model.center));
		local=MatrixMult(local, QuatToMatrix(cubeBody[i].orientation));
		perFrame[index].cubeInstancePtr[i]=MatrixMult(local, MatrixTranslatev(cubeBody[i].position));
	}

	// Update camera and modelview matrix
	modelView=CameraUpdate(&camera, fTimeStep);

	for(uint32_t i=0;i<NUM_ENEMY;i++)
		CameraUpdate(&enemy[i], fTimeStep);

	// View from enemy camera
	//modelView=MatrixMult(CameraUpdate(&enemy[0], fTimeStep), MatrixTranslate(0.0f, -5.0f, -10.0f));
	//////

	physicsTime=(float)(GetClock()-startTime);

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
	static uint32_t index=0, imageIndex[2]={ 0, 0 };

	Thread_AddJob(&threadPhysics, Thread_Physics, (void *)&index);
	//Thread_Physics((void *)&index);

	vkWaitForFences(vkContext.device, 1, &perFrame[index].frameFence, VK_TRUE, UINT64_MAX);

	// Handle VR frame start
	if(config.isVR)
	{
		if(!VR_StartFrame(&xrContext, imageIndex))
		{
			// Wait for physics to finish, then dump the frame and start over
			ThreadBarrier_Wait(&physicsThreadBarrier);

			index=(index+1)%FRAMES_IN_FLIGHT;

			return;
		}

		projection[0]=VR_GetEyeProjection(&xrContext, 0);
		projection[1]=VR_GetEyeProjection(&xrContext, 1);

		headPose=VR_GetHeadPose(&xrContext);

		// TODO: Find a better place for VR input handling!
		leftHand=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.leftHandSpace, 0);
		leftTrigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 0);
		leftGrip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 0);
		leftThumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 0);

		rightHand=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.rightHandSpace, 1);
		rightTrigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 1);
		rightGrip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 1);
		rightThumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 1);

		const float speed=240.0f;
		const float rotation=5.0f;

		camera.body.velocity=Vec3_Subv(camera.body.velocity, Vec3_Muls(camera.right, leftThumbstick.x*speed*fTimeStep));
		camera.body.velocity=Vec3_Addv(camera.body.velocity, Vec3_Muls(camera.forward, leftThumbstick.y*speed*fTimeStep));

		camera.body.angularVelocity.y-=rightThumbstick.x*rotation*fTimeStep;
		camera.body.angularVelocity.x-=rightThumbstick.y*rotation*fTimeStep;

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

			FireParticleEmitter(Vec3_Addv(camera.body.position, Vec3_Muls(direction, camera.body.radius)), direction);
		}

		if(rightTrigger<0.25f&&!rightTriggerOnce)
			rightTriggerOnce=true;
	}
	else
	// Handle non-VR frame start
	{
		VkResult Result=vkAcquireNextImageKHR(vkContext.device, swapchain.swapchain, UINT64_MAX, perFrame[index].completeSemaphore, VK_NULL_HANDLE, &imageIndex[0]);

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date or suboptimal... Rebuilding.\n");
			RecreateSwapchain();

			// Wait for physics to finish, then dump the frame and start over
			ThreadBarrier_Wait(&physicsThreadBarrier);

			index=(index+1)%FRAMES_IN_FLIGHT;

			return;
		}

		projection[0]=MatrixInfPerspective(90.0f, (float)config.renderWidth/config.renderHeight, 0.01f);
		headPose=MatrixIdentity();

		// TODO: Find a better place for gamepad input handling!
		const float speed=240.0f;
		const float rotation=5.0f;

		camera.body.velocity=Vec3_Subv(camera.body.velocity, Vec3_Muls(camera.right, leftThumbstick.x*speed*fTimeStep));
		camera.body.velocity=Vec3_Addv(camera.body.velocity, Vec3_Muls(camera.forward, leftThumbstick.y*speed*fTimeStep));

		camera.body.angularVelocity.y-=rightThumbstick.x*rotation*fTimeStep;
		camera.body.angularVelocity.x-=rightThumbstick.y*rotation*fTimeStep;
	}

	ConsoleDraw(&console);

	Audio_SetStreamVolume(0, UI_GetBarGraphValue(&UI, volumeID));

	UI_UpdateTextTitleTextf(&UI, currentTrack, "Current track: %s", GetCurrentMusicTrack());

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

	EyeRender(index, 0, headPose);

	if(config.isVR)
		EyeRender(index, 1, headPose);

	// Final drawing compositing
	CompositeDraw(imageIndex[0], index, 0);
	//////

	// Other eye compositing
	if(config.isVR)
		CompositeDraw(imageIndex[1], index, 1);

	// Reset the font text collection for the next frame
	Font_Reset(&font);

	vkEndCommandBuffer(perFrame[index].commandBuffer);

	// Wait for physics to finish before submitting frame
	ThreadBarrier_Wait(&physicsThreadBarrier);

	// Submit command queue
	VkSubmitInfo SubmitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&perFrame[index].completeSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&swapchain.waitSemaphore[imageIndex[0]],
		.commandBufferCount=1,
		.pCommandBuffers=&perFrame[index].commandBuffer,
	};

	if(config.isVR)
	{
		SubmitInfo.waitSemaphoreCount=0;
		SubmitInfo.pWaitSemaphores=VK_NULL_HANDLE;
		SubmitInfo.signalSemaphoreCount=0;
		SubmitInfo.pSignalSemaphores=VK_NULL_HANDLE;
	}

	vkQueueSubmit(vkContext.graphicsQueue, 1, &SubmitInfo, perFrame[index].frameFence);

	// Handle VR frame end
	if(config.isVR)
	{
		VR_EndFrame(&xrContext);
	}
	else
	// Handle non-VR frame end
	{
		// And present it to the screen
		VkResult Result=vkQueuePresentKHR(vkContext.graphicsQueue, &(VkPresentInfoKHR)
		{
			.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount=1,
			.pWaitSemaphores=&swapchain.waitSemaphore[imageIndex[0]],
			.swapchainCount=1,
			.pSwapchains=&swapchain.swapchain,
			.pImageIndices=&imageIndex[0],
		});

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
			DBGPRINTF(DEBUG_WARNING, "vkQueuePresent out of date or suboptimal...\n");
	}

	UpdateLineGraph(&frameTimes, fTimeStep, fTimeStep);
	UpdateLineGraph(&audioTimes, audioTime, fTimeStep);
	UpdateLineGraph(&physicsTimes, physicsTime, fTimeStep);

	index=(index+1)%FRAMES_IN_FLIGHT;
}

void Console_CmdQuit(Console_t *console, const char *param)
{
	isDone=true;
}

void Console_CmdConnect(Console_t *console, const char *param)
{
	if(param==NULL)
		ConsolePrint(console, "Missing parameter.");
	else
	{
		int32_t a=0, b=0, c=0, d=0;

		if(sscanf(param, "%d.%d.%d.%d", &a, &b, &c, &d)!=4)
		{
			ConsolePrint(console, "Invalid IP address.");
			return;
		}
		else
		{
			if(a<0||a>255||b<0||b>255||c<0||c>255||d<0||d>255)
			{
				ConsolePrint(console, "Invalid IP address.");
				return;
			}
		}

		if(!ClientNetwork_Init(NETWORK_ADDRESS(a, b, c, d)))
			ConsolePrint(console, "Connect failed.");
	}
}

void Console_CmdDisconnect(Console_t *console, const char *param)
{
	ClientNetwork_Destroy();
}

void Fire(void *arg)
{
	uint32_t key=KB_SPACE;

	Event_Trigger(EVENT_KEYDOWN, (void *)&key);
	Event_Trigger(EVENT_KEYUP, (void *)&key);
}

uint32_t em=UINT32_MAX;

void Console_CmdExplode(Console_t *console, const char *param)
{
	ParticleSystem_ResetEmitter(&particleSystem, em);
}

// Initialization call from system main
bool Init(void)
{
	vkuMemAllocator_Init(&vkContext);

	LoadingScreenInit(&loadingScreen, NUM_ASSETS+11);

	//const uint32_t seed=time(NULL);
	const uint32_t seed=69420;
	RandomSeed(seed);

	// TODO: This is a hack, fix it proper.
	if(config.isVR)
		swapchain.numImages=xrContext.swapchain[0].numImages;

	ConsoleInit(&console);
	ConsoleRegisterCommand(&console, "quit", Console_CmdQuit);
	ConsoleRegisterCommand(&console, "connect", Console_CmdConnect);
	ConsoleRegisterCommand(&console, "disconnect", Console_CmdDisconnect);
	ConsoleRegisterCommand(&console, "explode", Console_CmdExplode);

	CameraInit(&camera, Vec3(0.0f, 0.0f, -100.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));

	testSpring.position=Vec3(0.0f, 0.0f, -100.0f);
	testSpring.velocity=Vec3b(0.0f);
	testSpring.stiffness=800.0f;
	testSpring.damping=2.0f;
	testSpring.length=10.0f;
	testSpring.mass=1.0f;
	testSpring.invMass=1.0f/testSpring.mass;

	if(!Audio_Init())
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Audio_Init failed.\n");
		return false;
	}

	SFX_Init();
	Music_Init();

	// Load assets
	if(!AssetManagerLoad(assets, NUM_ASSETS))
		return false;

	GenNebulaVolume(&assets[assetIndices[TEXTURE_VOLUME]].image);
	LoadingScreenAdvance(&loadingScreen);

	// Create primary pipeline
	CreateLightingPipeline();
	LoadingScreenAdvance(&loadingScreen);

	// Create skybox pipeline
	CreateSkyboxPipeline();
	LoadingScreenAdvance(&loadingScreen);

	GenerateWorld();

	CreateSpherePipeline();
	LoadingScreenAdvance(&loadingScreen);
	CreateLinePipeline();
	LoadingScreenAdvance(&loadingScreen);

	// Create volumetric rendering pipeline
	CreateVolumePipeline();
	LoadingScreenAdvance(&loadingScreen);

	// Create shadow map pipeline
	CreateShadowPipeline();
	CreateShadowMap();
	LoadingScreenAdvance(&loadingScreen);

	// Create compositing pipeline
	CreateCompositePipeline();
	LoadingScreenAdvance(&loadingScreen);

	// Create primary frame buffers, depth image
	CreateFramebuffers(0);
	CreateCompositeFramebuffers(0);

	// Second eye framebuffer for VR
	if(config.isVR)
	{
		CreateFramebuffers(1);
		CreateCompositeFramebuffers(1);
	}

	LoadingScreenAdvance(&loadingScreen);
	CreateLineGraphPipeline();
	CreateLineGraph(&frameTimes, 200, 0.1f, 0.0f, 1.0f/30.0f, Vec2(200.0f, 16.0f), Vec2(120.0f, config.renderHeight-32.0f), Vec4(0.5f, 0.5f, 0.0f, 1.0f));
	CreateLineGraph(&audioTimes, 200, 0.1f, 0.0f, 1.0f/30.0f, Vec2(200.0f, 16.0f), Vec2(120.0f, config.renderHeight-48.0f), Vec4(0.5f, 0.5f, 0.0f, 1.0f));
	CreateLineGraph(&physicsTimes, 200, 0.1f, 0.0f, 1.0f/30.0f, Vec2(200.0f, 16.0f), Vec2(120.0f, config.renderHeight-64.0f), Vec4(0.5f, 0.5f, 0.0f, 1.0f));

	// Set up particle system
	if(!ParticleSystem_Init(&particleSystem))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: ParticleSystem_Init failed.\n");
		return false;
	}
	
	ParticleSystem_SetGravity(&particleSystem, 0.0f, 0.0f, 0.0f);
	em=ParticleSystem_AddEmitter(&particleSystem, Vec3(0.0f, 0.0f, 50.0f), Vec3b(10.0f), Vec3b(0.0f), 2.0f, 50, PARTICLE_EMITTER_BURST, NULL);

	// Set up emitter initial state and projectile rigid body parameters
	for(uint32_t i=0;i<MAX_EMITTERS;i++)
	{
		emitters[i].ID=UINT32_MAX;	// No assigned particle ID
		emitters[i].life=-1.0f;		// No life

		const float radius=5.0f;
		const float mass=(1.0f/3000.0f)*(1.33333333f*PI*radius)*10.0f;
		const float inertia=0.4f*mass*(radius*radius);

		emitters[i].body=(RigidBody_t)
		{
			.position=Vec3b(0.0f),

			.velocity=Vec3b(0.0f),
			.force=Vec3b(0.0f),
			.mass=mass,
			.invMass=1.0f/mass,

			.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f),
			.angularVelocity=Vec3b(0.0f),
			.inertia=inertia,
			.invInertia=1.0f/inertia,

			.type=RIGIDBODY_SPHERE,
			.radius=5.0f,
		};
	}

	SpatialHash_Create(&spatialHash, NUM_ASTEROIDS/2, 50.0f);

	Font_Init(&font);

	UI_Init(&UI, Vec2(0.0f, 0.0f), Vec2((float)config.renderWidth, (float)config.renderHeight), compositeRenderPass);

#ifdef ANDROID
	UI_AddButton(&UI, Vec2(0.0f, UI.size.y-50.0f), Vec2(100.0f, 50.0f), Vec3(0.25f, 0.25f, 0.25f), UI_CONTROL_VISIBLE, "Random", (UIControlCallback)GenerateWorld);
	UI_AddButton(&UI, Vec2(0.0f, UI.size.y-100.0f), Vec2(100.0f, 50.0f), Vec3(0.25f, 0.25f, 0.25f), UI_CONTROL_VISIBLE, "Fire", (UIControlCallback)Fire);
#endif

	windowID=UI_AddWindow(&UI, Vec2(UI.size.x-450, UI.size.y-50), Vec2(400, 128), Vec3(0.1, 0.1, 0.1), UI_CONTROL_VISIBLE, "Controls");

	uint32_t playButton=UI_AddButton(&UI,
				 Vec2(0, -50),							// Position
				 Vec2(100, 50),							// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 UI_CONTROL_VISIBLE,					// Not hidden
				 "Play",								// Title text
				 StartStreamCallback);					// Callback
	uint32_t pauseButton=UI_AddButton(&UI,
				 Vec2(100, -50),						// Position
				 Vec2(100, 50),							// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 UI_CONTROL_VISIBLE,					// Not hidden
				 "Pause",								// Title text
				 StopStreamCallback);					// Callback
	uint32_t prevButton=UI_AddButton(&UI,
				 Vec2(200, -50),						// Position
				 Vec2(100, 50),							// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 UI_CONTROL_VISIBLE,					// Not hidden
				 "Prev",								// Title text
				 PrevTrackCallback);					// Callback
	uint32_t nextButton=UI_AddButton(&UI,
				 Vec2(300, -50),						// Position
				 Vec2(100, 50),							// Size
				 Vec3(0.25f, 0.25f, 0.25f),				// Color
				 UI_CONTROL_VISIBLE,					// Not hidden
				 "Next",								// Title text
				 NextTrackCallback);					// Callback
	currentTrack=UI_AddText(&UI, Vec2(0, -58), 16.0f, Vec3b(1.0f), UI_CONTROL_VISIBLE, "NULL");
	volumeID=UI_AddBarGraph(&UI,
				Vec2(0, -96),							// Position
				Vec2(400, 30),							// Size
				Vec3(0.1f, 1.0f, 0.1f),					// Color
				UI_CONTROL_VISIBLE,						// Not hidden
				"Volume",								// Title text
				UI_CONTROL_MODIFIABLE,					// Control is modifiable
				UI_CONTROL_HORIZONTAL,					// Bar is horizontal
				0.0f, 1.0f, 0.125f);					// min/max/initial value
	colorShiftID=UI_AddBarGraph(&UI,
				Vec2(0, -128),							// Position
				Vec2(400, 30),							// Size
				Vec3(0.1f, 0.1f, 1.0f),					// Color
				UI_CONTROL_VISIBLE,						// Not hidden
				"Cloud Color Shift",					// Title text
				UI_CONTROL_MODIFIABLE,					// Control is modifiable
				UI_CONTROL_HORIZONTAL,					// Bar is horizontal
				0.0f, 1.0f, 0.45f);						// min/max/initial value

	UI_WindowAddControl(&UI, windowID, playButton);
	UI_WindowAddControl(&UI, windowID, pauseButton);
	UI_WindowAddControl(&UI, windowID, prevButton);
	UI_WindowAddControl(&UI, windowID, nextButton);
	UI_WindowAddControl(&UI, windowID, currentTrack);
	UI_WindowAddControl(&UI, windowID, volumeID);
	UI_WindowAddControl(&UI, windowID, colorShiftID);

	UI_AddSprite(&UI, Vec2(UI.size.x/2.0f, UI.size.y/2.0f), Vec2(50.0f, 50.0f), Vec3b(1.0f), UI_CONTROL_VISIBLE, &assets[assetIndices[TEXTURE_CROSSHAIR]].image, 0.0f);

	consoleBackground=UI_AddSprite(&UI, Vec2(UI.size.x/2.0f, 100.0f-16.0f+(16.0f*6.0f/2.0f)), Vec2(UI.size.x, 16.0f*6.0f), Vec3b(1.0f), UI_CONTROL_HIDDEN, &assets[assetIndices[TEXTURE_FIGHTER1]].image, 0.0f);

	editWindowID=UI_AddWindow(&UI, Vec2(100, 300), Vec2(200, 50), Vec3(0.5, 0.1, 0.1), UI_CONTROL_VISIBLE, "Edit control test");
	editControl=UI_AddEditText(&UI, Vec2(0, -50), Vec2(200, 50), Vec3b(1.0f), UI_CONTROL_VISIBLE, UI_CONTROL_MODIFIABLE, 150, "");
	UI_WindowAddControl(&UI, editWindowID, editControl);

	cursorID=UI_AddCursor(&UI, Vec2(0.0f, 0.0f), 16.0f, Vec3b(1.0f), UI_CONTROL_VISIBLE);

	// Other per-frame data
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		// Create needed fence and semaphore for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].completeSemaphore);

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

	DestroyLoadingScreen(&loadingScreen);

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

	config.depthFormat=VK_FORMAT_D32_SFLOAT;
	result=vkGetPhysicalDeviceImageFormatProperties(vkContext.physicalDevice,
													config.depthFormat,
													VK_IMAGE_TYPE_2D,
													VK_IMAGE_TILING_OPTIMAL,
													VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
													0,
													&imageFormatProps);

	if(result!=VK_SUCCESS)
	{
		config.depthFormat=VK_FORMAT_D24_UNORM_S8_UINT;
		result=vkGetPhysicalDeviceImageFormatProperties(vkContext.physicalDevice, config.depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &imageFormatProps);

		if(result!=VK_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "CreateFramebuffers: No suitable depth format found.\n");
			return false;
		}
	}

	vkuCreateTexture2D(&vkContext, &colorImage[eye], config.renderWidth, config.renderHeight, config.colorFormat, config.MSAA);
	vkuCreateTexture2D(&vkContext, &depthImage[eye], config.renderWidth, config.renderHeight, config.depthFormat, config.MSAA);
	vkuCreateTexture2D(&vkContext, &colorResolve[eye], config.renderWidth, config.renderHeight, config.colorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, colorImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, colorResolve[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=renderPass,
		.attachmentCount=3,
		.pAttachments=(VkImageView[]){ colorImage[eye].imageView, depthImage[eye].imageView, colorResolve[eye].imageView },
		.width=config.renderWidth,
		.height=config.renderHeight,
		.layers=1,
	}, 0, &framebuffer[eye]);

	return true;
}

void DestroyFramebuffers(void)
{
	vkuDestroyImageBuffer(&vkContext, &colorImage[0]);
	vkuDestroyImageBuffer(&vkContext, &depthImage[0]);
	vkuDestroyImageBuffer(&vkContext, &colorResolve[0]);

	vkDestroyFramebuffer(vkContext.device, framebuffer[0], VK_NULL_HANDLE);

	if(config.isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorImage[1]);
		vkuDestroyImageBuffer(&vkContext, &depthImage[1]);
		vkuDestroyImageBuffer(&vkContext, &colorResolve[1]);

		vkDestroyFramebuffer(vkContext.device, framebuffer[1], VK_NULL_HANDLE);
	}
}

// Rebuild Vulkan swapchain and related data
void RecreateSwapchain(void)
{
	// Wait for the device to complete any pending work
	vkDeviceWaitIdle(vkContext.device);

	// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
	// This is basically just the swapchain, framebuffers, depth buffers, and semaphores (since they can't be just reset like fences).

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkDestroyFence(vkContext.device, perFrame[i].frameFence, VK_NULL_HANDLE);
		vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		vkDestroySemaphore(vkContext.device, perFrame[i].completeSemaphore, VK_NULL_HANDLE);
		vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].completeSemaphore);
	}

	// swapchain, framebuffer, and depth buffer destruction
	DestroyFramebuffers();
	DestroyCompositeFramebuffers();

	// Recreate the swapchain, vkuCreateSwapchain will see that there is an existing swapchain and deal accordingly.
	vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE);

	config.renderWidth=max(2, swapchain.extent.width);
	config.renderHeight=max(2, swapchain.extent.height);

	UI.size.x=(float)config.renderWidth;
	UI.size.y=(float)config.renderHeight;

	// Recreate the framebuffer
	CreateFramebuffers(0);
	CreateCompositeFramebuffers(0);

	if(config.isVR)
	{
		CreateFramebuffers(1);
		CreateCompositeFramebuffers(1);
	}
}

// Destroy call from system main
void Destroy(void)
{
	vkDeviceWaitIdle(vkContext.device);

	ClientNetwork_Destroy();

	Audio_Destroy();
	SFX_Destroy();
	Music_Destroy();


	if(config.isVR)
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

	DestroyLineGraphPipeline();
	DestroyLineGraph(&frameTimes);
	DestroyLineGraph(&audioTimes);
	DestroyLineGraph(&physicsTimes);

	// Compositing pipeline
	DestroyComposite();
	//////////

	// Spatial hash for collision destruction
	SpatialHash_Destroy(&spatialHash);
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
	Font_Destroy(&font);
	//////////

	// Instance buffer destruction
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].asteroidInstance);
		vkuDestroyBuffer(&vkContext, &perFrame[i].fighterInstance);
		vkuDestroyBuffer(&vkContext, &perFrame[i].cubeInstance);
	}
	//////////

	// Assets from manager
	AssetManagerDestroy(assets, NUM_ASSETS);
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
	vkuDestroyImageBuffer(&vkContext, &depthImage[0]);

	vkDestroyFramebuffer(vkContext.device, framebuffer[0], VK_NULL_HANDLE);

	if(config.isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorImage[1]);
		vkuDestroyImageBuffer(&vkContext, &colorResolve[1]);
		vkuDestroyImageBuffer(&vkContext, &depthImage[1]);

		vkDestroyFramebuffer(vkContext.device, framebuffer[1], VK_NULL_HANDLE);
	}

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		// Destroy sync objects
		vkDestroyFence(vkContext.device, perFrame[i].frameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(vkContext.device, perFrame[i].completeSemaphore, VK_NULL_HANDLE);

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
