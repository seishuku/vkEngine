#ifndef __VULKAN_H__
#define __VULKAN_H__

#ifdef WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <Windows.h>
#elif LINUX
#ifdef WAYLAND
#define VK_USE_PLATFORM_WAYLAND_KHR
#include <wayland-client.h>
#else
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#endif
#elif ANDROID
#define VK_USE_PLATFORM_ANDROID_KHR
#include <android/native_activity.h>
#endif

#include <vulkan/vulkan.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT
extern PFN_vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT;

#define vkDestroyDebugUtilsMessengerEXT _vkDestroyDebugUtilsMessengerEXT
extern PFN_vkDestroyDebugUtilsMessengerEXT _vkDestroyDebugUtilsMessengerEXT;

#define vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR
extern PFN_vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR;

#define VKU_MAX_PIPELINE_VERTEX_BINDINGS 4
#define VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES 16
#define VKU_MAX_PIPELINE_SHADER_STAGES 4

#define VKU_MAX_DESCRIPTORSET_BINDINGS 16

#define VKU_MAX_FRAME_COUNT 8

#define VKU_MAX_FRAMEBUFFER_ATTACHMENTS 8

#define VKU_MAX_RENDERPASS_ATTACHMENTS 8
#define VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES 8

#define VKU_MIN_DEVICE_ALLOCATION_SIZE (256*1024)

typedef struct
{
#ifdef WIN32
	HWND hWnd;
#elif LINUX
#ifdef WAYLAND
	struct wl_display *wlDisplay;
	struct wl_surface *wlSurface;
#else
	Display *display;
	Window window;
#endif
#elif ANDROID
	ANativeWindow *window;
#endif

	VkSurfaceKHR surface;

	VkBool32 swapchainExtension;
	VkBool32 pushDescriptorExtension;
	VkBool32 dynamicRenderingExtension;
	VkBool32 getPhysicalDeviceProperties2Extension;
	VkBool32 depthStencilResolveExtension;
	VkBool32 createRenderPass2Extension;

	VkBool32 externalMemoryExtension;
	VkBool32 externalFenceExtension;
	VkBool32 externalSemaphoreExtension;
	VkBool32 getMemoryRequirements2Extension;
	VkBool32 dedicatedAllocationExtension;

#ifdef WIN32
	VkBool32 externalMemoryWIN32Extension;
	VkBool32 externalFenceWIN32Extension;
	VkBool32 externalSemaphoreWIN32Extension;
	VkBool32 win32KeyedMutexExtension;
#else
	VkBool32 externalMemoryFDExtension;
	VkBool32 externalFenceFDExtension;
	VkBool32 externalSemaphoreFDExtension;
#endif

	VkPhysicalDeviceProperties2 deviceProperties;
	VkPhysicalDeviceMaintenance3Properties deviceProperties2;

	VkPhysicalDeviceMemoryProperties deviceMemProperties;
	VkDeviceSize hostMemSize, localMemSize;
	uint32_t hostMemIndex, localMemIndex;

	uint32_t deviceIndex;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	uint32_t graphicsQueueIndex;
	uint32_t computeQueueIndex;

	VkQueue graphicsQueue;
	VkQueue computeQueue;

	VkPipelineCache pipelineCache;

	VkCommandPool commandPool;
} VkuContext_t;

typedef struct
{
	// Handles to dependencies
	VkDevice device;
	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkRenderPass renderPass;

	// Pipeline to be created
	VkPipeline pipeline;

	// What subpass
	uint32_t subpass;

	// Settable states:

	// Vertex bindings
	uint32_t numVertexBindings;
	VkVertexInputBindingDescription vertexBindings[VKU_MAX_PIPELINE_VERTEX_BINDINGS];

	// Vertex attributes
	uint32_t numVertexAttribs;
	VkVertexInputAttributeDescription vertexAttribs[VKU_MAX_PIPELINE_VERTEX_ATTRIBUTES];

	// Shader Stages
	uint32_t numStages;
	VkPipelineShaderStageCreateInfo stages[VKU_MAX_PIPELINE_SHADER_STAGES];

	// Input assembly state
	VkPrimitiveTopology topology;
	VkBool32 primitiveRestart;

	// Rasterization state
	VkBool32 depthClamp;
	VkBool32 rasterizerDiscard;
	VkPolygonMode polygonMode;
	VkCullModeFlags cullMode;
	VkFrontFace frontFace;
	VkBool32 depthBias;
	float depthBiasConstantFactor;
	float depthBiasClamp;
	float depthBiasSlopeFactor;
	float lineWidth;

	// Depth/stencil state
	VkBool32 depthTest;
	VkBool32 depthWrite;
	VkCompareOp depthCompareOp;
	VkBool32 depthBoundsTest;
	VkBool32 stencilTest;
	float minDepthBounds;
	float maxDepthBounds;

	// Front face stencil functions
	VkStencilOp frontStencilFailOp;
	VkStencilOp frontStencilPassOp;
	VkStencilOp frontStencilDepthFailOp;
	VkCompareOp frontStencilCompareOp;
	uint32_t frontStencilCompareMask;
	uint32_t frontStencilWriteMask;
	uint32_t frontStencilReference;

	// Back face stencil functions
	VkStencilOp backStencilFailOp;
	VkStencilOp backStencilPassOp;
	VkStencilOp backStencilDepthFailOp;
	VkCompareOp backStencilCompareOp;
	uint32_t backStencilCompareMask;
	uint32_t backStencilWriteMask;
	uint32_t backStencilReference;

	// Multisample state
	VkSampleCountFlags rasterizationSamples;
	VkBool32 sampleShading;
	float minSampleShading;
	const VkSampleMask *sampleMask;
	VkBool32 alphaToCoverage;
	VkBool32 alphaToOne;

	// blend state
	VkBool32 blendLogicOp;
	VkLogicOp blendLogicOpState;
	VkBool32 blend;
	VkBlendFactor srcColorBlendFactor;
	VkBlendFactor dstColorBlendFactor;
	VkBlendOp colorBlendOp;
	VkBlendFactor srcAlphaBlendFactor;
	VkBlendFactor dstAlphaBlendFactor;
	VkBlendOp alphaBlendOp;
	VkColorComponentFlags colorWriteMask;
} VkuPipeline_t;

typedef struct
{
	VkDevice device;

	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	uint32_t numBindings;
	VkDescriptorSetLayoutBinding bindings[VKU_MAX_DESCRIPTORSET_BINDINGS];
	VkWriteDescriptorSet writeDescriptorSet[VKU_MAX_DESCRIPTORSET_BINDINGS];
	VkDescriptorImageInfo imageInfo[VKU_MAX_DESCRIPTORSET_BINDINGS];
	VkDescriptorBufferInfo bufferInfo[VKU_MAX_DESCRIPTORSET_BINDINGS];
} VkuDescriptorSet_t;

typedef struct VkuMemBlock_s
{
	size_t offset;
	size_t size;
	bool free;
	VkDeviceMemory deviceMemory;
	void *mappedPointer;
	struct VkuMemBlock_s *next, *prev;
} VkuMemBlock_t;

typedef struct
{
	size_t size;
	VkuMemBlock_t *blocks;
	VkDeviceMemory deviceMemory;
	void *mappedPointer;
} VkuMemZone_t;

typedef struct
{
	VkBuffer buffer;
	VkuMemBlock_t *memory;
} VkuBuffer_t;

typedef struct
{
	uint32_t width, height, depth;
	uint8_t *data;

	VkSampler sampler;
	VkImage image;
	VkuMemBlock_t *memory;
	VkImageView imageView;
} VkuImage_t;

typedef struct
{
	VkSwapchainKHR swapchain;

	VkExtent2D extent;
	VkSurfaceFormatKHR surfaceFormat;

	uint32_t numImages;
	VkImage image[VKU_MAX_FRAME_COUNT];
	VkImageView imageView[VKU_MAX_FRAME_COUNT];
} VkuSwapchain_t;

typedef struct
{
	VkDevice device;
	VkRenderPass renderPass;

	VkFramebuffer framebuffer;

	uint32_t numAttachments;
	VkImageView attachments[VKU_MAX_FRAMEBUFFER_ATTACHMENTS];
} VkuFramebuffer_t;

typedef enum
{
	VKU_RENDERPASS_ATTACHMENT_INPUT=0,
	VKU_RENDERPASS_ATTACHMENT_COLOR,
	VKU_RENDERPASS_ATTACHMENT_DEPTH,
	VKU_RENDERPASS_ATTACHMENT_RESOLVE
} VkuRenderPassAttachmentType;

typedef struct
{
	VkuRenderPassAttachmentType type;
	VkFormat format;
	VkSampleCountFlagBits samples;
	VkAttachmentLoadOp loadOp;
	VkAttachmentStoreOp storeOp;
	VkImageLayout subpassLayout;
	VkImageLayout finalLayout;
} VkuRenderPassAttachments_t;

typedef struct
{
	VkDevice device;

	VkRenderPass renderPass;

	uint32_t numAttachments;
	VkuRenderPassAttachments_t attachments[VKU_MAX_RENDERPASS_ATTACHMENTS];

	uint32_t numSubpassDependencies;
	VkSubpassDependency subpassDependencies[VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES];
} VkuRenderPass_t;

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask);

VkBool32 vkuCreateImageBuffer(VkuContext_t* Context, VkuImage_t* image, VkImageType imageType, VkFormat format, uint32_t mipLevels, uint32_t layers, uint32_t width, uint32_t height, uint32_t depth, VkSampleCountFlagBits samples, VkImageTiling tiling, VkBufferUsageFlags flags, VkFlags requirementsMask, VkImageCreateFlags createFlags);
void vkuCreateTexture2D(VkuContext_t *context, VkuImage_t *image, uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits samples);
void vkuDestroyImageBuffer(VkuContext_t *context, VkuImage_t *image);

VkBool32 vkuCreateHostBuffer(VkuContext_t *context, VkuBuffer_t *buffer, uint32_t size, VkBufferUsageFlags flags);
VkBool32 vkuCreateGPUBuffer(VkuContext_t *context, VkuBuffer_t *buffer, uint32_t size, VkBufferUsageFlags flags);
void vkuDestroyBuffer(VkuContext_t *context, VkuBuffer_t *buffer);

void vkuTransitionLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t levelCount, uint32_t baseLevel, uint32_t layerCount, uint32_t baseLayer, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout);

VkCommandBuffer vkuOneShotCommandBufferBegin(VkuContext_t *context);
VkBool32 vkuOneShotCommandBufferFlush(VkuContext_t *context, VkCommandBuffer commandBuffer);
VkBool32 vkuOneShotCommandBufferEnd(VkuContext_t *context, VkCommandBuffer commandBuffer);

VkShaderModule vkuCreateShaderModuleMemory(VkDevice device, const uint32_t *data, const uint32_t size);
VkShaderModule vkuCreateShaderModule(VkDevice device, const char *shaderFile);

VkBool32 vkuPipeline_AddVertexBinding(VkuPipeline_t *pipeline, uint32_t binding, uint32_t stride, VkVertexInputRate inputRate);
VkBool32 vkuPipeline_AddVertexAttribute(VkuPipeline_t *pipeline, uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
VkBool32 vkuPipeline_AddStageMemory(VkuPipeline_t *pipeline, const uint32_t *data, const uint32_t size, VkShaderStageFlagBits stage);
VkBool32 vkuPipeline_AddStage(VkuPipeline_t *pipeline, const char *shaderFilename, VkShaderStageFlagBits stage);
VkBool32 vkuPipeline_SetRenderPass(VkuPipeline_t *pipeline, VkRenderPass renderPass);
VkBool32 vkuPipeline_SetPipelineLayout(VkuPipeline_t *pipeline, VkPipelineLayout pipelineLayout);
VkBool32 vkuInitPipeline(VkuPipeline_t *pipeline, VkDevice device, VkPipelineCache pipelineCache);
VkBool32 vkuAssemblePipeline(VkuPipeline_t *pipeline, void *pNext);
VkBool32 vkuAssembleComputePipeline(VkuPipeline_t *pipeline, void *pNext);

VkBool32 vkuDescriptorSet_AddBinding(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage);
VkBool32 vkuDescriptorSet_UpdateBindingImageInfo(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
VkBool32 vkuDescriptorSet_UpdateBindingBufferInfo(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
VkBool32 vkuInitDescriptorSet(VkuDescriptorSet_t *descriptorSet, VkDevice device);
VkBool32 vkuAssembleDescriptorSetLayout(VkuDescriptorSet_t *descriptorSet);
VkBool32 vkuAllocateUpdateDescriptorSet(VkuDescriptorSet_t *descriptorSet, VkDescriptorPool descriptorPool);

bool vkuMem_Init(VkuContext_t *context, VkuMemZone_t *zone, uint32_t typeIndex, size_t size);
void vkuMem_Destroy(VkuContext_t *context, VkuMemZone_t *vkZone);
void vkuMem_Free(VkuMemZone_t *vkZone, VkuMemBlock_t *ptr);
VkuMemBlock_t *vkuMem_Malloc(VkuMemZone_t *vkZone, VkMemoryRequirements requirements);
void vkuMem_Print(VkuMemZone_t *vkZone);

VkBool32 vkuCreateInstance(VkInstance *instance);
void vkuDestroyInstance(VkInstance instance);

VkBool32 vkuCreateContext(VkInstance instance, VkuContext_t *context);
void vkuDestroyContext(VkInstance instance, VkuContext_t *context);

VkBool32 vkuCreateSwapchain(VkuContext_t *context, VkuSwapchain_t *swapchain, VkBool32 vSync);
void vkuDestroySwapchain(VkuContext_t *context, VkuSwapchain_t *swapchain);

VkBool32 vkuFramebufferAddAttachment(VkuFramebuffer_t *framebuffer, VkImageView attachment);
VkBool32 vkuInitFramebuffer(VkuFramebuffer_t *framebuffer, VkuContext_t *context, VkRenderPass renderPass);
VkBool32 vkuFramebufferCreate(VkuFramebuffer_t *framebuffer, VkExtent2D size);

VkBool32 vkuRenderPass_AddAttachment(VkuRenderPass_t *renderPass, VkuRenderPassAttachmentType type, VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout subpassLayout, VkImageLayout finalLayout);
VkBool32 vkuRenderPass_AddSubpassDependency(VkuRenderPass_t *renderPass, uint32_t sourceSubpass, uint32_t destinationSubpass, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destinationStageMask, VkAccessFlags sourceAccessMask, VkAccessFlags destinationAccessMask, VkDependencyFlags dependencyFlags);
VkBool32 vkuInitRenderPass(VkuContext_t *context, VkuRenderPass_t *renderPass);
VkBool32 vkuCreateRenderPass(VkuRenderPass_t *renderPass);

bool vkuMemAllocator_Init(VkuContext_t *context);
void vkuMemAllocator_Destroy(void);
VkuMemBlock_t *vkuMemAllocator_Malloc(VkMemoryRequirements memoryRequirements);
void vkuMemAllocator_Free(VkuMemBlock_t *block);
void vkuMemAllocator_Print(void);

#endif
