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
	bool isDone;

	UI_t UI;

	ThreadWorker_t workerThread;
} LoadingScreen_t;

bool LoadingScreenInit(LoadingScreen_t *loadingScreen, uint32_t numItems);
void DestroyLoadingScreen(LoadingScreen_t *loadingScreen);

#endif
