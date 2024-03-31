#ifndef __PIPELINE_H__
#define __PIPELINE_H__

typedef struct
{
	VkuDescriptorSet_t descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkuPipeline_t pipeline;
} Pipeline_t;

bool CreatePipeline(VkuContext_t *context, Pipeline_t *pipeline, VkRenderPass renderPass, const char *filename);

#endif
