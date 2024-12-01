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
	VkSemaphore presentCompleteSemaphore;
	VkSemaphore renderCompleteSemaphore;
} PerFrame_t;

extern PerFrame_t perFrame[VKU_MAX_FRAME_COUNT];

#endif
