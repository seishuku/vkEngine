#ifndef __PERFRAME_H__
#define __PERFRAME_H__

#include "skybox.h"

typedef struct
{
	matrix HMD;
	matrix projection;
	matrix modelview;
	matrix light_mvp;
	vec4 light_color;
	vec4 light_direction;
} UBO_t;

typedef struct
{
	// Thresholding shader per-frame data
	VkuDescriptorSet_t ThresholdDescriptorSet;
	//////

	// Gaussian blur shader per-frame data
	VkuDescriptorSet_t GaussianVDescriptorSet;
	VkuDescriptorSet_t GaussianHDescriptorSet;
	//////

	// Compositing shader per-frame data
	VkuDescriptorSet_t CompositeDescriptorSet;
	VkFramebuffer CompositeFramebuffer;
	//////

	// Volume render per-frame, per-eye data
	VkuDescriptorSet_t VolumeDescriptorSet;
	//////

	// Skybox per-frame, per-eye data
	Skybox_UBO_t *Skybox_UBO[2];
	VkuBuffer_t Skybox_UBO_Buffer[2];
	VkuDescriptorSet_t SkyboxDescriptorSet;
	//////

	// Main render per-frame, per-eye data
	UBO_t *Main_UBO[2];
	VkuBuffer_t uboBuffer[2];
	VkuDescriptorSet_t DescriptorSet;
	//////

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
