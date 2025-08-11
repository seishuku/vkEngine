#include "vulkan/vulkan.h"
#include "system/system.h"
#include "system/threads.h"
#include "ui/ui.h"
#include "image/image.h"
#include "loadingscreen.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;

void LoadingScreenAdvance(LoadingScreen_t *loadingScreen)
{
	uint32_t imageIndex=0;

	UI_UpdateBarGraphValue(&loadingScreen->UI, loadingScreen->loadingGraphID, (float)(loadingScreen->currentCount++)/(loadingScreen->numItems-1));

	vkWaitForFences(vkContext.device, 1, &loadingScreen->frameFence, VK_TRUE, UINT64_MAX);

	VkResult Result=vkAcquireNextImageKHR(vkContext.device, swapchain.swapchain, UINT64_MAX, loadingScreen->completeSemaphore, VK_NULL_HANDLE, &imageIndex);

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		DBGPRINTF(DEBUG_WARNING, "Swapchain out of date or suboptimal...\n");

	vkResetFences(vkContext.device, 1, &loadingScreen->frameFence);
	vkResetCommandPool(vkContext.device, loadingScreen->commandPool, 0);
	vkResetDescriptorPool(vkContext.device, loadingScreen->descriptorPool, 0);

	vkBeginCommandBuffer(loadingScreen->commandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	vkCmdBeginRenderPass(loadingScreen->commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=loadingScreen->renderPass,
		.framebuffer=loadingScreen->framebuffer[imageIndex],
		.renderArea={ { 0, 0 }, { config.renderWidth, config.renderHeight } },
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}} },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(loadingScreen->commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)config.renderWidth, (float)config.renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(loadingScreen->commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth, config.renderHeight } });

	UI_Draw(&loadingScreen->UI, loadingScreen->commandBuffer, loadingScreen->descriptorPool, MatrixScale(1.0f, -1.0f, 1.0f), 999.0f);

	vkCmdEndRenderPass(loadingScreen->commandBuffer);

	vkEndCommandBuffer(loadingScreen->commandBuffer);

	vkQueueSubmit(vkContext.graphicsQueue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&loadingScreen->completeSemaphore,
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&swapchain.waitSemaphore[imageIndex],
		.commandBufferCount=1,
		.pCommandBuffers=&loadingScreen->commandBuffer,
	}, loadingScreen->frameFence);

	Result=vkQueuePresentKHR(vkContext.graphicsQueue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&swapchain.waitSemaphore[imageIndex],
		.swapchainCount=1,
		.pSwapchains=&swapchain.swapchain,
		.pImageIndices=&imageIndex,
	});

	if(Result==VK_ERROR_OUT_OF_DATE_KHR||Result==VK_SUBOPTIMAL_KHR)
		DBGPRINTF(DEBUG_WARNING, "vkQueuePresent out of date or suboptimal...\n");
}

bool LoadingScreenInit(LoadingScreen_t *loadingScreen, uint32_t numItems)
{
	loadingScreen->numItems=numItems;
	loadingScreen->currentCount=0;

	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=swapchain.surfaceFormat.format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
			},
		},
		.dependencyCount=1,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=0,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=0,
			}
		}
	}, 0, &loadingScreen->renderPass);

	if(!Image_Upload(&vkContext, &loadingScreen->logo, "assets/splash.qoi", IMAGE_BILINEAR))
		return false;

	if(!UI_Init(&loadingScreen->UI, Vec2(0, 0), Vec2(config.renderWidth, config.renderHeight), loadingScreen->renderPass))
		return false;

	UI_AddSprite(&loadingScreen->UI, Vec2(config.renderWidth/2, config.renderHeight/2), Vec2(config.renderWidth, config.renderHeight), Vec3b(1.0f), UI_CONTROL_VISIBLE, &loadingScreen->logo, 0.0f);
	loadingScreen->loadingGraphID=UI_AddBarGraph(&loadingScreen->UI, Vec2((config.renderWidth/2)-200, (config.renderHeight/2)-50), Vec2(400, 100), Vec3(0.1, 0.1, 0.6), UI_CONTROL_VISIBLE, "Loading...", UI_CONTROL_READONLY, UI_CONTROL_HORIZONTAL, 0.0f, 1.0f, 0.0f);

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=loadingScreen->renderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ swapchain.imageView[i] },
			.width=config.renderWidth,
			.height=config.renderHeight,
			.layers=1,
		}, 0, &loadingScreen->framebuffer[i]);
	}

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
	}, VK_NULL_HANDLE, &loadingScreen->descriptorPool);

	// Create per-frame command pools
	vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=0,
		.queueFamilyIndex=vkContext.graphicsQueueIndex,
	}, VK_NULL_HANDLE, &loadingScreen->commandPool);

	// Allocate the command buffers we will be rendering into
	vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=loadingScreen->commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &loadingScreen->commandBuffer);

	// Create needed fence and semaphore for rendering
	// Wait fence for command queue, to signal when we can submit commands again
	vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &loadingScreen->frameFence);

	// Semaphore for image presentation, to signal when we can present again
	vkCreateSemaphore(vkContext.device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &loadingScreen->completeSemaphore);

	LoadingScreenAdvance(loadingScreen);

	return true;
}

void DestroyLoadingScreen(LoadingScreen_t *loadingScreen)
{
	vkDeviceWaitIdle(vkContext.device);

	vkDestroyDescriptorPool(vkContext.device, loadingScreen->descriptorPool, VK_NULL_HANDLE);
	vkDestroyCommandPool(vkContext.device, loadingScreen->commandPool, VK_NULL_HANDLE);

	vkDestroyRenderPass(vkContext.device, loadingScreen->renderPass, VK_NULL_HANDLE);

	UI_Destroy(&loadingScreen->UI);
	
	vkuDestroyImageBuffer(&vkContext, &loadingScreen->logo);

	for(uint32_t i=0;i<swapchain.numImages;i++)
		vkDestroyFramebuffer(vkContext.device, loadingScreen->framebuffer[i], VK_NULL_HANDLE);

	vkDestroyFence(vkContext.device, loadingScreen->frameFence, VK_NULL_HANDLE);
	vkDestroySemaphore(vkContext.device, loadingScreen->completeSemaphore, VK_NULL_HANDLE);
}