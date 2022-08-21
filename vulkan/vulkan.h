#ifndef __VULKAN_H__
#define __VULKAN_H__

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#else
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <GL/glx.h>
#endif

#include <vulkan/vulkan.h>

#define vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT
extern PFN_vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT;

#define vkDestroyDebugUtilsMessengerEXT _vkDestroyDebugUtilsMessengerEXT
extern PFN_vkDestroyDebugUtilsMessengerEXT _vkDestroyDebugUtilsMessengerEXT;

#define vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR
extern PFN_vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR;

#define VKU_MAX_PIPELINE_VERTEX_BINDINGS 8
#define VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES 8
#define VKU_MAX_PIPELINE_SHADER_STAGES 4
#define VKU_MAX_DESCRIPTORSETLAYOUT_BINDINGS 16

typedef struct
{
#ifdef WIN32
	HWND hWnd;
#else
	Display *Dpy;
	Window Win;
#endif

	VkSurfaceKHR Surface;

	uint32_t QueueFamilyIndex;
	VkPhysicalDevice PhysicalDevice;
	VkPhysicalDeviceMemoryProperties DeviceMemProperties;

	VkDevice Device;
	VkQueue Queue;
	VkPipelineCache PipelineCache;
	VkCommandPool CommandPool;
} VkuContext_t;

// Because vulkan stuff here depends on image.h and image.h depends on the VkContext_t struct, annoying.
#include "../image/image.h"

typedef struct
{
	// Handles to dependencies
	VkDevice Device;
	VkPipelineCache PipelineCache;
	VkPipelineLayout PipelineLayout;
	VkRenderPass RenderPass;

	// Pipeline to be created
	VkPipeline Pipeline;

	// Settable states:

	// Vertex bindings
	uint32_t NumVertexBindings;
	VkVertexInputBindingDescription VertexBindings[VKU_MAX_PIPELINE_VERTEX_BINDINGS];

	// Vertex attributes
	uint32_t NumVertexAttribs;
	VkVertexInputAttributeDescription VertexAttribs[VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES];

	// Shader Stages
	uint32_t NumStages;
	VkPipelineShaderStageCreateInfo Stages[VKU_MAX_PIPELINE_SHADER_STAGES];

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

typedef struct
{
	VkDevice Device;

	VkDescriptorSetLayout DescriptorSetLayout;

	uint32_t NumBindings;
	VkDescriptorSetLayoutBinding Bindings[VKU_MAX_DESCRIPTORSETLAYOUT_BINDINGS];
} VkuDescriptorSetLayout_t;

VkShaderModule vkuCreateShaderModule(VkDevice Device, const char *shaderFile);

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask);

VkBool32 vkuCreateImageBuffer(VkuContext_t *Context, Image_t *Image, VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth, VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags);
VkBool32 vkuCreateBuffer(VkuContext_t *Context, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask);
VkBool32 vkuCopyBuffer(VkuContext_t *Context, VkBuffer Src, VkBuffer Dest, uint32_t Size);

VkBool32 vkuPipeline_AddVertexBinding(VkuPipeline_t *Pipeline, uint32_t Binding, uint32_t Stride, VkVertexInputRate InputRate);
VkBool32 vkuPipeline_AddVertexAttribute(VkuPipeline_t *Pipeline, uint32_t Location, uint32_t Binding, VkFormat Format, uint32_t Offset);
VkBool32 vkuPipeline_AddStage(VkuPipeline_t *Pipeline, const char *ShaderFilename, VkShaderStageFlagBits Stage);
VkBool32 vkuPipeline_SetRenderPass(VkuPipeline_t *Pipeline, VkRenderPass RenderPass);
VkBool32 vkuPipeline_SetPipelineLayout(VkuPipeline_t *Pipeline, VkPipelineLayout PipelineLayout);
VkBool32 vkuInitPipeline(VkuPipeline_t *Pipeline, VkuContext_t *Context);
VkBool32 vkuAssemblePipeline(VkuPipeline_t *Pipeline);

VkBool32 vkuDescriptorSetLayout_AddBinding(VkuDescriptorSetLayout_t *DescriptorSetLayout, uint32_t Binding, VkDescriptorType Type, uint32_t Count, VkShaderStageFlags Stage, const VkSampler *ImmutableSamplers);
VkBool32 vkuInitDescriptorSetLayout(VkuDescriptorSetLayout_t *DescriptorSetLayout, VkuContext_t *Context);
VkBool32 vkuAssembleDescriptorSetLayout(VkuDescriptorSetLayout_t *DescriptorSetLayout);
	
VkBool32 CreateVulkanInstance(VkInstance *Instance);
VkBool32 CreateVulkanContext(VkInstance Instance, VkuContext_t *Context);
void DestroyVulkan(VkInstance Instance, VkuContext_t *Context);

#endif
