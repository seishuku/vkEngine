#ifndef __VULKAN_H__
#define __VULKAN_H__

#include <vulkan/vulkan.h>

#ifdef WIN32
#include <Windows.h>
#endif

typedef struct
{
	HWND hWnd;
	VkSurfaceKHR Surface;

	uint32_t QueueFamilyIndex;
	VkPhysicalDevice PhysicalDevice;
	VkPhysicalDeviceMemoryProperties DeviceMemProperties;

	VkDevice Device;
	VkQueue Queue;
	VkCommandPool CommandPool;
} VkContext_t;

#include "image.h"

typedef struct
{
	// Handles to dependencies
	VkDevice Device;
	VkPipelineLayout PipelineLayout;
	VkRenderPass RenderPass;

	// Pipeline to be created
	VkPipeline Pipeline;

	// Settable states:

	// Vertex bindings, up to 5 should be enough? (to match attrib count)
	uint32_t NumVertexBindings;
	VkVertexInputBindingDescription VertexBindings[5];

	// Vertex attributes
	uint32_t NumVertexAttribs;
	VkVertexInputAttributeDescription VertexAttribs[5];

	// Shader Stages
	uint32_t NumStages;
	VkPipelineShaderStageCreateInfo Stages[4];

	// Input assembly state
	VkPrimitiveTopology Topology;
	VkBool32 PrimitiveRestart;

	// Rasterization state
	VkBool32 DepthClamp;
	VkBool32 RasterizerDiscard;
	VkPolygonMode PolygonMode;
	VkCullModeFlags CullMode;
	VkFrontFace FrontFace;
	VkBool32 DepthBias;
	float DepthBiasConstantFactor;
	float DepthBiasClamp;
	float DepthBiasSlopeFactor;
	float LineWidth;

	// Depth/stencil state
	VkBool32 DepthTest;
	VkBool32 DepthWrite;
	VkCompareOp DepthCompareOp;
	VkBool32 DepthBoundsTest;
	VkBool32 StencilTest;
	float MinDepthBounds;
	float MaxDepthBounds;

	// Front face stencil functions
	VkStencilOp FrontStencilFailOp;
	VkStencilOp FrontStencilPassOp;
	VkStencilOp FrontStencilDepthFailOp;
	VkStencilOp FrontStencilCompareOp;
	uint32_t FrontStencilCompareMask;
	uint32_t FrontStencilWriteMask;
	uint32_t FrontStencilRference;

	// Back face stencil functions
	VkStencilOp BackStencilFailOp;
	VkStencilOp BackStencilPassOp;
	VkStencilOp BackStencilDepthFailOp;
	VkStencilOp BackStencilCompareOp;
	uint32_t BackStencilCompareMask;
	uint32_t BackStencilWriteMask;
	uint32_t BackStencilReference;

	// Multisample state
	VkSampleCountFlagBits RasterizationSamples;
	VkBool32 SampleShading;
	float MinSampleShading;
	const VkSampleMask *SampleMask;
	VkBool32 AlphaToCoverage;
	VkBool32 AlphaToOne;

	// Blend state
	VkBool32 BlendLogicOp;
	VkLogicOp BlendLogicOpState;
	VkBool32 Blend;
	VkBlendFactor SrcColorBlendFactor;
	VkBlendFactor DstColorBlendFactor;
	VkBlendOp ColorBlendOp;
	VkBlendFactor SrcAlphaBlendFactor;
	VkBlendFactor DstAlphaBlendFactor;
	VkBlendOp AlphaBlendOp;
	VkColorComponentFlags ColorWriteMask;
} VkuPipeline_t;

VkShaderModule vkuCreateShaderModule(VkDevice Device, const char *shaderFile);

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask);

VkBool32 vkuCreateImageBuffer(VkContext_t *Context, Image_t *Image,
	VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth,
	VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags);
VkBool32 vkuCreateBuffer(VkContext_t *Context, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask);
VkBool32 vkuCopyBuffer(VkContext_t *Context, VkBuffer Src, VkBuffer Dest, uint32_t Size);

VkBool32 vkuPipeline_AddVertexBinding(VkuPipeline_t *Pipeline, uint32_t Binding, uint32_t Stride, VkVertexInputRate InputRate);
VkBool32 vkuPipeline_AddVertexAttribute(VkuPipeline_t *Pipeline, uint32_t Location, uint32_t Binding, VkFormat Format, uint32_t Offset);
VkBool32 vkuPipeline_AddStage(VkuPipeline_t *Pipeline, const char *ShaderFilename, VkShaderStageFlagBits Stage);
VkBool32 vkuInitPipeline(VkuPipeline_t *Pipeline, VkDevice Device, VkPipelineLayout PipelineLayout, VkRenderPass RenderPass);
VkBool32 vkuAssemblePipeline(VkuPipeline_t *Pipeline);

VkBool32 CreateVulkanInstance(VkInstance *Instance);
VkBool32 CreateVulkanContext(VkInstance Instance, VkContext_t *Context);
void DestroyVulkan(VkInstance Instance, VkContext_t *Context);

#endif
