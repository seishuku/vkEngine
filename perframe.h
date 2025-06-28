#ifndef __PERFRAME_H__
#define __PERFRAME_H__

#include "vulkan/vulkan.h"
#include "math/math.h"
#include "pipelines/skybox.h"

typedef struct
{
	matrix HMD;
	matrix projection;
	matrix modelView;
	matrix lightMVP;
	vec4 lightColor;
	vec4 lightDirection;
} Main_UBO_t;

typedef struct
{
	// Skybox per-frame, per-eye data
	Skybox_UBO_t *skyboxUBO[2];
	VkuBuffer_t skyboxUBOBuffer[2];
	//////

	// Main render per-frame, per-eye data
	Main_UBO_t *mainUBO[2];
	VkuBuffer_t mainUBOBuffer[2];
	//////

	// Model instance per-frame data
	VkuBuffer_t asteroidInstance;
	matrix *asteroidInstancePtr;

	VkuBuffer_t fighterInstance;
	matrix *fighterInstancePtr;

	VkuBuffer_t cubeInstance;
	matrix *cubeInstancePtr;
	//////

	VkFramebuffer compositeFramebuffer[2];

	// Descriptor pool
	VkDescriptorPool descriptorPool;

	// Command buffer
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkCommandBuffer secCommandBuffer[2];

	// Fences/semaphores
	VkFence frameFence;
	VkSemaphore completeSemaphore;
} PerFrame_t;

#define FRAMES_IN_FLIGHT 3
extern PerFrame_t perFrame[FRAMES_IN_FLIGHT];

#endif
