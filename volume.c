#include "vulkan/vulkan.h"
#include "math/math.h"
#include "utils/pipeline.h"
#include "ui/ui.h"
#include "perframe.h"
#include "textures.h"
#include "shadow.h"

#define USE_COMPUTE_SHADER

extern VkuContext_t vkContext;
extern VkSampleCountFlags MSAA;
extern VkFormat colorFormat;
extern VkFormat depthFormat;
extern VkuImage_t depthImage[2];
extern VkRenderPass renderPass;

extern UI_t UI;
extern uint32_t colorShiftID;

extern uint32_t renderWidth;
extern uint32_t renderHeight;

// Volume rendering vulkan stuff
Pipeline_t volumePipeline;
//////

// Nebula volume texture generation
#ifndef USE_COMPUTE_SHADER
static int p[512]=
{
	151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225,
	140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
	247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
	57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
	74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122,
	60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
	65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169,
	200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
	52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
	207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
	119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
	129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
	218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241,
	81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
	184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
	222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180
};

static float fade(float t)
{
	return t*t*t*(t*(t*6-15)+10);
}

static float grad(int hash, float x, float y, float z)
{
	int h=hash&15;
	float u=h<8?x:y, v=h<4?y:h==12||h==14?x:z;

	return ((h&1)==0?u:-u)+((h&2)==0?v:-v);
}

static float noise(float x, float y, float z)
{
	static bool init=true;

	if(init)
	{
		for(int i=0; i<256; i++)
			p[256+i]=p[i];

		init=false;
	}

	int X=(int)floor(x)&255, Y=(int)floor(y)&255, Z=(int)floor(z)&255;

	x-=floorf(x);
	y-=floorf(y);
	z-=floorf(z);

	float u=fade(x), v=fade(y), w=fade(z);

	int A=p[X]+Y, AA=p[A]+Z, AB=p[A+1]+Z, B=p[X+1]+Y, BA=p[B]+Z, BB=p[B+1]+Z;

	return Lerp(
		Lerp(
			Lerp(
				grad(p[AA], x, y, z),
				grad(p[BA], x-1, y, z),
				u
			),
			Lerp(
				grad(p[AB], x, y-1, z),
				grad(p[BB], x-1, y-1, z),
				u
			),
			v
		),
		Lerp(
			Lerp(
				grad(p[AA+1], x, y, z-1),
				grad(p[BA+1], x-1, y, z-1),
				u
			),
			Lerp(
				grad(p[AB+1], x, y-1, z-1),
				grad(p[BB+1], x-1, y-1, z-1),
				u
			),
			v
		),
		w
	);
}

static float nebula(vec3 p)
{
	const int iterations=6;
	float turb=0.0f, scale=1.0f;

	for(int i=0;i<iterations;i++)
	{
		scale*=0.5f;
		turb+=scale*noise(p.x/scale, p.y/scale, p.z/scale);
	}

	return clampf(turb, 0.0f, 1.0f);
}

static float fbm(vec3 p)
{
	const int octaves=12;
	const float gain=0.25;
	const float lacunarity=4.0;

	float amplitude=1.0;
	float frequency=1.0;
	float sum=0.0;
	float q=0.0;

	for(int i=0;i<octaves;i++)
	{
		sum+=amplitude*noise(p.x*frequency, p.y*frequency, p.z*frequency);
		q+=amplitude;
		amplitude*=gain;
		frequency*=lacunarity;
	}

	return sum/q;
}
#endif

VkBool32 GenNebulaVolume(VkuImage_t *image)
{
	image->width=512;
	image->height=512;
	image->depth=512; // Slight abuse of image struct, depth is supposed to be color depth, not image depth.

	if(!vkuCreateImageBuffer(&vkContext, image,
							 VK_IMAGE_TYPE_3D, VK_FORMAT_R8_UNORM, 1, 1, image->width, image->height, image->depth,
							 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
							 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return VK_FALSE;

	// Create texture sampler object
	vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter=VK_FILTER_LINEAR,
			.minFilter=VK_FILTER_LINEAR,
			.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.mipLodBias=0.0f,
			.compareOp=VK_COMPARE_OP_NEVER,
			.minLod=0.0f,
			.maxLod=VK_LOD_CLAMP_NONE,
			.maxAnisotropy=1.0f,
			.anisotropyEnable=VK_FALSE,
			.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &image->sampler);

	// Create texture image view object
	vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=image->image,
			.viewType=VK_IMAGE_VIEW_TYPE_3D,
			.format=VK_FORMAT_R8_UNORM,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &image->imageView);

#ifndef USE_COMPUTE_SHADER
	VkCommandBuffer commandBuffer;
	VkuBuffer_t stagingBuffer;
	void *data=NULL;

	// Byte size of image data
	uint32_t size=image->width*image->height*image->depth;

	// Create staging buffer
	vkuCreateHostBuffer(&vkContext, &stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// Map image memory and copy data
	vkMapMemory(vkContext.device, stagingBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, &data);

	const float Scale=12.0f;

	for(uint32_t i=0;i<size;i++)
	{
		uint32_t x=i%image->width;
		uint32_t y=(i%(image->width*image->height))/image->width;
		uint32_t z=i/(image->width*image->height);

		vec3 v=
		{
			((float)x-(image->width>>1))/image->width,
			((float)y-(image->height>>1))/image->height,
			((float)z-(image->depth>>1))/image->depth,
		};

		float p=nebula(Vec3_Muls(v, Scale));

		((uint8_t *)data)[i]=(uint8_t)(p*255.0f);
	}

	vkUnmapMemory(vkContext.device, stagingBuffer.deviceMemory);

	// Start a one shot command buffer
	commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);

	// Change image layout from undefined to destination optimal, so we can copy from the staging buffer to the texture.
	vkuTransitionLayout(commandBuffer, image->image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy from staging buffer to the texture buffer.
	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, (VkBufferImageCopy[1])
	{
		{
			0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { image->width, image->height, image->depth }
		}
	});

	// Final change to image layout from destination optimal to be optimal reading only by shader.
	// This is also done by generating mipmaps, if requested.
	vkuTransitionLayout(commandBuffer, image->image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// End one shot command buffer and submit
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	// Delete staging buffers
	vkuDestroyBuffer(&vkContext, &stagingBuffer);
#else
	VkCommandPool computeCommandPool;
	VkCommandBuffer computeCommand;
	VkDescriptorPool computeDescriptorPool;
	Pipeline_t computePipeline;

	if(!CreatePipeline(&vkContext, &computePipeline, VK_NULL_HANDLE, "pipelines/volume_gen.pipeline"))
		return VK_FALSE;

	vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
		.poolSizeCount=1,
		.pPoolSizes=(VkDescriptorPoolSize[]){ {.type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount=1, }, },
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

	vkBeginCommandBuffer(computeCommand, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE,
		.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image->image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
	});

	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&computePipeline.descriptorSet, 0, VK_NULL_HANDLE, image->imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuAllocateUpdateDescriptorSet(&computePipeline.descriptorSet, computeDescriptorPool);

	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 1, &computePipeline.descriptorSet.descriptorSet, 0, 0);

	vec4 vRandom=Vec4(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	vkCmdPushConstants(computeCommand, computePipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4), &vRandom);

	vkCmdDispatch(computeCommand, image->width/8, image->height/8, image->depth/8);

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_GENERAL,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image->image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
	});

	// End command buffer and submit
	if(vkEndCommandBuffer(computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	VkFence fence=VK_NULL_HANDLE;

	if(vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
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

	vkDestroyFence(vkContext.device, fence, VK_NULL_HANDLE);

	vkDestroyCommandPool(vkContext.device, computeCommandPool, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(vkContext.device, computeDescriptorPool, VK_NULL_HANDLE);
	DestroyPipeline(&vkContext, &computePipeline);
#endif

	return VK_TRUE;
}
//////

// Create functions for volume rendering
bool CreateVolumePipeline(void)
{
	if(!CreatePipeline(&vkContext, &volumePipeline, renderPass, "pipelines/volume.pipeline"))
		return false;

	return true;
}

void DestroyVolume(void)
{
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
	} PC;

	PC.uFrame=uFrame++;
	PC.uWidth=renderWidth;
	PC.uHeight=renderHeight;
	PC.fShift=UI_GetBarGraphValue(&UI, colorShiftID);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipeline.pipeline);

	vkCmdPushConstants(commandBuffer, volumePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 0, textures[TEXTURE_VOLUME].sampler, textures[TEXTURE_VOLUME].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 1, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&volumePipeline.descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipelineLayout, 0, 1, &volumePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}
