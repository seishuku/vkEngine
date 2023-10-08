/*
	Copyright 2020 Matt Williams/NitroGL
	Simple (?) Vulakn Font/Text printing function
	Uses two vertex buffers, one for a single triangle strip making up
	a quad, the other contains instancing data for character
	position, texture altas lookup and color.
*/
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../font/font.h"
#include "../perframe.h"

// external Vulkan context data/functions for this module:
extern VkuContext_t Context;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t Swapchain;

extern VkuMemZone_t *VkZone;

extern uint32_t Width, Height;	// Window width/height from main app.
// ---

// Vulkan context data unique to this module:

VkuDescriptorSet_t fontDescriptorSet;
VkPipelineLayout fontPipelineLayout;
VkuPipeline_t fontPipeline;

// Texture handles
VkuImage_t fontTexture;

// Vertex data handles
VkuBuffer_t fontVertexBuffer;

// Instance data handles
VkuBuffer_t fontInstanceBuffer;
void *fontInstanceBufferPtr;

// Initialization flag
bool Font_Init=false;

// Global running instance buffer pointer, resets on submit
static vec4 *Instance=NULL;
static uint32_t numchar=0;

static void _Font_Init(void)
{
	VkuBuffer_t stagingBuffer;
	VkCommandBuffer CopyCommand;
	void *data=NULL;

	// Create descriptors and pipeline
	vkuInitDescriptorSet(&fontDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&fontDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&fontDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&fontDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(uint32_t)*2,
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &fontPipelineLayout);

	vkuInitPipeline(&fontPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&fontPipeline, fontPipelineLayout);

	fontPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	fontPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	fontPipeline.RasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

	fontPipeline.Blend=VK_TRUE;
	fontPipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	fontPipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	fontPipeline.ColorBlendOp=VK_BLEND_OP_ADD;
	fontPipeline.SrcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	fontPipeline.DstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	fontPipeline.AlphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&fontPipeline, "./shaders/font.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return;

	if(!vkuPipeline_AddStage(&fontPipeline, "./shaders/font.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return;

	vkuPipeline_AddVertexBinding(&fontPipeline, 0, sizeof(vec4), VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&fontPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);

	vkuPipeline_AddVertexBinding(&fontPipeline, 1, sizeof(vec4)*2, VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&fontPipeline, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&fontPipeline, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&Swapchain.SurfaceFormat.format,
	};

	if(!vkuAssemblePipeline(&fontPipeline, &PipelineRenderingCreateInfo))
		return;
	// ---

	// Create sampler and load texture data
	vkuCreateHostBuffer(&Context, &stagingBuffer, 16*16*223, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	{
		FILE *Stream=NULL;

		if((Stream=fopen("./assets/font.bin", "rb"))==NULL)
			return;

		if(Stream==NULL)
			return;

		fseek(Stream, 0, SEEK_END);
		uint32_t Size=ftell(Stream);
		fseek(Stream, 0, SEEK_SET);

		uint8_t *_FontData=(uint8_t *)Zone_Malloc(Zone, Size);

		if(_FontData==NULL)
			return;

		fread(_FontData, 1, Size, Stream);
		fclose(Stream);

		// Map image memory and copy data
		vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &data);
		uint8_t *ptr=(uint8_t *)data;
		uint8_t *FontPtr=(uint8_t *)_FontData;

		for(uint32_t i=0;i<16*16*223;i++)
			*ptr++=*FontPtr++;
		vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

		Zone_Free(Zone, _FontData);
	}

	// Create the 2D texture object and sampler/imageview
	vkuCreateImageBuffer(&Context, &fontTexture,
		VK_IMAGE_TYPE_2D, VK_FORMAT_R8_UNORM, 1, 223, 16, 16, 1,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	vkCreateSampler(Context.Device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy=1.0f,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=0.0f,
		.maxAnisotropy=1.0,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &fontTexture.Sampler);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		.format=VK_FORMAT_R8_UNORM,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=223,
		.subresourceRange.levelCount=1,
		.image=fontTexture.Image,
	}, VK_NULL_HANDLE, &fontTexture.View);

	// Start a one shot command buffer for image transfer from CPU to GPU
	CopyCommand=vkuOneShotCommandBufferBegin(&Context);

	// Undefined layout -> Transfer destination
	vkuTransitionLayout(CopyCommand, fontTexture.Image, 1, 0, 223, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy from staging buffer to the texture buffer
	vkCmdCopyBufferToImage(CopyCommand, stagingBuffer.Buffer, fontTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy)
	{
		.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel=0,
		.imageSubresource.baseArrayLayer=0,
		.imageSubresource.layerCount=223,
		.imageExtent.width=16,
		.imageExtent.height=16,
		.imageExtent.depth=1,
		.bufferOffset=0,
	});

	// Transfer destination -> Shader read-only layout
	vkuTransitionLayout(CopyCommand, fontTexture.Image, 1, 0, 223, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Done with command buffer, submit it
	vkuOneShotCommandBufferEnd(&Context, CopyCommand);

	// Done with the staging buffer
	vkuDestroyBuffer(&Context, &stagingBuffer);

	// Create static vertex data buffer
	vkuCreateGPUBuffer(&Context, &fontVertexBuffer, sizeof(float)*4*4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Create staging buffer, map it, and copy vertex data to it
	vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(float)*4*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// Map it
	vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &data);

	if(!data)
		return;

	vec4 *Ptr=(vec4 *)data;

	*Ptr++=Vec4(0.0f, 1.0f, 0.0f, 1.0f);	// XYUV
	*Ptr++=Vec4(0.0f, 0.0f, 0.0f, 0.0f);
	*Ptr++=Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	*Ptr++=Vec4(1.0f, 0.0f, 1.0f, 0.0f);

	vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

	CopyCommand=vkuOneShotCommandBufferBegin(&Context);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, fontVertexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&Context, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&Context, &stagingBuffer);

	// Create instance buffer and map it
	vkuCreateHostBuffer(&Context, &fontInstanceBuffer, sizeof(vec4)*2*8192, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vkMapMemory(Context.Device, fontInstanceBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&fontInstanceBufferPtr);
}

// Accumulates text to render
void Font_Print(float size, float x, float y, char *string, ...)
{
	// Pointer and buffer for formatted text
	char *ptr, text[255];
	// Variable arguments list
	va_list	ap;
	// Save starting x position
	float sx=x;
	// Current text color
	float r=1.0f, g=1.0f, b=1.0f;

	// Check if the string is even valid first
	if(string==NULL)
		return;

	// Generate texture, shaders, etc once
	if(!Font_Init)
	{
		_Font_Init();

		// Set initial instance data pointer
		Instance=(vec4 *)fontInstanceBufferPtr;

		// Check if it's still valid
		if(Instance==NULL)
			return;

		// Set initial character count
		numchar=0;

		// Done with init
		Font_Init=true;
	}

	// Format string, including variable arguments
	va_start(ap, string);
	vsprintf(text, string, ap);
	va_end(ap);

	// Add in how many characters were need to deal with
	numchar+=(uint32_t)strlen(text);

	// Loop through and pre-offset 'y' by any CRs in the string, so nothing goes off screen
	for(ptr=text;*ptr!='\0';ptr++)
	{
		if(*ptr=='\n')
			y+=size;
	}

	// Loop through the text string until EOL
	for(ptr=text;*ptr!='\0';ptr++)
	{
		// Decrement 'y' for any CR's
		if(*ptr=='\n')
		{
			x=sx;
			y-=size;
			numchar--;
			continue;
		}

		// Just advance spaces instead of rendering empty quads
		if(*ptr==' ')
		{
			x+=size;
			numchar--;
			continue;
		}

		// ANSI color escape codes
		// I'm sure there's a better way to do this!
		// But it works, so whatever.
		if(*ptr=='\x1B')
		{
			ptr++;
			     if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=0.0f;	}	// BLACK
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.0f;	b=0.0f;	}	// DARK RED
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.5f;	b=0.0f;	}	// DARK GREEN
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.0f;	}	// DARK YELLOW
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=0.5f;	}	// DARK BLUE
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.0f;	b=0.5f;	}	// DARK MAGENTA
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.5f;	b=0.5f;	}	// DARK CYAN
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.5f;	}	// GREY
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.5f;	}	// GREY
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{	r=1.0f;	g=0.0f;	b=0.0f;	}	// RED
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{	r=0.0f;	g=1.0f;	b=0.0f;	}	// GREEN
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{	r=1.0f;	g=1.0f;	b=0.0f;	}	// YELLOW
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=1.0f;	}	// BLUE
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{	r=1.0f;	g=0.0f;	b=1.0f;	}	// MAGENTA
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{	r=0.0f;	g=1.0f;	b=1.0f;	}	// CYAN
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{	r=1.0f;	g=1.0f;	b=1.0f;	}	// WHITE
			ptr+=4;
			numchar-=5;
		}

		// Emit position, atlas offset, and color for this character
		*Instance++=Vec4(x, y, (float)(*ptr-33), size); // Instance position, character to render, size
		*Instance++=Vec4(r, g, b, 1.0f);				// Instance color

		// Advance one character
		x+=size;
	}
	// ---
}

// Submits text draw data to GPU and resets for next frame
void Font_Draw(uint32_t Index)
{
	// Bind the font rendering pipeline (sets states and shaders)
	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline.Pipeline);

	// Bind descriptor sets (sets uniform/texture bindings)
	vkuDescriptorSet_UpdateBindingImageInfo(&fontDescriptorSet, 0, &fontTexture);
	vkuAllocateUpdateDescriptorSet(&fontDescriptorSet, PerFrame[Index].DescriptorPool);

	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipelineLayout, 0, 1, &fontDescriptorSet.DescriptorSet, 0, NULL);

	uint32_t viewport[2]={ Width, Height };
	vkCmdPushConstants(PerFrame[Index].CommandBuffer, fontPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)*2, viewport);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 0, 1, &fontVertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 1, 1, &fontInstanceBuffer.Buffer, &(VkDeviceSize) { 0 });

	// Draw the number of characters
	vkCmdDraw(PerFrame[Index].CommandBuffer, 4, numchar, 0, 0);

	// Reset instance data pointer and character count
	Instance=fontInstanceBufferPtr;
	numchar=0;
}

void Font_Destroy(void)
{
	// Instance buffer handles
	if(fontInstanceBuffer.DeviceMemory)
		vkUnmapMemory(Context.Device, fontInstanceBuffer.DeviceMemory);

	vkuDestroyBuffer(&Context, &fontInstanceBuffer);

	// Vertex data handles
	vkuDestroyBuffer(&Context, &fontVertexBuffer);

	// Texture handles
	vkuDestroyImageBuffer(&Context, &fontTexture);

	// Pipeline
	vkDestroyPipelineLayout(Context.Device, fontPipelineLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, fontPipeline.Pipeline, VK_NULL_HANDLE);

	// Descriptors
	vkDestroyDescriptorSetLayout(Context.Device, fontDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
}
