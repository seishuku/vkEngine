#ifndef __PERFRAME_H__
#define __PERFRAME_H__

#include "vulkan/vulkan.h"
#include "math/math.h"
#include "skybox.h"

typedef struct
{
	matrix HMD;
	matrix projection;
	matrix modelview;
	matrix light_mvp;
	vec4 light_color;
	vec4 light_direction;
} Main_UBO_t;

typedef struct
{
	// Skybox per-frame, per-eye data
	Skybox_UBO_t *Skybox_UBO[2];
	VkuBuffer_t Skybox_UBO_Buffer[2];
	//////

	// Main render per-frame, per-eye data
	Main_UBO_t *Main_UBO[2];
	VkuBuffer_t Main_UBO_Buffer[2];
	//////

	VkFramebuffer CompositeFramebuffer;

	// Descriptor pool
	VkDescriptorPool DescriptorPool;

	// Command buffer
	VkCommandPool CommandPool;
	VkCommandBuffer CommandBuffer;

	// Fences/semaphores
	VkFence FrameFence;
	VkSemaphore PresentCompleteSemaphore;
	VkSemaphore RenderCompleteSemaphore;
} PerFrame_t;

extern PerFrame_t PerFrame[VKU_MAX_FRAME_COUNT];

#endif
