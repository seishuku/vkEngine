#ifndef __LOADINGSCREEN_H__
#define __LOADINGSCREEN_H__

#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "system/threads.h"
#include "image/image.h"
#include "ui/ui.h"

typedef struct
{
	VkRenderPass renderPass;
	VkFramebuffer framebuffer[VKU_MAX_FRAME_COUNT];

	VkFence frameFence;
	VkSemaphore completeSemaphore;

	VkDescriptorPool descriptorPool;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;

	VkuImage_t logo;

	uint32_t numItems;
	uint32_t currentCount;
	uint32_t loadingGraphID;

	UI_t UI;
} LoadingScreen_t;

bool LoadingScreenInit(LoadingScreen_t *loadingScreen, uint32_t numItems);
void LoadingScreenAdvance(LoadingScreen_t *loadingScreen);
void DestroyLoadingScreen(LoadingScreen_t *loadingScreen);

#endif
