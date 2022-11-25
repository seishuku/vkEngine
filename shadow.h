#ifndef __SHADOW_H__
#define __SHADOW_H__

typedef struct
{
	matrix mvp;
	matrix local;
} Shadow_UBO_t;

extern Shadow_UBO_t Shadow_UBO;

extern VkuImage_t ShadowDepth;

void InitShadowMap(void);
bool InitShadowPipeline(void);
void ShadowUpdateMap(VkCommandBuffer CommandBuffer, uint32_t FrameIndex);
void DestroyShadow(void);

#endif
