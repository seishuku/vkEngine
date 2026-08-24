#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"

#define POSTPROCESS_MAX_EFFECTS 8

typedef struct PostProcessEffect_s PostProcessEffect_t;

struct PostProcessEffect_s
{
	const char *name;

	bool (*Create)(PostProcessEffect_t *self);
	void (*Destroy)(PostProcessEffect_t *self);

	bool (*CreateFramebuffers)(PostProcessEffect_t *self, uint32_t width, uint32_t height);
	void (*DestroyFramebuffers)(PostProcessEffect_t *self);

	void (*Draw)(PostProcessEffect_t *self, VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t eye, VkImageView inputView, VkSampler inputSampler);

	VkImageView (*GetOutputView)(PostProcessEffect_t *self, uint32_t eye);
	VkSampler (*GetOutputSampler)(PostProcessEffect_t *self, uint32_t eye);

	bool enabled;
	void *data;
};

void PostProcess_Register(PostProcessEffect_t *effect);

bool PostProcess_CreateAll(void);
void PostProcess_DestroyAll(void);

bool PostProcess_CreateFramebuffers(uint32_t width, uint32_t height);
void PostProcess_DestroyFramebuffers(void);

void PostProcess_Draw(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t eye, VkImageView sourceView, VkSampler sourceSampler, VkImageView *outView, VkSampler *outSampler);

uint32_t PostProcess_GetCount(void);
PostProcessEffect_t *PostProcess_GetByIndex(uint32_t index);
PostProcessEffect_t *PostProcess_GetByName(const char *name);

#endif
