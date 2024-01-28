#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdalign.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
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

// Vulkan memory allocator zone
VkuMemZone_t *vkZone;

// Camera data
Camera_t camera;
matrix modelView, projection[2], headPose;

// extern timing data from system main
extern float fps, fTimeStep, fTime;

// Main particle system struct
ParticleSystem_t particleSystem;

// 3D Model data
BModel_t models[NUM_MODELS];

// Texture images
VkuImage_t textures[NUM_TEXTURES];

// Sound sample data
Sample_t sounds[NUM_SOUNDS];

// Vulkan swapchain helper struct
VkuSwapchain_t swapchain;

// Multisample anti-alias sample count
VkSampleCountFlags MSAA=VK_SAMPLE_COUNT_4_BIT;

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
#define NUM_ASTEROIDS 1000
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

#define NUM_THREADS 3
ThreadData_t threadData[NUM_THREADS];
ThreadWorker_t thread[NUM_THREADS], threadPhysics/*, threadNetUpdate*/;

//pthread_barrier_t threadBarrier, physicsThreadBarrier;

mtx_t threadMutex;
cnd_t threadCondition;
_Atomic(int32_t) threadBarrier;

mtx_t physicsThreadMutex;
cnd_t physicsThreadCondition;
_Atomic(int32_t) physicsThreadBarrier;
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
	vec3 position, velocity;
	vec3 forward, up;
} NetCamera_t;

// Connect data when connecting to server
typedef struct
{
	uint32_t seed;
	uint16_t port;
} NetConnect_t;

// Overall data network packet
typedef struct
{
	uint32_t packetMagic;
	uint32_t clientID;
	union
	{
		NetConnect_t connect;
		NetCamera_t camera;
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

Console_t console;

XrPosef leftHand, rightHand;
float leftTrigger, rightTrigger;
float leftGrip, rightGrip;
vec2 leftThumbstick, rightThumbstick;

void RecreateSwapchain(void);

// Create functions for creating render data for asteroids
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
	vkuTransitionLayout(commandBuffer, depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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

bool CreatePipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
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
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
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
		.pSubpasses=(VkSubpassDescription[])
		{
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
		},
		.dependencyCount=3,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
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
			{
				.srcSubpass=0,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		}
	}, 0, &renderPass);

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(vkContext.device, perFrame[i].mainUBOBuffer[0].deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[0]);

		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(vkContext.device, perFrame[i].mainUBOBuffer[1].deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[1]);
	}

	vkuInitDescriptorSet(&mainDescriptorSet, &vkContext);

	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

	vkuAssembleDescriptorSetLayout(&mainDescriptorSet);

	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&mainDescriptorSet.descriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &mainPipelineLayout);

	vkuInitPipeline(&mainPipeline, &vkContext);

	vkuPipeline_SetPipelineLayout(&mainPipeline, mainPipelineLayout);
	vkuPipeline_SetRenderPass(&mainPipeline, renderPass);

	mainPipeline.depthTest=VK_TRUE;
	mainPipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	mainPipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	mainPipeline.rasterizationSamples=MSAA;

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
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		if(!perFrame[i].asteroidInstance.buffer)
		{
			vkuCreateHostBuffer(&vkContext, &perFrame[i].asteroidInstance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
			vkMapMemory(vkContext.device, perFrame[i].asteroidInstance.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].asteroidInstancePtr);
		}
	}

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		vec3 randomDirection=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(&randomDirection);

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
	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
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

	vkuInitPipeline(&spherePipeline, &vkContext);

	vkuPipeline_SetPipelineLayout(&spherePipeline, spherePipelineLayout);
	vkuPipeline_SetRenderPass(&spherePipeline, renderPass);

	spherePipeline.depthTest=VK_TRUE;
	spherePipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	spherePipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	spherePipeline.rasterizationSamples=MSAA;
	//spherePipeline.polygonMode=VK_POLYGON_MODE_LINE;

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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline);
	vkCmdPushConstants(commandBuffer, spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
//////

// Debug line pipeline
bool CreateLinePipeline(void)
{
	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
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

	vkuInitPipeline(&linePipeline, &vkContext);

	vkuPipeline_SetPipelineLayout(&linePipeline, linePipelineLayout);
	vkuPipeline_SetRenderPass(&linePipeline, renderPass);

	linePipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	linePipeline.depthTest=VK_TRUE;
	linePipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	linePipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	linePipeline.rasterizationSamples=MSAA;

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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline);
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

	vec4 position=Vec4_Vec3(player.position, 1.0f);
	vec4 forward=Vec4_Vec3(player.forward, 1.0f);
	vec4 up=Vec4_Vec3(player.up, 1.0f);
	vec4 right=Vec4_Vec3(player.right, 1.0f);

	linePC.mvp=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->projection);
	linePC.start=position;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline);

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
	local=MatrixMult(local, MatrixInverse(MatrixLookAt(player.position, Vec3_Addv(player.position, player.forward), player.up)));

	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	spherePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);

	spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline);
	vkCmdPushConstants(commandBuffer, spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
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
				.queueFamilyIndex=vkContext.queueFamilyIndex,
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

float raySphereIntersect(vec3 rayOrigin, vec3 rayDirection, vec3 sphereCenter, float sphereRadius)
{
	vec3 oc=Vec3_Subv(rayOrigin, sphereCenter);
	float a=Vec3_Dot(rayDirection, rayDirection);
	float b=2.0f*Vec3_Dot(oc, rayDirection);
	float c=Vec3_Dot(oc, oc)-sphereRadius*sphereRadius;
	float discriminant=b*b-4*a*c;

	if(discriminant<0.0f)
		return -1.0f;
	else
		return (-b-sqrtf(discriminant))/(2.0f*a);
}

// Asteroids render pass thread
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

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipeline);

	// Draw the models
	vkCmdBindVertexBuffers(data->perFrame[data->index].secCommandBuffer[data->eye], 1, 1, &perFrame[data->index].asteroidInstance.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 0, textures[2*i+0].sampler, textures[2*i+0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 1, textures[2*i+1].sampler, textures[2*i+1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 3, perFrame[data->index].mainUBOBuffer[data->eye].buffer, 0, VK_WHOLE_SIZE);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 4, perFrame[data->index].skyboxUBOBuffer[data->eye].buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&mainDescriptorSet, data->perFrame[data->index].descriptorPool[data->eye]);

		vkCmdBindDescriptorSets(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipelineLayout, 0, 1, &mainDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

		matrix local=MatrixIdentity();
		//local=MatrixMult(local, MatrixRotate(fTime, 1.0f, 0.0f, 0.0f));
		//local=MatrixMult(local, MatrixRotate(fTime, 0.0f, 1.0f, 0.0f));

		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], mainPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &local);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(data->perFrame[data->index].secCommandBuffer[data->eye], 0, 1, &models[i].vertexBuffer.buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<models[i].numMesh;j++)
		{
			vkCmdBindIndexBuffer(data->perFrame[data->index].secCommandBuffer[data->eye], models[i].mesh[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(data->perFrame[data->index].secCommandBuffer[data->eye], models[i].mesh[j].numFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}

#if 0
	if(camera.shift)
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

	// Draw VR "hands"
	if(isVR)
	{
		struct
		{
			matrix mvp;
			vec4 color;
		} spherePC;

		spherePC.color=Vec4(1.0f, 1.0f, 1.0f, 1.0f);

		vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline);

		vec3 leftPos=Vec3(leftHand.position.x, leftHand.position.y, leftHand.position.z);
		vec4 leftRot=Vec4(leftHand.orientation.x, leftHand.orientation.y, leftHand.orientation.z, leftHand.orientation.w);
		matrix local=MatrixMult(MatrixMult(QuatMatrix(leftRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 60, 1, 0, 0);

		vec3 rightPos=Vec3(rightHand.position.x, rightHand.position.y, rightHand.position.z);
		vec4 rightRot=Vec4(rightHand.orientation.x, rightHand.orientation.y, rightHand.orientation.z, rightHand.orientation.w);
		local=MatrixMult(MatrixMult(QuatMatrix(rightRot), MatrixScale(0.1f, 0.1f, 0.1f)), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		spherePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 60, 1, 0, 0);

		struct
		{
			matrix mvp;
			vec4 color;
			vec4 Verts[2];
		} LinePC;

		LinePC.Verts[0]=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		LinePC.Verts[1]=Vec4(0.0f, 0.0f, -1.0f, 1.0f);

		vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline);

		LinePC.color=Vec4(leftTrigger*100.0f+1.0f, 1.0f, leftGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatMatrix(leftRot), MatrixTranslatev(leftPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		LinePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinePC), &LinePC);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 2, 1, 0, 0);

		LinePC.color=Vec4(rightTrigger*100.0f+1.0f, 1.0f, rightGrip*100.0f+1.0f, 1.0f);

		local=MatrixMult(QuatMatrix(rightRot), MatrixTranslatev(rightPos));
		local=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->HMD);
		LinePC.mvp=MatrixMult(local, perFrame[data->index].mainUBO[data->eye]->projection);

		vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], linePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(LinePC), &LinePC);
		vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 2, 1, 0, 0);
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

	//pthread_barrier_wait(&threadBarrier);
	mtx_lock(&threadMutex);
	atomic_fetch_sub(&threadBarrier, 1);
	cnd_signal(&threadCondition);
	mtx_unlock(&threadMutex);
}

// Skybox render pass thread
void Thread_Skybox(void *arg)
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

	vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxDescriptorSet, 0, perFrame[data->index].mainUBOBuffer[data->eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxDescriptorSet, 1, perFrame[data->index].skyboxUBOBuffer[data->eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&skyboxDescriptorSet, data->perFrame[data->index].descriptorPool[data->eye]);

	vkCmdBindDescriptorSets(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1, &skyboxDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 60, 1, 0, 0);

	vkEndCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye]);

	//pthread_barrier_wait(&threadBarrier);
	mtx_lock(&threadMutex);
	atomic_fetch_sub(&threadBarrier, 1);
	cnd_signal(&threadCondition);
	mtx_unlock(&threadMutex);
}

// Particles render pass thread, also has volumetric rendering
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

#if 0
	static uint32_t uFrame=0;
	uFrame++;

	vkCmdBindPipeline(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipeline);

	vkCmdPushConstants(data->perFrame[data->index].secCommandBuffer[data->eye], volumePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &uFrame);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumeDescriptorSet, 0, &textures[TEXTURE_VOLUME]);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumeDescriptorSet, 1, perFrame[data->index].mainUBOBuffer[data->eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumeDescriptorSet, 2, &depthImage[data->eye]);
	vkuAllocateUpdateDescriptorSet(&volumeDescriptorSet, data->perFrame[data->index].descriptorPool[data->eye]);
	vkCmdBindDescriptorSets(data->perFrame[data->index].secCommandBuffer[data->eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipelineLayout, 0, 1, &volumeDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(data->perFrame[data->index].secCommandBuffer[data->eye], 36, 1, 0, 0);
#endif

	vkEndCommandBuffer(data->perFrame[data->index].secCommandBuffer[data->eye]);

	//pthread_barrier_wait(&threadBarrier);
	mtx_lock(&threadMutex);
	atomic_fetch_sub(&threadBarrier, 1);
	cnd_signal(&threadCondition);
	mtx_unlock(&threadMutex);
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
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}}, {{{ 0.0f, 0 }}} },
		.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth, renderHeight } },
	}, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	// Set per thread data and add the job to that worker thread
	threadData[0].index=index;
	threadData[0].eye=eye;
	Thread_AddJob(&thread[0], Thread_Main, (void *)&threadData[0]);

	threadData[1].index=index;
	threadData[1].eye=eye;
	Thread_AddJob(&thread[1], Thread_Skybox, (void *)&threadData[1]);

	threadData[2].index=index;
	threadData[2].eye=eye;
	Thread_AddJob(&thread[2], Thread_Particles, (void *)&threadData[2]);

	vkBeginCommandBuffer(perFrame[index].secCommandBuffer[eye], &(VkCommandBufferBeginInfo)
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
			.framebuffer=framebuffer[eye],
			//.subpass=1
		}
	});

	vkCmdSetViewport(perFrame[index].secCommandBuffer[eye], 0, 1, &(VkViewport) { 0.0f, 0, (float)renderWidth, (float)renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[index].secCommandBuffer[eye], 0, 1, &(VkRect2D) { { 0, 0 }, { renderWidth, renderHeight } });

#if 1
#ifndef ANDROID
//	vkuTransitionLayout(perFrame[index].secCommandBuffer[eye], depthImage[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	// Volumetric rendering is broken on Android when rendering at half resolution for some reason.
	static uint32_t uFrame=0;

	uFrame++;

	vkCmdBindPipeline(perFrame[index].secCommandBuffer[eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipeline);

	vkCmdPushConstants(perFrame[index].secCommandBuffer[eye], volumePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t), &uFrame);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumeDescriptorSet, 0, textures[TEXTURE_VOLUME].sampler, textures[TEXTURE_VOLUME].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumeDescriptorSet, 1, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumeDescriptorSet, 2, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&volumeDescriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].secCommandBuffer[eye], VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipelineLayout, 0, 1, &volumeDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(perFrame[index].secCommandBuffer[eye], 36, 1, 0, 0);
#endif
#endif

	vkEndCommandBuffer(perFrame[index].secCommandBuffer[eye]);

	mtx_lock(&threadMutex);
	while(atomic_load(&threadBarrier))
		cnd_wait(&threadCondition, &threadMutex);
	mtx_unlock(&threadMutex);
	atomic_store(&threadBarrier, NUM_THREADS);

	//pthread_barrier_wait(&threadBarrier);

	// Execute the secondary command buffers from the threads
	vkCmdExecuteCommands(perFrame[index].commandBuffer, 3, (VkCommandBuffer[])
	{
		threadData[0].perFrame[index].secCommandBuffer[eye],
		threadData[1].perFrame[index].secCommandBuffer[eye],
		threadData[2].perFrame[index].secCommandBuffer[eye],
		perFrame[index].secCommandBuffer[eye]
	});

	//vkCmdNextSubpass(perFrame[index].commandBuffer, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

	//vkCmdExecuteCommands(perFrame[index].commandBuffer, 1, (VkCommandBuffer[])
	//{
	//	perFrame[index].secCommandBuffer[eye]
	//});

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

vec4 calculatePlane(vec3 p, vec3 norm)
{
	Vec3_Normalize(&norm);
	return Vec4(norm.x, norm.y, norm.z, Vec3_Dot(norm, p));
}

void calculateFrustumPlanes(const Camera_t camera, vec4 *frustumPlanes)
{
	const float fov=deg2rad(90.0f);
	const float nearPlane=0.01f;
	const float farPlane=100000.0f;
	const float aspect=(float)renderWidth/renderHeight;

	const float half_v=farPlane*tanf(fov*0.5f);
	const float half_h=half_v*aspect;

	vec3 forward_far=Vec3_Muls(camera.forward, farPlane);

	// Top, bottom, right, left, far, near
	frustumPlanes[0]=calculatePlane(Vec3_Addv(camera.position, Vec3_Muls(camera.forward, nearPlane)), camera.forward);
	frustumPlanes[1]=calculatePlane(Vec3_Addv(camera.position, forward_far), Vec3_Muls(camera.forward, -1.0f));

	frustumPlanes[2]=calculatePlane(camera.position, Vec3_Cross(camera.up, Vec3_Addv(forward_far, Vec3_Muls(camera.right, half_h))));
	frustumPlanes[3]=calculatePlane(camera.position, Vec3_Cross(Vec3_Subv(forward_far, Vec3_Muls(camera.right, half_h)), camera.up));
	frustumPlanes[4]=calculatePlane(camera.position, Vec3_Cross(camera.right, Vec3_Subv(forward_far, Vec3_Muls(camera.up, half_v))));
	frustumPlanes[5]=calculatePlane(camera.position, Vec3_Cross(Vec3_Addv(forward_far, Vec3_Muls(camera.up, half_v)), camera.right));
}

// Runs anything physics related
void Thread_Physics(void *arg)
{
	uint32_t index=*((uint32_t *)arg);

	//CvWriteFlag(flagSeries, "physics start");

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
		PhysicsCameraToSphereCollisionResponse(&camera, &asteroids[i]);

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
			if(emitter->particles[j].ID!=emitter->ID||emitter->particles[j].life>0.0f)
				PhysicsParticleToSphereCollisionResponse(&emitter->particles[j], &asteroids[i]);
		}

#if 0
		if(camera.shift)
		{
			float distance=raySphereIntersect(camera.position, camera.forward, asteroids[i].position, asteroids[i].radius);

			if(distance>0.0f)
			{
				Particle_t particle;
				particle.position=Vec3_Addv(camera.position, Vec3_Muls(camera.forward, distance));
				particle.velocity=Vec3_Muls(camera.forward, 300.0f);
				PhysicsParticleToSphereCollisionResponse(&particle, &asteroids[i]);
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

	// Update camera and modelview matrix
	modelView=CameraUpdate(&camera, fTimeStep);
	//////

	// Update instance matrix data
	//matrix *data=NULL;
	//vkMapMemory(Context.device, asteroidInstance.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&data);

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		float radiusScale=0.666667f;

		matrix local=MatrixScale(asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale, asteroids[i].radius*radiusScale);
		local=MatrixMult(local, QuatMatrix(asteroids[i].orientation));
		perFrame[index].asteroidInstancePtr[i]=MatrixMult(local, MatrixTranslatev(asteroids[i].position));
	}

	//vkUnmapMemory(Context.device, asteroidInstance.deviceMemory);
	//////

#if 0
	// Network status packet
	if(clientSocket!=-1)
	{
		NetworkPacket_t StatusPacket;

		memset(&StatusPacket, 0, sizeof(NetworkPacket_t));

		StatusPacket.packetMagic=STATUS_PACKETMAGIC;
		StatusPacket.clientID=clientID;

		StatusPacket.camera.position=camera.position;
		StatusPacket.camera.velocity=camera.velocity;
		StatusPacket.camera.forward=camera.forward;
		StatusPacket.camera.up=camera.up;

		Network_SocketSend(clientSocket, (uint8_t *)&StatusPacket, sizeof(NetworkPacket_t), serverAddress, serverPort);
	}
	//////
#endif

	//CvWriteFlag(flagSeries, "physics end");

	// Barrier now that we're done here
	//pthread_barrier_wait(&physicsThreadBarrier);
//	mtx_lock(&physicsThreadMutex);
	atomic_fetch_sub(&physicsThreadBarrier, 1);
//	cnd_signal(&physicsThreadCondition);
//	mtx_unlock(&physicsThreadMutex);
}

#if 0
pthread_t UpdateThread;
bool NetUpdate_Run=true;
uint8_t NetBuffer[32767]={ 0 };

void NetUpdate(void *arg)
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
		uint16_t port=0;

		Network_SocketReceive(clientSocket, NetBuffer, sizeof(NetBuffer), &Address, &port);

		memcpy(&Magic, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

		if(Magic==STATUS_PACKETMAGIC)
		{
			memcpy(&connectedClients, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<connectedClients;i++)
			{
				uint32_t clientID=0;

				memcpy(&clientID, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

				memcpy(&netCameras[clientID].position, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].velocity, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].forward, pBuffer, sizeof(float)*3);	pBuffer+=sizeof(float)*3;
				memcpy(&netCameras[clientID].up, pBuffer, sizeof(float)*3);			pBuffer+=sizeof(float)*3;

				//DBGPRINTF(DEBUG_INFO, "\033[%d;0H\033[KID %d Pos: %0.1f %0.1f %0.1f", clientID+1, clientID, NetCameras[clientID].position.x, NetCameras[clientID].position.y, NetCameras[clientID].position.z);
			}
		}
		else if(Magic==FIELD_PACKETMAGIC)
		{
			uint32_t asteroidCount=NUM_ASTEROIDS;

			memcpy(&asteroidCount, pBuffer, sizeof(uint32_t));	pBuffer+=sizeof(uint32_t);

			for(uint32_t i=0;i<asteroidCount;i++)
			{
				memcpy(&asteroids[i].position, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
				memcpy(&asteroids[i].velocity, pBuffer, sizeof(vec3));	pBuffer+=sizeof(vec3);
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

	//CvWriteFlag(flagSeries, "frame start");

	static uint32_t cleanUpCount=0;

	if(cleanUpCount++>100)
	{
		cleanUpCount=0;

		for(uint32_t i=1;i<List_GetCount(&particleSystem.emitters);i++)
		{
			ParticleEmitter_t *emitter=List_GetPointer(&particleSystem.emitters, i);
			bool isActive=false;

			for(uint32_t j=0;j<emitter->numParticles;j++)
			{
				if(emitter->particles[i].life>0.0f)
				{
					isActive=true;
					break;
				}
			}

			if(!isActive)
			{
				DBGPRINTF(DEBUG_WARNING, "REMOVING UNUSED EMMITER #%d\n", emitter->ID);
				ParticleSystem_DeleteEmitter(&particleSystem, emitter->ID);
			}
		}
	}

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
			vec3 direction=Matrix3x3MultVec3(Vec3(0.0f, 0.0f, -1.0f), MatrixMult(QuatMatrix(rightOrientation), MatrixInverse(modelView)));

			FireParticleEmitter(Vec3_Addv(camera.position, Vec3_Muls(direction, camera.radius)), direction);
			Audio_PlaySample(&sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &camera.position);
		}

		if(rightTrigger<0.25f&&!rightTriggerOnce)
			rightTriggerOnce=true;
	}
	else
	// Handle non-VR frame start
	{
		VkResult Result=vkAcquireNextImageKHR(vkContext.device, swapchain.swapchain, UINT64_MAX, perFrame[index].presentCompleteSemaphore, VK_NULL_HANDLE, &imageIndex);

		if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		{
			DBGPRINTF(DEBUG_WARNING, "Swapchain out of date... Rebuilding.\n");
			RecreateSwapchain();
			return;
		}

		projection[0]=MatrixInfPerspective(90.0f, (float)renderWidth/renderHeight, 0.01f);
		headPose=MatrixIdentity();
	}

	Thread_AddJob(&threadPhysics, Thread_Physics, (void *)&index);

	Console_Draw(&console);

	Audio_SetStreamVolume(0, UI_GetBarGraphValue(&UI, volumeID));

	Font_Print(&Fnt, 16.0f, renderWidth-400.0f, renderHeight-50.0f-16.0f, "Current track: %s", musicList[currentMusic].string);

	vkWaitForFences(vkContext.device, 1, &perFrame[index].frameFence, VK_TRUE, UINT64_MAX);

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

	// Wait for physics to finish before sumbitting frame
	//pthread_barrier_wait(&physicsThreadBarrier);

//	mtx_lock(&physicsThreadMutex);
	while(atomic_load(&physicsThreadBarrier));
//		cnd_wait(&physicsThreadCondition, &physicsThreadMutex);
//	mtx_unlock(&physicsThreadMutex);

	atomic_store(&physicsThreadBarrier, 1);

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

	vkQueueSubmit(vkContext.queue, 1, &SubmitInfo, perFrame[index].frameFence);

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
		VkResult Result=vkQueuePresentKHR(vkContext.queue, &(VkPresentInfoKHR)
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

void Console_CmdQuit(Console_t *Console, char *Param)
{
	isDone=true;
}

// Initialization call from system main
bool Init(void)
{
	RandomSeed(42069);

	//CvInitProvider(&CvDefaultProviderGuid, &provider);
	//CvCreateMarkerSeries(provider, "flag series", &flagSeries);

	// TODO: This is a hack, fix it proper.
	if(isVR)
		swapchain.numImages=xrContext.swapchain[0].numImages;

	Event_Add(EVENT_KEYDOWN, Event_KeyDown);
	Event_Add(EVENT_KEYUP, Event_KeyUp);
	Event_Add(EVENT_MOUSEDOWN, Event_MouseDown);
	Event_Add(EVENT_MOUSEUP, Event_MouseUp);
	Event_Add(EVENT_MOUSEMOVE, Event_Mouse);

	Console_Init(&console, 80, 25);
	Console_AddCommand(&console, "quit", Console_CmdQuit);

	vkZone=vkuMem_Init(&vkContext, (size_t)(vkContext.deviceProperties2.maxMemoryAllocationSize*0.8f));

	if(vkZone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Init: vkuMem_Init failed.\n");
		return false;
	}

	CameraInit(&camera, Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f));

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

	GenNebulaVolume(&vkContext, &textures[TEXTURE_VOLUME]);

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
			.queueFamilyIndex=vkContext.queueFamilyIndex,
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
	//pthread_barrier_init(&threadBarrier, NULL, NUM_THREADS+1);
	cnd_init(&threadCondition);
	mtx_init(&threadMutex, mtx_plain);
	atomic_store(&threadBarrier, NUM_THREADS);

	// Thread for physics, and sync barrier
	Thread_Init(&threadPhysics);
	Thread_Start(&threadPhysics);

	//pthread_barrier_init(&physicsThreadBarrier, NULL, 2);
	cnd_init(&physicsThreadCondition);
	mtx_init(&physicsThreadMutex, mtx_plain);
	atomic_store(&physicsThreadBarrier, 1);

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
		uint16_t port=0;
		NetworkPacket_t ResponsePacket;

		memset(&ResponsePacket, 0, sizeof(NetworkPacket_t));

		if(Network_SocketReceive(clientSocket, (uint8_t *)&ResponsePacket, sizeof(NetworkPacket_t), &Address, &port)>0)
		{
			if(ResponsePacket.packetMagic==CONNECT_PACKETMAGIC)
			{
				DBGPRINTF(DEBUG_INFO, "Response from server - ID: %d Seed: %d Port: %d Address: 0x%X Port: %d\n",
						  ResponsePacket.clientID,
						  ResponsePacket.connect.seed,
						  ResponsePacket.connect.port,
						  Address,
						  port);

				RandomSeed(ResponsePacket.connect.seed);
				clientID=ResponsePacket.clientID;
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

	Thread_Init(&threadNetUpdate);
	Thread_Start(&threadNetUpdate);
	Thread_AddJob(&threadNetUpdate, NetUpdate, NULL);
#endif

	if(!Zone_VerifyHeap(zone))
		exit(-1);

	return true;
}

// Rebuild Vulkan swapchain and related data
void RecreateSwapchain(void)
{
	cnd_broadcast(&physicsThreadCondition);

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

#if 0
	NetUpdate_Run=false;
	Thread_Destroy(&threadNetUpdate);

	// Send disconnect message to server and close/destroy network stuff
	if(clientSocket!=-1)
	{
		Network_SocketSend(clientSocket, (uint8_t *)&(NetworkPacket_t)
		{
			.packetMagic=DISCONNECT_PACKETMAGIC, .clientID=clientID
		}, sizeof(NetworkPacket_t), serverAddress, serverPort);
		Network_SocketClose(clientSocket);
	}

	Network_Destroy();
#endif

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

	// UI destruction
	UI_Destroy(&UI);
	//////////

	// Font destruction
	Font_Destroy(&Fnt);
	//////////

	// Asteroid instance buffer destruction
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkUnmapMemory(vkContext.device, perFrame[i].asteroidInstance.deviceMemory);
		vkuDestroyBuffer(&vkContext, &perFrame[i].asteroidInstance);
	}
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
	vkDestroyDescriptorSetLayout(vkContext.device, volumeDescriptorSet.descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(vkContext.device, volumePipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.device, volumePipelineLayout, VK_NULL_HANDLE);
	//////////

	// Main render destruction
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkUnmapMemory(vkContext.device, perFrame[i].mainUBOBuffer[0].deviceMemory);
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0]);

		vkUnmapMemory(vkContext.device, perFrame[i].mainUBOBuffer[1].deviceMemory);
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1]);
	}

	vkDestroyDescriptorSetLayout(vkContext.device, mainDescriptorSet.descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(vkContext.device, renderPass, VK_NULL_HANDLE);
	vkDestroyPipeline(vkContext.device, mainPipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.device, mainPipelineLayout, VK_NULL_HANDLE);
	//////////

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
	vkuMem_Print(vkZone);
	vkuMem_Destroy(&vkContext, vkZone);
}
