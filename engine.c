#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>

#include "system/system.h"
#include "network/network.h"
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
#include "system/threads.h"
#include "vr/vr.h"
#include "font/font.h"
#include "audio/audio.h"
#include "physics/physics.h"
#include "ui/ui.h"
#include "console/console.h"

#include "models.h"
#include "textures.h"
#include "skybox.h"
#include "shadow.h"
#include "nebula.h"
#include "composite.h"
#include "perframe.h"
#include "sounds.h"

#ifndef ANDROID
#include "music.h"
#endif

extern bool Done;

// Initial window size
extern uint32_t windowWidth, windowHeight;
uint32_t renderWidth=1920, renderHeight=1080;

// external switch from system for if VR was initialized
extern bool isVR;
XruContext_t xrContext;

// Vulkan instance handle and context structs
VkInstance Instance;
VkuContext_t Context;

// Vulkan memory allocator zone
VkuMemZone_t *VkZone;

// Camera data
Camera_t Camera;
matrix modelView, projection[2], headPose;

// extern timing data from system main
extern float fps, fTimeStep, fTime;

// Main particle system struct
ParticleSystem_t particleSystem;

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
VkFormat colorFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
VkuImage_t colorImage[2];		// left and right eye color buffer

// Depth buffer image and format
VkFormat depthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;
VkuImage_t depthImage[2];		// left and right eye depth buffers

// Renderpass for primary renderer
VkRenderPass RenderPass;
// Framebuffers, per-eye
VkFramebuffer Framebuffer[2];

// Primary rendering Vulkan stuff
VkuDescriptorSet_t mainDescriptorSet;
VkPipelineLayout mainPipelineLayout;
VkuPipeline_t mainPipeline;
//////

// Debug rendering Vulkan stuff
VkPipelineLayout spherePipelineLayout;
VkuPipeline_t spherePipeline;

VkPipelineLayout linePipelineLayout;
VkuPipeline_t linePipeline;
//////

// Asteroid data
VkuBuffer_t asteroidInstance;
matrix *asteroidInstanceData;

#define NUM_ASTEROIDS 1000
RigidBody_t asteroids[NUM_ASTEROIDS];
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
ThreadWorker_t Thread[NUM_THREADS], ThreadPhysics, ThreadNetUpdate;
pthread_barrier_t ThreadBarrier, ThreadBarrier_Physics;
//////

// Network stuff
#define CONNECT_PACKETMAGIC		('C'|('o'<<8)|('n'<<16)|('n'<<24)) // "Conn"
#define DISCONNECT_PACKETMAGIC	('D'|('i'<<8)|('s'<<16)|('C'<<24)) // "DisC"
#define STATUS_PACKETMAGIC		('S'|('t'<<8)|('a'<<16)|('t'<<24)) // "Stat"
#define FIELD_PACKETMAGIC		('F'|('e'<<8)|('l'<<16)|('d'<<24)) // "Feld"
#define MAX_CLIENTS 16

// PacketMagic determines packet type:
//
// Connect:
//		Client sends connect magic, server responds back with current random seed and slot.
// Disconnect:
//		Client sends disconnect magic, server closes socket and removes client from list.
// Status:
//		Client to server: Sends current camera data
//		Server to client: Sends all current connected client cameras.
// Field:
//		Server sends current play field (as it sees it) to all connected clients at a regular interval.

// Camera data for sending over the network
typedef struct
{
	vec3 Position, Velocity;
	vec3 Forward, Up;
} NetCamera_t;

// Connect data when connecting to server
typedef struct
{
	uint32_t Seed;
	uint16_t Port;
} NetConnect_t;

// Overall data network packet
typedef struct
{
	uint32_t PacketMagic;
	uint32_t ClientID;
	union
	{
		NetConnect_t Connect;
		NetCamera_t Camera;
	};
} NetworkPacket_t;

uint32_t serverAddress=NETWORK_ADDRESS(192, 168, 1, 10);
uint16_t serverPort=4545;

uint16_t clientPort=0;
uint32_t clientID=0;

Socket_t clientSocket=-1;

uint32_t connectedClients=0;
Camera_t netCameras[MAX_CLIENTS];
//////

// UI Stuff
Font_t Fnt; // Fnt instead of Font, because Xlib is dumb and declares a type Font *rolley-eyes*
UI_t UI;

uint32_t volumeID=UINT32_MAX;
uint32_t faceID=UINT32_MAX;
uint32_t cursorID=UINT32_MAX;
//////

Console_t Console;

XrPosef leftHand, rightHand;
float leftTrigger, rightTrigger;
float leftGrip, rightGrip;
vec2 leftThumbstick, rightThumbstick;

void RecreateSwapchain(void);

// Create functions for creating render data for asteroids
bool CreateFramebuffers(uint32_t eye)
{
	VkImageFormatProperties imageFormatProps;
	VkResult Result;

	depthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;
	Result=vkGetPhysicalDeviceImageFormatProperties(Context.PhysicalDevice,
													depthFormat,
													VK_IMAGE_TYPE_2D,
													VK_IMAGE_TILING_OPTIMAL,
													VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
													0,
													&imageFormatProps);

	if(Result!=VK_SUCCESS)
	{
		depthFormat=VK_FORMAT_D24_UNORM_S8_UINT;
		Result=vkGetPhysicalDeviceImageFormatProperties(Context.PhysicalDevice, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT, 0, &imageFormatProps);

		if(Result!=VK_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "CreateFramebuffers: No suitable depth format found.\n");
			return false;
		}
	}

	vkuCreateTexture2D(&Context, &colorImage[eye], renderWidth, renderHeight, colorFormat, MSAA);
	vkuCreateTexture2D(&Context, &depthImage[eye], renderWidth, renderHeight, depthFormat, MSAA);
	vkuCreateTexture2D(&Context, &colorResolve[eye], renderWidth, renderHeight, colorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(commandBuffer, colorImage[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(commandBuffer, depthImage[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	vkuTransitionLayout(commandBuffer, colorResolve[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, commandBuffer);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=RenderPass,
		.attachmentCount=3,
		.pAttachments=(VkImageView[]){ colorImage[eye].View, depthImage[eye].View, colorResolve[eye].View },
		.width=renderWidth,
		.height=renderHeight,
		.layers=1,
	}, 0, &Framebuffer[eye]);

	return true;
}

bool CreatePipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=3,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=colorFormat,
				.samples=MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=depthFormat,
				.samples=MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
			{
				.format=colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
			.pResolveAttachments=&(VkAttachmentReference)
			{
				.attachment=2,
				.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
		},
		.dependencyCount=3,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
				.dependencyFlags=0,
			},
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=0,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
				.dependencyFlags=0,
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		},
	}, 0, &RenderPass);

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuCreateHostBuffer(&Context, &perFrame[i].mainUBOBuffer[0], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, perFrame[i].mainUBOBuffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[0]);

		vkuCreateHostBuffer(&Context, &perFrame[i].mainUBOBuffer[1], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, perFrame[i].mainUBOBuffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[1]);
	}

	vkuInitDescriptorSet(&mainDescriptorSet, &Context);

	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

	vkuAssembleDescriptorSetLayout(&mainDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&mainDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &mainPipelineLayout);

	vkuInitPipeline(&mainPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&mainPipeline, mainPipelineLayout);
	vkuPipeline_SetRenderPass(&mainPipeline, RenderPass);

	mainPipeline.DepthTest=VK_TRUE;
	mainPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	mainPipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	mainPipeline.RasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&mainPipeline, "shaders/lighting.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&mainPipeline, "shaders/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&mainPipeline, 0, sizeof(vec4)*5, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*4);

	vkuPipeline_AddVertexBinding(&mainPipeline, 1, sizeof(matrix), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&mainPipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}
//////

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
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
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
	if(!asteroidInstance.Buffer)
		vkuCreateHostBuffer(&Context, &asteroidInstance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, asteroidInstance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&asteroidInstanceData);

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		vec3 randomDirection=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(&randomDirection);

		const float radiusScale=1.5f;

		asteroids[i].velocity=Vec3_Muls(randomDirection, 10.0f);
		asteroids[i].force=Vec3b(0.0f);

		asteroids[i].orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		asteroids[i].angularVelocity=randomDirection;

		asteroids[i].mass=(1.0f/3000.0f)*(1.33333333f*PI*asteroids[i].radius);
		asteroids[i].invMass=1.0f/asteroids[i].mass;

		asteroids[i].inertia=0.4f*asteroids[i].mass*(asteroids[i].radius*asteroids[i].radius);
		asteroids[i].invInertia=1.0f/asteroids[i].inertia;
	}
	//////
}
//////

// Debug sphere pipeline
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
	}, 0, &spherePipelineLayout);

	vkuInitPipeline(&spherePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&spherePipeline, spherePipelineLayout);
	vkuPipeline_SetRenderPass(&spherePipeline, RenderPass);

	spherePipeline.DepthTest=VK_TRUE;
	spherePipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	spherePipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	spherePipeline.RasterizationSamples=MSAA;
	//spherePipeline.PolygonMode=VK_POLYGON_MODE_LINE;

	if(!vkuPipeline_AddStage(&spherePipeline, "shaders/sphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&spherePipeline, "shaders/sphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&spherePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}

void DrawSphere(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, float radius, vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
	} spherePC;

	matrix local=MatrixIdentity();
	local=MatrixMult(local, MatrixScale(radius, radius, radius));
	local=MatrixMult(local, MatrixTranslatev(position));
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->HMD);
	spherePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	spherePC.color=color;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.Pipeline);
	vkCmdPushConstants(commandBuffer, spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
//////

// Debug line pipeline
bool CreateLinePipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+(sizeof(vec4)*3),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &linePipelineLayout);

	vkuInitPipeline(&linePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&linePipeline, linePipelineLayout);
	vkuPipeline_SetRenderPass(&linePipeline, RenderPass);

	linePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	linePipeline.DepthTest=VK_TRUE;
	linePipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	linePipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	linePipeline.RasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&linePipeline, "shaders/line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&linePipeline, "shaders/line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&linePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}

void DrawLine(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 start, vec3 end, vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
		vec4 verts[2];
	} linePC;

	matrix local=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	linePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	linePC.color=color;
	linePC.verts[0]=Vec4_Vec3(start, 1.0f);
	linePC.verts[1]=Vec4_Vec3(end, 1.0f);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.Pipeline);
	vkCmdPushConstants(commandBuffer, linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);
}
//////

void DrawPlayer(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, Camera_t player)
{
	struct
	{
		matrix mvp;
		vec4 color, start, end;
	} linePC;

	vec4 position=Vec4_Vec3(player.Position, 1.0f);
	vec4 forward=Vec4_Vec3(player.Forward, 1.0f);
	vec4 up=Vec4_Vec3(player.Up, 1.0f);
	vec4 right=Vec4_Vec3(player.Right, 1.0f);

	linePC.mvp=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->projection);
	linePC.start=position;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.Pipeline);

	linePC.color=Vec4(1.0f, 0.0f, 0.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(forward, 15.0f));
	vkCmdPushConstants(commandBuffer, linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);

	linePC.color=Vec4(0.0f, 1.0f, 0.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(up, 15.0f));
	vkCmdPushConstants(commandBuffer, linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);

	linePC.color=Vec4(0.0f, 0.0f, 1.0f, 1.0f);
	linePC.end=Vec4_Addv(position, Vec4_Muls(right, 15.0f));
	vkCmdPushConstants(commandBuffer, linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);

	struct
	{
		matrix mvp;
		vec4 color;
	} spherePC;

	matrix local=MatrixIdentity();
	local=MatrixMult(local, MatrixScale(5.0f, 5.0f, 5.0f));
	local=MatrixMult(local, MatrixInverse(MatrixLookAt(player.Position, Vec3_Addv(player.Position, player.Forward), player.Up)));

	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	spherePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);

	spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.Pipeline);
	vkCmdPushConstants(commandBuffer, spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}

// General thread constructor for threads using Vulkan
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
			.renderPass=RenderPass,
			.framebuffer=Framebuffer[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.Pipeline);

	// Draw the models
	vkCmdBindVertexBuffers(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 1, 1, &asteroidInstance.Buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 0, &Textures[2*i+0]);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 1, &Textures[2*i+1]);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 2, &shadowDepth);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 3, perFrame[Data->Index].mainUBOBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 4, perFrame[Data->Index].skyboxUBOBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&mainDescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);

		vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipelineLayout, 0, 1, &mainDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

		matrix local=MatrixIdentity();
		//local=MatrixMult(local, MatrixRotate(fTime, 1.0f, 0.0f, 0.0f));
		//local=MatrixMult(local, MatrixRotate(fTime, 0.0f, 1.0f, 0.0f));

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], mainPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &local);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &Models[i].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<Models[i].NumMesh;j++)
		{
			vkCmdBindIndexBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Models[i].Mesh[j].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Models[i].Mesh[j].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}

	// Draw VR "hands"
	if(isVR)
	{
		struct
		{
			matrix mvp;
			vec4 color;
		} spherePC;

		spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.Pipeline);

		vec3 leftPos=Vec3(leftHand.position.x, leftHand.position.y, leftHand.position.z);
		vec4 leftRot=Vec4(leftHand.orientation.x, leftHand.orientation.y, leftHand.orientation.z, leftHand.orientation.w);
		matrix local=MatrixMult(MatrixMult(QuatMatrix(leftRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->projection);

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 60, 1, 0, 0);

		vec3 rightPos=Vec3(rightHand.position.x, rightHand.position.y, rightHand.position.z);
		vec4 rightRot=Vec4(rightHand.orientation.x, rightHand.orientation.y, rightHand.orientation.z, rightHand.orientation.w);
		local=MatrixMult(MatrixMult(QuatMatrix(rightRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->projection);

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 60, 1, 0, 0);

		struct
		{
			matrix mvp;
			vec4 Color;
			vec4 Verts[2];
		} LinePC;

		LinePC.Verts[0]=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		LinePC.Verts[1]=Vec4(0.0f, 0.0f, -1.0f, 1.0f);

		vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.Pipeline);

		LinePC.Color=Vec4(leftTrigger*100.0f+1.0f, 1.0f, leftGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatMatrix(leftRot), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->HMD);
		LinePC.mvp=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->projection);

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinePC), &LinePC);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 2, 1, 0, 0);

		LinePC.Color=Vec4(rightTrigger*100.0f+1.0f, 1.0f, rightGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatMatrix(rightRot), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->HMD);
		LinePC.mvp=MatrixMult(local, perFrame[Data->Index].mainUBO[Data->Eye]->projection);

		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinePC), &LinePC);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 2, 1, 0, 0);
	}

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

		vec4 position=Vec4(netCameras[i].Position.x, netCameras[i].Position.y, netCameras[i].Position.z, 1.0f);
		vec4 forward=Vec4(netCameras[i].Forward.x, netCameras[i].Forward.y, netCameras[i].Forward.z, 1.0f);
		vec4 up=Vec4(netCameras[i].Up.x, netCameras[i].Up.y, netCameras[i].Up.z, 1.0f);
		netCameras[i].Right=Vec3_Cross(netCameras[i].Forward, netCameras[i].Up);
		vec4 right=Vec4(netCameras[i].Right.x, netCameras[i].Right.y, netCameras[i].Right.z, 1.0f);

		line_ubo.mvp=MatrixMult(PerFrame[Data->Index].Main_UBO[Data->Eye]->modelview, PerFrame[Data->Index].Main_UBO[Data->Eye]->projection);
		line_ubo.start=position;

		vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.mainPipeline);

		line_ubo.color=Vec4(1.0f, 0.0f, 0.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(forward, 15.0f));
		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 2, 1, 0, 0);

		line_ubo.color=Vec4(0.0f, 1.0f, 0.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(up, 15.0f));
		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 2, 1, 0, 0);

		line_ubo.color=Vec4(0.0f, 0.0f, 1.0f, 1.0f);
		line_ubo.end=Vec4_Addv(position, Vec4_Muls(right, 15.0f));
		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 2, 1, 0, 0);

		struct
		{
			matrix mvp;
			vec4 color;
		} sphere_ubo={ 0 };

		matrix local=MatrixIdentity();
		local=MatrixMult(local, MatrixScale(10.0f, 10.0f, 10.0f));
		local=MatrixMult(local, MatrixInverse(MatrixLookAt(netCameras[i].Position, Vec3_Addv(netCameras[i].Position, netCameras[i].Forward), netCameras[i].Up)));

		local=MatrixMult(local, PerFrame[Data->Index].Main_UBO[Data->Eye]->modelview);
		sphere_ubo.mvp=MatrixMult(local, PerFrame[Data->Index].Main_UBO[Data->Eye]->projection);

		sphere_ubo.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.mainPipeline);
		vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(sphere_ubo), &sphere_ubo);
		vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 60, 1, 0, 0);
	}
	//////
#endif

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
			.renderPass=RenderPass,
			.framebuffer=Framebuffer[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxDescriptorSet, 0, perFrame[Data->Index].mainUBOBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxDescriptorSet, 1, perFrame[Data->Index].skyboxUBOBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&skyboxDescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);

	vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1, &skyboxDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

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
			.renderPass=RenderPass,
			.framebuffer=Framebuffer[Data->Eye]
		}
	});

	vkCmdSetViewport(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

	matrix Modelview=MatrixMult(perFrame[Data->Index].mainUBO[Data->Eye]->modelView, perFrame[Data->Index].mainUBO[Data->Eye]->HMD);
	ParticleSystem_Draw(&particleSystem, Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], Data->PerFrame[Data->Index].DescriptorPool[Data->Eye], Modelview, perFrame[Data->Index].mainUBO[Data->Eye]->projection);

	vkCmdBindPipeline(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.Pipeline);

	uFrame++;
	vkCmdPushConstants(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], volumePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &uFrame);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumeDescriptorSet, 0, &Textures[TEXTURE_VOLUME]);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumeDescriptorSet, 1, perFrame[Data->Index].mainUBOBuffer[Data->Eye].Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&volumeDescriptorSet, Data->PerFrame[Data->Index].DescriptorPool[Data->Eye]);
	vkCmdBindDescriptorSets(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipelineLayout, 0, 1, &volumeDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye], 36, 1, 0, 0);

	vkEndCommandBuffer(Data->PerFrame[Data->Index].SecCommandBuffer[Data->Eye]);

	pthread_barrier_wait(&ThreadBarrier);
}

// Render everything together, per-eye, per-frame index
void EyeRender(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, matrix headPose)
{
	// Copy projection matrix
	perFrame[index].mainUBO[eye]->projection=projection[eye];

	// Copy modelview matrix
	perFrame[index].mainUBO[eye]->modelView=modelView;

	perFrame[index].mainUBO[eye]->HMD=headPose;

	perFrame[index].mainUBO[eye]->lightColor=perFrame[index].skyboxUBO[eye]->uSunColor;
	perFrame[index].mainUBO[eye]->lightDirection=perFrame[index].skyboxUBO[eye]->uSunPosition;
	perFrame[index].mainUBO[eye]->lightDirection.w=perFrame[index].skyboxUBO[eye]->uSunSize;

	perFrame[index].mainUBO[eye]->lightMVP=shadowUBO.mvp;

	// Start a render pass and clear the frame/depth buffer
	//vkCmdBeginRendering(commandBuffer, &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.flags=VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth, renderHeight } },
	//	.layerCount=1,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=colorImage[Eye].View,
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.resolveMode=VK_RESOLVE_MODE_AVERAGE_BIT,
	//		.resolveImageView=colorResolve[Eye].View,
	//		.resolveImageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//		.clearValue=(VkClearValue){ .color.float32={ 0.0f, 0.0f, 0.0f, 1.0f } },
	//	},
	//	.pDepthAttachment=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=depthImage[Eye].View,
	//		.imageLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
	//		.clearValue=(VkClearValue){ .depthStencil={ 0.0f, 0 } },
	//	},
	//});
	vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=RenderPass,
		.framebuffer=Framebuffer[eye],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}, {{{ 0.0f, 0 }}} },
		.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth, renderHeight } },
	}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Set per thread data and add the job to that worker thread
	ThreadData[0].Index=index;
	ThreadData[0].Eye=eye;
	Thread_AddJob(&Thread[0], Thread_Main, (void *)&ThreadData[0]);

	ThreadData[1].Index=index;
	ThreadData[1].Eye=eye;
	Thread_AddJob(&Thread[1], Thread_Skybox, (void *)&ThreadData[1]);

	ThreadData[2].Index=index;
	ThreadData[2].Eye=eye;
	Thread_AddJob(&Thread[2], Thread_Particles, (void *)&ThreadData[2]);

	pthread_barrier_wait(&ThreadBarrier);

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(commandBuffer, 3, (VkCommandBuffer[])
	{
		ThreadData[0].PerFrame[index].SecCommandBuffer[eye],
		ThreadData[1].PerFrame[index].SecCommandBuffer[eye],
		ThreadData[2].PerFrame[index].SecCommandBuffer[eye]
	});

	//vkCmdEndRendering(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);
}

// Runs anything physics related
void Thread_Physics(void *Arg)
{
	// Get a pointer to the emitter that's providing the positions
	ParticleEmitter_t *emitter=List_GetPointer(&particleSystem.emitters, 0);

	for(uint32_t i=0;i<emitter->numParticles;i++)
	{
		if(emitter->particles[i].ID!=emitter->ID)
		{
			// Get those positions and set the other emitter's positions to those
			ParticleSystem_SetEmitterPosition(&particleSystem, emitter->particles[i].ID, emitter->particles[i].position);

			// If this particle is dead, delete that emitter and reset it's ID 
			if(emitter->particles[i].life<0.0f)
			{
				ParticleSystem_DeleteEmitter(&particleSystem, emitter->particles[i].ID);
				emitter->particles[i].ID=0;
			}
		}
	}

	ParticleSystem_Step(&particleSystem, fTimeStep);

	// Loop through objects, integrate and check/resolve collisions
	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		// Run physics integration on the asteroids
		PhysicsIntegrate(&asteroids[i], fTimeStep);

		// Check asteroids against other asteroids
		for(uint32_t j=i+1;j<NUM_ASTEROIDS;j++)
			PhysicsSphereToSphereCollisionResponse(&asteroids[i], &asteroids[j]);

		// Check asteroids against the camera
		PhysicsCameraToSphereCollisionResponse(&Camera, &asteroids[i]);

#if 0
		for(uint32_t j=0;j<connectedClients;j++)
		{
			// Don't check for collision with our own net camera
			if(j!=clientID)
				PhysicsCameraToSphereCollisionResponse(&netCameras[j], &asteroids[i]);
		}
#endif

		// Check asteroids against projectile particles
		// Emitter '0' on the particle system contains particles that drive the projectile physics
		ParticleEmitter_t *emitter=List_GetPointer(&particleSystem.emitters, 0);
		// Loop through all the possible particles
		for(uint32_t j=0;j<emitter->numParticles;j++)
		{
			// If the particle ID matches with the projectile ID, then check collision and respond
			if(emitter->particles[j].ID!=emitter->ID)
				PhysicsParticleToSphereCollisionResponse(&emitter->particles[j], &asteroids[i]);
		}
	}

#if 0
	for(uint32_t i=0;i<connectedClients;i++)
	{
		// Don't check for collision with our own net camera
		if(i!=clientID)
			PhysicsCameraToCameraCollisionResponse(&Camera, &netCameras[i]);
	}
	//////
#endif

	// Update camera and modelview matrix
	modelView=CameraUpdate(&Camera, fTimeStep);
	//////

	// Update instance matrix data
	//matrix *Data=NULL;
	//vkMapMemory(Context.Device, asteroidInstance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&Data);

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		const float radiusScale=1.5f;
		matrix local=MatrixScale(asteroids[i].radius/radiusScale, asteroids[i].radius/radiusScale, asteroids[i].radius/radiusScale);
		local=MatrixMult(local, QuatMatrix(asteroids[i].orientation));
		asteroidInstanceData[i]=MatrixMult(local, MatrixTranslatev(asteroids[i].position));
	}

	//vkUnmapMemory(Context.Device, asteroidInstance.DeviceMemory);
	//////

#if 0
	// Network status packet
	if(clientSocket!=-1)
	{
		NetworkPacket_t StatusPacket;

		memset(&StatusPacket, 0, sizeof(NetworkPacket_t));

		StatusPacket.PacketMagic=STATUS_PACKETMAGIC;
		StatusPacket.ClientID=clientID;

		StatusPacket.Camera.Position=Camera.Position;
		StatusPacket.Camera.Velocity=Camera.Velocity;
		StatusPacket.Camera.Forward=Camera.Forward;
		StatusPacket.Camera.Up=Camera.Up;

		Network_SocketSend(clientSocket, (uint8_t *)&StatusPacket, sizeof(NetworkPacket_t), serverAddress, serverPort);
	}
	//////
#endif

	// Barrier now that we're done here
	pthread_barrier_wait(&ThreadBarrier_Physics);
}

#if 0
pthread_t UpdateThread;
bool NetUpdate_Run=true;
uint8_t NetBuffer[32767]={ 0 };

void NetUpdate(void *Arg)
{
	memset(netCameras, 0, sizeof(Camera_t)*MAX_CLIENTS);

	if(clientSocket==-1)
	{
		NetUpdate_Run=false;
		return;
	}

	while(NetUpdate_Run)
	{
		uint8_t *pBuffer=NetBuffer;
		uint32_t Magic=0;
		uint32_t Address=0;
		uint16_t Port=0;

		Network_SocketReceive(clientSocket, NetBuffer, sizeof(NetBuffer), &Address, &Port);

		memcpy(&Magic, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

		if(Magic==STATUS_PACKETMAGIC)
		{
			memcpy(&connectedClients, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<connectedClients;i++)
			{
				uint32_t clientID=0;

				memcpy(&clientID, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

				memcpy(&netCameras[clientID].Position, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].Velocity, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].Forward, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].Up, pBuffer, sizeof(float)*3);			pBuffer+=sizeof(float)*3;

				//DBGPRINTF(DEBUG_INFO, "\033[%d;0H\033[KID %d Pos: %0.1f %0.1f %0.1f", clientID+1, clientID, NetCameras[clientID].Position.x, NetCameras[clientID].Position.y, NetCameras[clientID].Position.z);
			}
		}
		else if(Magic==FIELD_PACKETMAGIC)
		{
			uint32_t asteroidCount=NUM_ASTEROIDS;

			memcpy(&asteroidCount, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<asteroidCount;i++)
			{
				memcpy(&asteroids[i].Position, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
				memcpy(&asteroids[i].Velocity, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
			}
		}
	}
}
#endif

extern vec2 mousePosition;

void FireParticleEmitter(vec3 position, vec3 direction);

bool leftTriggerOnce=true;
bool rightTriggerOnce=true;

static vec3 lastLeftPosition={ 0.0f, 0.0f, 0.0f };

// Render call from system main event loop
void Render(void)
{
	static uint32_t index=0;
	uint32_t imageIndex;

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

		Camera.Velocity.x-=leftThumbstick.x*speed*fTimeStep;
		Camera.Velocity.z+=leftThumbstick.y*speed*fTimeStep;
		Camera.Yaw-=rightThumbstick.x*rotation*fTimeStep;
		Camera.Pitch+=rightThumbstick.y*rotation*fTimeStep;

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
			vec3 Direction=Matrix3x3MultVec3(Vec3(0.0f, 0.0f, -1.0f), MatrixMult(QuatMatrix(rightOrientation), MatrixInverse(modelView)));

			FireParticleEmitter(Vec3_Addv(Camera.Position, Vec3_Muls(Direction, Camera.Radius)), Direction);
			Audio_PlaySample(&Sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &Camera.Position);
		}

		if(rightTrigger<0.25f&&!rightTriggerOnce)
			rightTriggerOnce=true;
	}
	else
	// Handle non-VR frame start
	{
		VkResult Result=vkAcquireNextImageKHR(Context.Device, Swapchain.Swapchain, UINT64_MAX, perFrame[index].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date... Rebuilding.\n");
			RecreateSwapchain();
			return;
		}

		projection[0]=MatrixInfPerspective(90.0f, (float)renderWidth/renderHeight, 0.01f);
		headPose=MatrixIdentity();
	}

	Thread_AddJob(&ThreadPhysics, Thread_Physics, NULL);

	Console_Draw(&Console);

	Audio_SetStreamVolume(UI_GetBarGraphValue(&UI, volumeID));

#ifndef ANDROID
	Font_Print(&Fnt, 16.0f, renderWidth-400.0f, renderHeight-50.0f-16.0f, "Current track: %s", MusicList[CurrentMusic].String);
#endif

	vkWaitForFences(Context.Device, 1, &perFrame[index].frameFence, VK_TRUE, UINT64_MAX);

	// Reset the frame fence and command pool (and thus the command buffer)
	vkResetFences(Context.Device, 1, &perFrame[index].frameFence);
	vkResetDescriptorPool(Context.Device, perFrame[index].descriptorPool, 0);
	vkResetCommandPool(Context.Device, perFrame[index].commandPool, 0);

	// Start recording the commands
	vkBeginCommandBuffer(perFrame[index].commandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	// Update shadow depth map
	ShadowUpdateMap(perFrame[index].commandBuffer, index);

	// Wait for physics to finish before rendering
	pthread_barrier_wait(&ThreadBarrier_Physics);

	vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[0].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	EyeRender(perFrame[index].commandBuffer, index, 0, headPose);

	if(isVR)
	{
		vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[1].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		EyeRender(perFrame[index].commandBuffer, index, 1, headPose);
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

	vkQueueSubmit(Context.Queue, 1, &SubmitInfo, perFrame[index].frameFence);

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
		VkResult Result=vkQueuePresentKHR(Context.Queue, &(VkPresentInfoKHR)
		{
			.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount=1,
			.pWaitSemaphores=&perFrame[index].renderCompleteSemaphore,
			.swapchainCount=1,
			.pSwapchains=&Swapchain.Swapchain,
			.pImageIndices=&imageIndex,
		});

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date... Rebuilding.\n");
			RecreateSwapchain();
			return;
		}

		index=(index+1)%Swapchain.NumImages;
	}
}

void Console_CmdQuit(Console_t *Console, char *Param)
{
	Done=true;
}

// Initialization call from system main
bool Init(void)
{
	// TODO: This is a hack, fix it proper.
	if(isVR)
		Swapchain.NumImages=xrContext.swapchain[0].numImages;

	RandomSeed((uint32_t)GetClock()*UINT32_MAX);

	Event_Add(EVENT_KEYDOWN, Event_KeyDown);
	Event_Add(EVENT_KEYUP, Event_KeyUp);
	Event_Add(EVENT_MOUSEDOWN, Event_MouseDown);
	Event_Add(EVENT_MOUSEUP, Event_MouseUp);
	Event_Add(EVENT_MOUSEMOVE, Event_Mouse);

	Console_Init(&Console, 80, 25);
	Console_AddCommand(&Console, "quit", Console_CmdQuit);

	VkZone=vkuMem_Init(&Context, (size_t)(Context.DeviceProperties2.maxMemoryAllocationSize*0.8f));

	if(VkZone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Init: vkuMem_Init failed.\n");
		return false;
	}

	CameraInit(&Camera, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));
	modelView=CameraUpdate(&Camera, 0.0f);

	if(!Audio_Init())
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Audio_Init failed.\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew1.wav", &Sounds[SOUND_PEW1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew2.wav", &Sounds[SOUND_PEW2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/pew3.wav", &Sounds[SOUND_PEW3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/pew3.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone1.wav", &Sounds[SOUND_STONE1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone2.wav", &Sounds[SOUND_STONE2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/stone3.wav", &Sounds[SOUND_STONE3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/stone3.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/crash.wav", &Sounds[SOUND_CRASH]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/crash.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode1.wav", &Sounds[SOUND_EXPLODE1]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode1.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode2.wav", &Sounds[SOUND_EXPLODE2]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode2.wav\n");
		return false;
	}

	if(!Audio_LoadStatic("assets/explode3.wav", &Sounds[SOUND_EXPLODE3]))
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/explode3.wav\n");
		return false;
	}

#ifndef ANDROID
	Music_Init();
	StartStreamCallback(NULL);
#endif

	// Load models
	if(LoadBModel(&Models[MODEL_ASTEROID1], "assets/asteroid1.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID1]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid1.bmodel\n");
		return false;
	}

	if(LoadBModel(&Models[MODEL_ASTEROID2], "assets/asteroid2.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID2]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid2.bmodel\n");
		return false;
	}

	if(LoadBModel(&Models[MODEL_ASTEROID3], "assets/asteroid3.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID3]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid3.bmodel\n");
		return false;
	}

	if(LoadBModel(&Models[MODEL_ASTEROID4], "assets/asteroid4.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Models[MODEL_ASTEROID4]);
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Init: Failed to load assets/asteroid4.bmodel\n");
		return false;
	}

	uint32_t TriangleCount=0;

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		for(uint32_t j=0;j<Models[i].NumMesh;j++)
			TriangleCount+=Models[i].Mesh[j].NumFace;
	}

	TriangleCount*=NUM_ASTEROIDS;

	DBGPRINTF(DEBUG_ERROR, "\nNot an error, just a total triangle count: %d\n\n", TriangleCount);

	// Load textures
	
	// TODO: For some reason on Linux/Android, the first loaded QOI here has corruption and I'm not sure why.
	//			If I upload a temp image and delete it afterwards, it's all good.
	VkuImage_t Temp;
	Image_Upload(&Context, &Temp, "assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);

	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1], "assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1_NORMAL], "assets/asteroid1_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2], "assets/asteroid2.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2_NORMAL], "assets/asteroid2_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3], "assets/asteroid3.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3_NORMAL], "assets/asteroid3_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4], "assets/asteroid4.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4_NORMAL], "assets/asteroid4_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_CROSSHAIR], "assets/crosshair.qoi", IMAGE_NONE);

	vkuDestroyImageBuffer(&Context, &Temp);

	GenNebulaVolume(&Context, &Textures[TEXTURE_VOLUME]);

	// Create primary pipeline
	CreatePipeline();

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

	volatile vec3 Zero=Vec3(0.0f, 0.0f, 0.0f);
	ParticleSystem_AddEmitter(&particleSystem, Zero, Zero, Zero, 0.0f, 1000, true, NULL);

	Font_Init(&Fnt);

	UI_Init(&UI, Vec2(0.0f, 0.0f), Vec2((float)renderWidth, (float)renderHeight));

#ifndef ANDROID
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
#endif

	UI_AddSprite(&UI, Vec2((float)renderWidth/2.0f, (float)renderHeight/2.0f), Vec2(50.0f, 50.0f), Vec3(1.0f, 1.0f, 1.0f), &Textures[TEXTURE_CROSSHAIR], 0.0f);

	cursorID=UI_AddCursor(&UI, Vec2(0.0f, 0.0f), 16.0f, Vec3(1.0f, 1.0f, 1.0f));

	// Other per-frame data
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		// Create needed fence and semaphores for rendering
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(Context.Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &perFrame[i].frameFence);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].presentCompleteSemaphore);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(Context.Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &perFrame[i].renderCompleteSemaphore);

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
		}, VK_NULL_HANDLE, &perFrame[i].descriptorPool);

		// Create per-frame command pools
		vkCreateCommandPool(Context.Device, &(VkCommandPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags=0,
			.queueFamilyIndex=Context.QueueFamilyIndex,
		}, VK_NULL_HANDLE, &perFrame[i].commandPool);

		// Allocate the command buffers we will be rendering into
		vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool=perFrame[i].commandPool,
			.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount=1,
		}, &perFrame[i].commandBuffer);
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

#if 0
	// Initialize the network API (mainly for winsock)
	Network_Init();

	// Create a new socket
	clientSocket=Network_CreateSocket();

	if(clientSocket==-1)
		return false;

	// Send connect magic to initiate connection
	uint32_t Magic=CONNECT_PACKETMAGIC;
	if(!Network_SocketSend(clientSocket, (uint8_t *)&Magic, sizeof(uint32_t), serverAddress, serverPort))
		return false;

	double Timeout=GetClock()+5.0; // Current time +5 seconds
	bool Response=false;

	while(!Response)
	{
		uint32_t Address=0;
		uint16_t Port=0;
		NetworkPacket_t ResponsePacket;

		memset(&ResponsePacket, 0, sizeof(NetworkPacket_t));

		if(Network_SocketReceive(clientSocket, (uint8_t *)&ResponsePacket, sizeof(NetworkPacket_t), &Address, &Port)>0)
		{
			if(ResponsePacket.PacketMagic==CONNECT_PACKETMAGIC)
			{
				DBGPRINTF(DEBUG_INFO, "Response from server - ID: %d Seed: %d Port: %d Address: 0x%X Port: %d\n",
						  ResponsePacket.ClientID,
						  ResponsePacket.Connect.Seed,
						  ResponsePacket.Connect.Port,
						  Address,
						  Port);

				RandomSeed(ResponsePacket.Connect.Seed);
				clientID=ResponsePacket.ClientID;
				Response=true;
			}
		}

		if(GetClock()>Timeout)
		{
			DBGPRINTF("Connection timed out...\n");
			Network_SocketClose(clientSocket);
			clientSocket=-1;
			break;
		}
	}

	Thread_Init(&ThreadNetUpdate);
	Thread_Start(&ThreadNetUpdate);
	Thread_AddJob(&ThreadNetUpdate, NetUpdate, NULL);
#endif

	if(!Zone_VerifyHeap(Zone))
		exit(-1);

	return true;
}

// Rebuild Vulkan swapchain and related data
void RecreateSwapchain(void)
{
	if(Context.Device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
	{
		// Wait for the device to complete any pending work
		vkDeviceWaitIdle(Context.Device);

		// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
		// This is basically just the swapchain, framebuffers, and depth buffer.

		// Swapchain, framebuffer, and depth buffer destruction
		vkuDestroyImageBuffer(&Context, &colorImage[0]);
		vkuDestroyImageBuffer(&Context, &colorResolve[0]);
		vkuDestroyImageBuffer(&Context, &colorBlur[0]);
		vkuDestroyImageBuffer(&Context, &colorTemp[0]);
		vkuDestroyImageBuffer(&Context, &depthImage[0]);

		vkDestroyFramebuffer(Context.Device, Framebuffer[0], VK_NULL_HANDLE);

		if(isVR)
		{
			vkuDestroyImageBuffer(&Context, &colorImage[1]);
			vkuDestroyImageBuffer(&Context, &colorResolve[1]);
			vkuDestroyImageBuffer(&Context, &colorBlur[1]);
			vkuDestroyImageBuffer(&Context, &colorTemp[1]);
			vkuDestroyImageBuffer(&Context, &depthImage[1]);

			vkDestroyFramebuffer(Context.Device, Framebuffer[1], VK_NULL_HANDLE);
		}

		// Destroy the swapchain
		vkuDestroySwapchain(&Context, &Swapchain);

		// Recreate the swapchain
		vkuCreateSwapchain(&Context, &Swapchain, VK_TRUE);

		renderWidth=Swapchain.Extent.width;
		renderHeight=Swapchain.Extent.height;

		UI.Size.x=(float)Swapchain.Extent.width;
		UI.Size.y=(float)Swapchain.Extent.height;

		// Recreate the framebuffer
		CreateFramebuffers(0);
		CreateCompositeFramebuffers(0);

		if(isVR)
		{
			CreateFramebuffers(1);
			CreateCompositeFramebuffers(1);
		}
	}
}

// Destroy call from system main
void Destroy(void)
{
	vkDeviceWaitIdle(Context.Device);

#if 0
	NetUpdate_Run=false;
	Thread_Destroy(&ThreadNetUpdate);

	// Send disconnect message to server and close/destroy network stuff
	if(clientSocket!=-1)
	{
		Network_SocketSend(clientSocket, (uint8_t *)&(NetworkPacket_t)
		{
			.PacketMagic=DISCONNECT_PACKETMAGIC, .ClientID=clientID
		}, sizeof(NetworkPacket_t), serverAddress, serverPort);
		Network_SocketClose(clientSocket);
	}

	Network_Destroy();
#endif

	Audio_Destroy();

#ifndef ANDROID
	Music_Destroy();
#endif

	Zone_Free(Zone, Sounds[SOUND_PEW1].Data);
	Zone_Free(Zone, Sounds[SOUND_PEW2].Data);
	Zone_Free(Zone, Sounds[SOUND_PEW3].Data);
	Zone_Free(Zone, Sounds[SOUND_STONE1].Data);
	Zone_Free(Zone, Sounds[SOUND_STONE2].Data);
	Zone_Free(Zone, Sounds[SOUND_STONE3].Data);
	Zone_Free(Zone, Sounds[SOUND_CRASH].Data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE1].Data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE2].Data);
	Zone_Free(Zone, Sounds[SOUND_EXPLODE3].Data);

#ifndef ANDROID
	if(isVR)
		VR_Destroy(&xrContext);
#endif

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
	ParticleSystem_Destroy(&particleSystem);
	//////////

	// Skybox destruction
	DestroySkybox();
	//////////

	// UI destruction
	UI_Destroy(&UI);
	//////////

	// Font destruction
	Font_Destroy(&Fnt);
	//////////

	// Asteroid instance buffer destruction
	vkUnmapMemory(Context.Device, asteroidInstance.DeviceMemory);
	vkuDestroyBuffer(&Context, &asteroidInstance);
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
	vkDestroyDescriptorSetLayout(Context.Device, volumeDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, volumePipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, volumePipelineLayout, VK_NULL_HANDLE);
	//////////

	// Main render destruction
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkUnmapMemory(Context.Device, perFrame[i].mainUBOBuffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &perFrame[i].mainUBOBuffer[0]);

		vkUnmapMemory(Context.Device, perFrame[i].mainUBOBuffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &perFrame[i].mainUBOBuffer[1]);
	}

	vkDestroyDescriptorSetLayout(Context.Device, mainDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(Context.Device, RenderPass, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, mainPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, mainPipelineLayout, VK_NULL_HANDLE);
	//////////

	// Swapchain, framebuffer, and depth buffer destruction
	vkuDestroyImageBuffer(&Context, &colorImage[0]);
	vkuDestroyImageBuffer(&Context, &colorResolve[0]);
	vkuDestroyImageBuffer(&Context, &colorBlur[0]);
	vkuDestroyImageBuffer(&Context, &colorTemp[0]);
	vkuDestroyImageBuffer(&Context, &depthImage[0]);

	vkDestroyFramebuffer(Context.Device, Framebuffer[0], VK_NULL_HANDLE);

	if(isVR)
	{
		vkuDestroyImageBuffer(&Context, &colorImage[1]);
		vkuDestroyImageBuffer(&Context, &colorResolve[1]);
		vkuDestroyImageBuffer(&Context, &colorBlur[1]);
		vkuDestroyImageBuffer(&Context, &colorTemp[1]);
		vkuDestroyImageBuffer(&Context, &depthImage[1]);

		vkDestroyFramebuffer(Context.Device, Framebuffer[1], VK_NULL_HANDLE);
	}

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		// Destroy sync objects
		vkDestroyFence(Context.Device, perFrame[i].frameFence, VK_NULL_HANDLE);

		vkDestroySemaphore(Context.Device, perFrame[i].presentCompleteSemaphore, VK_NULL_HANDLE);
		vkDestroySemaphore(Context.Device, perFrame[i].renderCompleteSemaphore, VK_NULL_HANDLE);

		// Destroy main thread descriptor pools
		vkDestroyDescriptorPool(Context.Device, perFrame[i].descriptorPool, VK_NULL_HANDLE);

		// Destroy command pools
		vkDestroyCommandPool(Context.Device, perFrame[i].commandPool, VK_NULL_HANDLE);
	}

	vkuDestroySwapchain(&Context, &Swapchain);
	//////////

	DBGPRINTF(DEBUG_INFO, "Remaining Vulkan memory blocks:\n");
	vkuMem_Print(VkZone);
	vkuMem_Destroy(&Context, VkZone);
}
