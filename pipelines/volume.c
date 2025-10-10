#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../ui/ui.h"
#include "../assetmanager.h"
#include "../perframe.h"
#include "shadow.h"
#include "volume.h"

extern VkuContext_t vkContext;
extern VkuImage_t depthImage[2];
extern VkRenderPass renderPass;

extern UI_t UI;
extern uint32_t colorShiftID;

extern float fTimeStep, fTime;

// Volume rendering vulkan stuff
Pipeline_t volumePipeline;
//////

// Nebula volume texture generation
static Pipeline_t computePipeline;

// Fluid simulation
static Pipeline_t fluidPipeline;

static VkCommandPool computeCommandPool;
static VkCommandBuffer computeCommand;
static VkDescriptorPool computeDescriptorPool;

static VkFence fence=VK_NULL_HANDLE;

static uint32_t width=128;
static uint32_t height=128;
static uint32_t depth=128;

static VkuImage_t density_base;
static VkuImage_t density0, density1;
static VkuImage_t velocityX0, velocityX1;
static VkuImage_t velocityY0, velocityY1;
static VkuImage_t velocityZ0, velocityZ1;

VkBool32 InitNebulaVolume(void)
{
	if(!vkuCreateTexture3D(&vkContext, &density_base, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &density0, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityX0, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityY0, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityZ0, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &density1, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityX1, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityY1, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	if(!vkuCreateTexture3D(&vkContext, &velocityZ1, width, height, depth, VK_FORMAT_R32_SFLOAT, VK_SAMPLE_COUNT_1_BIT))
		return VK_FALSE;

	VkCommandBuffer command=vkuOneShotCommandBufferBegin(&vkContext);
	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density_base.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityX0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityY0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityZ0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityX1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityY1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityZ1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdClearColorImage(command, density_base.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,     density0.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityX0.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityY0.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityZ0.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,     density1.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityX1.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityY1.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });
	vkCmdClearColorImage(command,   velocityZ1.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &(VkClearColorValue) { { 0.0f, 0.0f, 0.0f, 0.0f } }, 1, &(VkImageSubresourceRange) {.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 });

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density_base.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityX0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityY0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityZ0.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityX1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityY1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=velocityZ1.image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .subresourceRange.baseMipLevel=0, .subresourceRange.levelCount=1, .subresourceRange.baseArrayLayer=0, .subresourceRange.layerCount=1,
	});

	vkuOneShotCommandBufferEnd(&vkContext, command);

	if(!CreatePipeline(&vkContext, &computePipeline, VK_NULL_HANDLE, "pipelines/volume_gen.pipeline"))
		return VK_FALSE;

	if(!CreatePipeline(&vkContext, &fluidPipeline, VK_NULL_HANDLE, "pipelines/fluid3d.pipeline"))
		return VK_FALSE;

	vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
		.poolSizeCount=1,
		.pPoolSizes=(VkDescriptorPoolSize[]){ {.type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount=1024, }, },
	}, VK_NULL_HANDLE, &computeDescriptorPool);

	if(vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=0,
		.queueFamilyIndex=vkContext.computeQueueIndex,
	}, VK_NULL_HANDLE, &computeCommandPool)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=computeCommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
		return VK_FALSE;

	GenNebulaVolume();

	return VK_TRUE;
}

VkBool32 GenNebulaVolume(void)
{
	vkResetDescriptorPool(vkContext.device, computeDescriptorPool, 0);
	vkResetCommandPool(vkContext.device, computeCommandPool, 0);
	vkResetFences(vkContext.device, 1, (VkFence []){ fence });

	vkBeginCommandBuffer(computeCommand, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density_base.image,
		.subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 }
	});

	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&computePipeline.descriptorSet, 0, VK_NULL_HANDLE, density_base.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuAllocateUpdateDescriptorSet(&computePipeline.descriptorSet, computeDescriptorPool);

	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 1, &computePipeline.descriptorSet.descriptorSet, 0, 0);

	static vec4 vRandom={ 0.0f, 0.0f, 0.0f, 0.0f };
	vRandom=Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	vkCmdPushConstants(computeCommand, computePipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4), &vRandom);

	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_GENERAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=density_base.image,
		.subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 }
	});

	// End command buffer and submit
	if(vkEndCommandBuffer(computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	VkSubmitInfo submitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&computeCommand,
	};

	vkQueueSubmit(vkContext.computeQueue, 1, &submitInfo, fence);

	if(vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

void barrier(VkCommandBuffer command, VkImage image)
{
	vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_GENERAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image,
		.subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 }
	});
}

VkBool32 FluidStep(void)
{
	vkResetDescriptorPool(vkContext.device, computeDescriptorPool, 0);
	vkResetCommandPool(vkContext.device, computeCommandPool, 0);
	vkResetFences(vkContext.device, 1, (VkFence[]) { fence });

	vkBeginCommandBuffer(computeCommand, &(VkCommandBufferBeginInfo) {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, fluidPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 0, VK_NULL_HANDLE, density_base.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 1, VK_NULL_HANDLE, density0.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 2, VK_NULL_HANDLE, velocityX0.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 3, VK_NULL_HANDLE, velocityY0.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 4, VK_NULL_HANDLE, velocityZ0.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 5, VK_NULL_HANDLE, density1.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 6, VK_NULL_HANDLE, velocityX1.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 7, VK_NULL_HANDLE, velocityY1.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&fluidPipeline.descriptorSet, 8, VK_NULL_HANDLE, velocityZ1.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuAllocateUpdateDescriptorSet(&fluidPipeline.descriptorSet, computeDescriptorPool);

	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, fluidPipeline.pipelineLayout, 0, 1, &fluidPipeline.descriptorSet.descriptorSet, 0, 0);

	struct
	{
		uint32_t uSize[3];
		uint32_t uKernel;
		float fVisc, fDiff;
		float fDt, pad;

		vec4 positionRadius;
		vec4 velocityDensity;
	} PC;

	PC.uSize[0]=width;
	PC.uSize[1]=height;
	PC.uSize[2]=depth;
	PC.fDiff=0.00002f;
	PC.fVisc=0.0000002f;
	PC.fDt=fTimeStep;

	// Set some density and velocity
	PC.uKernel=14;

	PC.positionRadius=Vec4(35.0f, 35.0f, (depth/2), 0.0001f);
	PC.velocityDensity=Vec4(5.0f, 5.0f, 0.0f, 1.0f);
	//Vec3_Normalize((vec3 *)&PC.velocityDensity);
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, density0.image);
	barrier(computeCommand, velocityX0.image);
	barrier(computeCommand, velocityY0.image);
	barrier(computeCommand, velocityZ0.image);

	PC.positionRadius=Vec4(width-10, (height/2), (depth/2), 0.0001f);
	PC.velocityDensity=Vec4(-5.0f, sinf(fTime*8)*0.5, cosf(fTime*8)*0.5, 1.0f);
	//Vec3_Normalize((vec3 *)&PC.velocityDensity);
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, density0.image);
	barrier(computeCommand, velocityX0.image);
	barrier(computeCommand, velocityY0.image);
	barrier(computeCommand, velocityZ0.image);

	// Restore density from image
	//PC.uKernel=15;
	//vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	//vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	//barrier(computeCommand, density0.image);

	// Diffuse
	PC.uKernel=0;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		barrier(computeCommand, velocityX1.image);
	}

	PC.uKernel=1;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		barrier(computeCommand, velocityY1.image);
	}

	PC.uKernel=2;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		barrier(computeCommand, velocityZ1.image);
	}

	// Project
	PC.uKernel=3;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityX0.image);
	barrier(computeCommand, velocityY0.image);

	PC.uKernel=4;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &(VkMemoryBarrier) {.sType=VK_STRUCTURE_TYPE_MEMORY_BARRIER, .pNext=VK_NULL_HANDLE, .srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT }, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE);
		barrier(computeCommand, velocityX0.image);
	}

	PC.uKernel=5;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityX1.image);
	barrier(computeCommand, velocityY1.image);
	barrier(computeCommand, velocityZ1.image);

	// Advect
	PC.uKernel=6;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityX0.image);
	PC.uKernel=7;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityY0.image);
	PC.uKernel=8;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityZ0.image);

	// Project
	PC.uKernel=9;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityY1.image);

	PC.uKernel=10;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		barrier(computeCommand, velocityX1.image);
	}

	PC.uKernel=11;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, velocityX0.image);
	barrier(computeCommand, velocityY0.image);
	barrier(computeCommand, velocityZ0.image);

	// Diffuse
	PC.uKernel=12;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);

	for(uint32_t iter=0;iter<4;iter++)
	{
		vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
		barrier(computeCommand, density1.image);
	}

	// Advect
	PC.uKernel=13;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, density0.image);

	PC.uKernel=16;
	vkCmdPushConstants(computeCommand, fluidPipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PC), &PC);
	vkCmdDispatch(computeCommand, width/8, height/8, depth/8);
	barrier(computeCommand, density0.image);
	barrier(computeCommand, velocityX0.image);
	barrier(computeCommand, velocityY0.image);
	barrier(computeCommand, velocityZ0.image);

	// End command buffer and submit
	if(vkEndCommandBuffer(computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	VkSubmitInfo submitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&computeCommand,
	};

	vkQueueSubmit(vkContext.computeQueue, 1, &submitInfo, fence);

	if(vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

// Create functions for volume rendering
bool CreateVolumePipeline(void)
{
	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &volumePipeline, renderPass, "pipelines/volume.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	return true;
}

void DestroyVolume(void)
{
	vkuDestroyImageBuffer(&vkContext, &density_base);
	vkuDestroyImageBuffer(&vkContext, &density0);
	vkuDestroyImageBuffer(&vkContext, &velocityX0);
	vkuDestroyImageBuffer(&vkContext, &velocityY0);
	vkuDestroyImageBuffer(&vkContext, &velocityZ0);
	vkuDestroyImageBuffer(&vkContext, &density1);
	vkuDestroyImageBuffer(&vkContext, &velocityX1);
	vkuDestroyImageBuffer(&vkContext, &velocityY1);
	vkuDestroyImageBuffer(&vkContext, &velocityZ1);

	vkDestroyFence(vkContext.device, fence, VK_NULL_HANDLE);

	vkDestroyCommandPool(vkContext.device, computeCommandPool, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(vkContext.device, computeDescriptorPool, VK_NULL_HANDLE);
	
	DestroyPipeline(&vkContext, &computePipeline);

	DestroyPipeline(&vkContext, &fluidPipeline);

	DestroyPipeline(&vkContext, &volumePipeline);
}

void DrawVolume(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	// Volumetric rendering is broken on Android when rendering at half resolution for some reason.
	static uint32_t uFrame=0;

	struct
	{
		uint32_t uFrame;
		uint32_t uWidth;
		uint32_t uHeight;
		float fShift;
		uint32_t uSamples;
		uint32_t pad[3];
	} PC;

	PC.uFrame=uFrame++;
	PC.uWidth=config.renderWidth;
	PC.uHeight=config.renderHeight;
	PC.fShift=UI_GetBarGraphValue(&UI, colorShiftID);
	PC.uSamples=config.msaaSamples;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipeline.pipeline);

	vkCmdPushConstants(commandBuffer, volumePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 0, density_base.sampler, density_base.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 1, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&volumePipeline.descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipelineLayout, 0, 1, &volumePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}
