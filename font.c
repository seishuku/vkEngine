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
#include "system.h"
#include "vulkan.h"
#include "math.h"
#include "font.h"

// external Vulkan context data/functions for this module:
extern VkContext_t Context;
extern VkRenderPass RenderPass;

extern int Width, Height;	// Window width/height from main app.
// ---

// Vulkan context data unique to this module:

// Descriptors
VkDescriptorPool fontDescriptorPool=VK_NULL_HANDLE;
VkDescriptorSet fontDescriptorSet=VK_NULL_HANDLE;
VkDescriptorSetLayout fontDescriptorSetLayout=VK_NULL_HANDLE;

// Pipeline
VkPipelineLayout fontPipelineLayout=VK_NULL_HANDLE;
VkPipeline fontPipeline=VK_NULL_HANDLE;

// Texture handles
Image_t fontTexture;

// Vertex data handles
VkDeviceMemory fontVertexBufferMemory=VK_NULL_HANDLE;
VkBuffer fontVertexBuffer=VK_NULL_HANDLE;

// Instance data handles
VkDeviceMemory fontInstanceBufferMemory=VK_NULL_HANDLE;
VkBuffer fontInstanceBuffer=VK_NULL_HANDLE;
void *fontInstanceBufferPtr=VK_NULL_HANDLE;

// Uniform data handles
VkDeviceMemory fontUniformBufferMemory=VK_NULL_HANDLE;
VkBuffer fontUniformBuffer=VK_NULL_HANDLE;
void *fontUniformBufferPtr=VK_NULL_HANDLE;
// ---

// Initialization flag
bool Font_Init=false;

void _Font_Init(void)
{
	VkBuffer stagingBuffer=VK_NULL_HANDLE;
	VkDeviceMemory stagingBufferMemory=VK_NULL_HANDLE;
	VkCommandBuffer copyCmd=VK_NULL_HANDLE;
	VkFence fence=VK_NULL_HANDLE;
	VkShaderModule vertexShader=vkuCreateShaderModule(Context.Device, "font_v.spv");
	VkShaderModule fragmentShader=vkuCreateShaderModule(Context.Device, "font_f.spv");
	void *data=NULL;

	// Create new descriptor sets and pipeline
	vkCreateDescriptorPool(Context.Device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.maxSets=1,
		.poolSizeCount=2,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=1,
			},
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
			},
		},
	}, NULL, &fontDescriptorPool);

	vkCreateDescriptorSetLayout(Context.Device, &(VkDescriptorSetLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.bindingCount=2,
		.pBindings=(VkDescriptorSetLayoutBinding[])
		{
			{
				.binding=0,
				.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_VERTEX_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
		},
	}, NULL, &fontDescriptorSetLayout);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&fontDescriptorSetLayout,
	}, 0, &fontPipelineLayout);

	vkCreateGraphicsPipelines(Context.Device, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount=2,
		.pStages=(VkPipelineShaderStageCreateInfo[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage=VK_SHADER_STAGE_VERTEX_BIT,
				.module=vertexShader,
				.pName="main",
			},
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage=VK_SHADER_STAGE_FRAGMENT_BIT,
				.module=fragmentShader,
				.pName="main",
			},
		},
		.pVertexInputState=&(VkPipelineVertexInputStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount=2,
			.pVertexBindingDescriptions=(VkVertexInputBindingDescription[])
			{
				{
					.binding=0,
					.stride=sizeof(float)*4,
					.inputRate=VK_VERTEX_INPUT_RATE_VERTEX,
				},
				{
					.binding=1,
					.stride=sizeof(float)*7,
					.inputRate=VK_VERTEX_INPUT_RATE_INSTANCE,
				}
			},
			.vertexAttributeDescriptionCount=3,
			.pVertexAttributeDescriptions=(VkVertexInputAttributeDescription[])
			{
				{
					.location=0,
					.binding=0,
					.format=VK_FORMAT_R32G32B32A32_SFLOAT,
					.offset=0
				},
				{
					.location=1,
					.binding=1,
					.format=VK_FORMAT_R32G32B32A32_SFLOAT,
					.offset=0
				},
				{
					.location=2,
					.binding=1,
					.format=VK_FORMAT_R32G32B32_SFLOAT,
					.offset=sizeof(float)*4
				},
			},
		},
		.pInputAssemblyState=&(VkPipelineInputAssemblyStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
			.primitiveRestartEnable=VK_FALSE,
		},
		.pViewportState=&(VkPipelineViewportStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount=1,
			.pViewports=0,
			.scissorCount=1,
			.pScissors=0,
		},
		.pRasterizationState=&(VkPipelineRasterizationStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable=VK_FALSE,
			.rasterizerDiscardEnable=VK_FALSE,
			.polygonMode=VK_POLYGON_MODE_FILL,
			.cullMode=VK_CULL_MODE_NONE,
			.frontFace=VK_FRONT_FACE_CLOCKWISE,
			.depthBiasEnable=VK_FALSE,
			.depthBiasConstantFactor=0.0f,
			.depthBiasClamp=0.0f,
			.depthBiasSlopeFactor=0.0f,
			.lineWidth=1.0f,
		},
		.pDepthStencilState=&(VkPipelineDepthStencilStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable=VK_FALSE,
			.depthWriteEnable=VK_FALSE,
			.depthCompareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
			.depthBoundsTestEnable=VK_FALSE,
			.stencilTestEnable=VK_FALSE,
			.minDepthBounds=0.0f,
			.maxDepthBounds=0.0f,
			.front=(VkStencilOpState)
			{
				.failOp=VK_STENCIL_OP_KEEP,
				.passOp=VK_STENCIL_OP_KEEP,
				.depthFailOp=VK_STENCIL_OP_KEEP,
				.compareOp=VK_COMPARE_OP_ALWAYS,
				.compareMask=0,
				.writeMask=0,
				.reference=0,
			},
			.back=(VkStencilOpState)
			{
				.failOp=VK_STENCIL_OP_KEEP,
				.passOp=VK_STENCIL_OP_KEEP,
				.depthFailOp=VK_STENCIL_OP_KEEP,
				.compareOp=VK_COMPARE_OP_ALWAYS,
				.compareMask=0,
				.writeMask=0,
				.reference=0,
			},
		},
		.pMultisampleState=&(VkPipelineMultisampleStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable=VK_FALSE,
			.minSampleShading=1.0f,
			.pSampleMask=0,
			.alphaToCoverageEnable=VK_FALSE,
			.alphaToOneEnable=VK_FALSE,
		},
		.pColorBlendState=&(VkPipelineColorBlendStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable=VK_FALSE,
			.attachmentCount=1,
			.pAttachments=(VkPipelineColorBlendAttachmentState[])
			{
				{
					.blendEnable=VK_TRUE,
					.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,
					.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.colorBlendOp=VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA,
					.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
					.alphaBlendOp=VK_BLEND_OP_ADD,
					.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT,
				},
			},
		},
		.pDynamicState=&(VkPipelineDynamicStateCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount=2,
			.pDynamicStates=(VkDynamicState[]) { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR },
		},
		.layout=fontPipelineLayout,
		.renderPass=RenderPass,
	}, 0, &fontPipeline);

	vkDestroyShaderModule(Context.Device, vertexShader, 0);
	vkDestroyShaderModule(Context.Device, fragmentShader, 0);
	// ---

	// Create sampler and load texture data
	vkuCreateBuffer(&Context,
		&stagingBuffer, &stagingBufferMemory, 16*16*223,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	{
		FILE *Stream=NULL;

		if(fopen_s(&Stream, "font.bin", "rb"))
			return;

		if(Stream==NULL)
			return;

		fseek(Stream, 0, SEEK_END);
		uint32_t Size=ftell(Stream);
		fseek(Stream, 0, SEEK_SET);

		uint8_t *_FontData=(uint8_t *)malloc(Size);

		if(_FontData==NULL)
			return;

		fread_s(_FontData, Size, 1, Size, Stream);
		fclose(Stream);

		// Map image memory and copy data
		vkMapMemory(Context.Device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
		uint8_t *ptr=(uint8_t *)data;
		uint8_t *FontPtr=(uint8_t *)_FontData;

		for(uint32_t i=0;i<16*16*223;i++)
			*ptr++=*FontPtr++;
		vkUnmapMemory(Context.Device, stagingBufferMemory);

		FREE(_FontData);
	}

	vkuCreateImageBuffer(&Context, &fontTexture,
		VK_IMAGE_TYPE_3D, VK_FORMAT_R8_UNORM, 1, 1, 16, 16, 223,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	// Setup image memory barrier transfer image to shader read layout
	vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context.CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);
	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=fontTexture.Image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	});

	// Copy from staging buffer to the texture buffer
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, fontTexture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy)
	{
		.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel=0,
		.imageSubresource.baseArrayLayer=0,
		.imageSubresource.layerCount=1,
		.imageExtent.width=16,
		.imageExtent.height=16,
		.imageExtent.depth=223,
		.bufferOffset=0,
	});

	// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
	// Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
	// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=fontTexture.Image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	});

	vkEndCommandBuffer(copyCmd);
		
	// Submit to the queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	// Wait for the fence to signal that command buffer has finished executing
	vkQueueWaitIdle(Context.Queue);
	vkFreeCommandBuffers(Context.Device, Context.CommandPool, 1, &copyCmd);

	vkFreeMemory(Context.Device, stagingBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, stagingBuffer, VK_NULL_HANDLE);

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
		.viewType=VK_IMAGE_VIEW_TYPE_3D,
		.format=VK_FORMAT_R8_UNORM,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=fontTexture.Image,
	}, VK_NULL_HANDLE, &fontTexture.View);
	// ---

	// Uniform data
	vkuCreateBuffer(&Context, &fontUniformBuffer, &fontUniformBufferMemory, sizeof(uint32_t)*2,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(Context.Device, fontUniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &fontUniformBufferPtr);
	// ---

	// Allocate and update descripter set with uniform/texture handles
	vkAllocateDescriptorSets(Context.Device, &(VkDescriptorSetAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext=NULL,
		.descriptorPool=fontDescriptorPool,
		.descriptorSetCount=1,
		.pSetLayouts=&fontDescriptorSetLayout
	}, &fontDescriptorSet);

	vkUpdateDescriptorSets(Context.Device, 2, (VkWriteDescriptorSet[])
	{
		{
			.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.descriptorCount=1,
			.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.dstBinding=0,
			.pBufferInfo=&(VkDescriptorBufferInfo)
			{
				.buffer=fontUniformBuffer,
				.offset=0,
				.range=sizeof(uint32_t)*2,
			},
			.dstSet=fontDescriptorSet,
		},
		{
			.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.descriptorCount=1,
			.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.dstBinding=1,
			.pImageInfo=&(VkDescriptorImageInfo)
			{
				.imageView=fontTexture.View,
				.sampler=fontTexture.Sampler,
				.imageLayout=fontTexture.ImageLayout,
			},
			.dstSet=fontDescriptorSet,
		},
	}, 0, VK_NULL_HANDLE);

	// Create static vertex data buffer
	vkuCreateBuffer(&Context, &fontVertexBuffer, &fontVertexBufferMemory, sizeof(float)*4*4,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Create staging buffer, map it, and copy vertex data to it
	vkuCreateBuffer(&Context, &stagingBuffer, &stagingBufferMemory, sizeof(float)*4*4,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map it
	vkMapMemory(Context.Device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);

	*((float *)data)++=0.0f;	// X
	*((float *)data)++=16.0f;	// Y
	*((float *)data)++=0.0f;
	*((float *)data)++=1.0f;

	*((float *)data)++=0.0f;
	*((float *)data)++=0.0f;
	*((float *)data)++=0.0f;	// U
	*((float *)data)++=0.0f;	// V

	*((float *)data)++=16.0f;
	*((float *)data)++=16.0f;
	*((float *)data)++=1.0f;
	*((float *)data)++=1.0f;

	*((float *)data)++=16.0f;
	*((float *)data)++=0.0f;
	*((float *)data)++=1.0f;
	*((float *)data)++=0.0f;

	vkUnmapMemory(Context.Device, stagingBufferMemory);

	vkuCopyBuffer(&Context, stagingBuffer, fontVertexBuffer, sizeof(float)*4*4);

	// Delete staging data
	vkFreeMemory(Context.Device, stagingBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, stagingBuffer, VK_NULL_HANDLE);
	// ---

	// Create instance buffer and map it
	vkuCreateBuffer(&Context, &fontInstanceBuffer, &fontInstanceBufferMemory, sizeof(float)*7*255,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	vkMapMemory(Context.Device, fontInstanceBufferMemory, 0, VK_WHOLE_SIZE, 0, (void *)&fontInstanceBufferPtr);
	// ---
}

void Font_Print(VkCommandBuffer cmd, float x, float y, char *string, ...)
{
	// float pointer for VBO mappings (both vertex and instance data)
	float *verts=NULL;
	// pointer and buffer for formatted text
	char *ptr, text[255];
	// variable arguments list
	va_list	ap;
	// some misc variables
	int sx=(int)x, numchar;
	// current text color
	float r=1.0f, g=1.0f, b=1.0f;

	// Check if the string is even valid first
	if(string==NULL)
		return;

	// Format string, including variable arguments
	va_start(ap, string);
	vsprintf_s(text, 255, string, ap);
	va_end(ap);

	// Find how many characters were need to deal with
	numchar=(int)strlen(text);

	// Generate texture, shaders, etc once
	if(!Font_Init)
	{
		_Font_Init();

		// Done with init
		Font_Init=true;
	}

	((uint32_t *)fontUniformBufferPtr)[0]=Width;
	((uint32_t *)fontUniformBufferPtr)[1]=Height;

	// Update instance data
	// Get the pointer to the mapped instance buffer
	verts=(float *)fontInstanceBufferPtr;

	// Check if it's still valid
	if(verts==NULL)
		return;

	// Loop through the text string until EOL
	for(ptr=text;*ptr!='\0';ptr++)
	{
		// Decrement 'y' for any CR's
		if(*ptr=='\n')
		{
			x=(float)sx;
			y-=4;
			continue;
		}

		// Just advance spaces instead of rendering empty quads
		if(*ptr==' ')
		{
			x+=9;
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
		}

		// Emit position, atlas offset, and color for this character
		*verts++=x;									// InstancePos.x
		*verts++=y;									// InstancePos.y
		*verts++=((float)(*ptr-33)/223.0f)+0.0023f;	// InstancePos.z
		*verts++=0.0f;								// InstancePos.w
		*verts++=r;									// InstanceColor.r
		*verts++=g;									// InstanceColor.g
		*verts++=b;									// InstanceColor.b

		// Advance one character
		x+=9;
	}
	// ---

	// Bind the font rendering pipeline (sets states and shaders)
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipeline);
	// Bind descriptor sets (sets uniform/texture bindings)
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fontPipelineLayout, 0, 1, &fontDescriptorSet, 0, NULL);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(cmd, 0, 1, &fontVertexBuffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(cmd, 1, 1, &fontInstanceBuffer, &(VkDeviceSize) { 0 });

	// Draw the number of characters
	vkCmdDraw(cmd, 4, numchar, 0, 0);
}

void Font_Destroy(void)
{
	vkUnmapMemory(Context.Device, fontUniformBufferMemory);
	vkFreeMemory(Context.Device, fontUniformBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, fontUniformBuffer, VK_NULL_HANDLE);

	// Instance buffer handles
	vkUnmapMemory(Context.Device, fontInstanceBufferMemory);
	vkFreeMemory(Context.Device, fontInstanceBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, fontInstanceBuffer, VK_NULL_HANDLE);

	// Vertex data handles
	vkFreeMemory(Context.Device, fontVertexBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, fontVertexBuffer, VK_NULL_HANDLE);

	// Texture handles
	vkDestroySampler(Context.Device, fontTexture.Sampler, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, fontTexture.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImageView(Context.Device, fontTexture.View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, fontTexture.Image, VK_NULL_HANDLE);

	// Pipeline
	vkDestroyPipelineLayout(Context.Device, fontPipelineLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, fontPipeline, VK_NULL_HANDLE);

	// Descriptors
	vkDestroyDescriptorPool(Context.Device, fontDescriptorPool, VK_NULL_HANDLE);
	vkDestroyDescriptorSetLayout(Context.Device, fontDescriptorSetLayout, VK_NULL_HANDLE);
}
