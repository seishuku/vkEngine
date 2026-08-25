#include <string.h>
#include "postprocess.h"

static PostProcessEffect_t *effectChain[POSTPROCESS_MAX_EFFECTS];
static uint32_t numEffects=0;

void PostProcess_Register(PostProcessEffect_t *effect)
{
	if(numEffects>=POSTPROCESS_MAX_EFFECTS)
		return;

	effectChain[numEffects++]=effect;
}

bool PostProcess_CreateAll(void)
{
	for(uint32_t i=0;i<numEffects;i++)
	{
		if(!effectChain[i]->Create(effectChain[i]))
			return false;
	}

	return true;
}

void PostProcess_DestroyAll(void)
{
	for(uint32_t i=0;i<numEffects;i++)
		effectChain[i]->Destroy(effectChain[i]);
}

bool PostProcess_CreateFramebuffers(uint32_t width, uint32_t height)
{
	for(uint32_t i=0;i<numEffects;i++)
	{
		if(!effectChain[i]->CreateFramebuffers(effectChain[i], width, height))
			return false;
	}

	return true;
}

void PostProcess_DestroyFramebuffers(void)
{
	for(uint32_t i=0;i<numEffects;i++)
		effectChain[i]->DestroyFramebuffers(effectChain[i]);
}

void PostProcess_Draw(VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t eye)
{
	for(uint32_t i=0;i<numEffects;i++)
	{
		PostProcessEffect_t *effect=effectChain[i];

		if(!effect->enabled)
			continue;

		effect->Draw(effect, commandBuffer, frameIndex, eye);
	}
}

uint32_t PostProcess_GetCount(void)
{
	return numEffects;
}

PostProcessEffect_t *PostProcess_GetByIndex(uint32_t index)
{
	if(index>=numEffects)
		return NULL;

	return effectChain[index];
}

PostProcessEffect_t *PostProcess_GetByName(const char *name)
{
	for(uint32_t i=0;i<numEffects;i++)
	{
		if(strcmp(effectChain[i]->name, name)==0)
			return effectChain[i];
	}

	return NULL;
}
