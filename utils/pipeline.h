#ifndef __PIPELINE_H__
#define __PIPELINE_H__

#include "../vulkan/vulkan.h"

typedef struct
{
	VkuDescriptorSet_t descriptorSet;

	VkPushConstantRange pushConstant;
	VkPipelineLayout pipelineLayout;

	VkuPipeline_t pipeline;
} Pipeline_t;

void PipelineOverrideRasterizationSamples(const VkSampleCountFlags rasterizationsamples);
bool CreatePipeline(VkuContext_t *context, Pipeline_t *pipeline, VkRenderPass renderPass, const char *filename);
void DestroyPipeline(VkuContext_t *context, Pipeline_t *pipeline);

#endif
