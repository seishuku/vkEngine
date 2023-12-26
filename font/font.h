#ifndef __FONT_H__
#define __FONT_H__

#include "../vulkan/vulkan.h"
#include "../math/math.h"

typedef struct
{
	VkuDescriptorSet_t descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkuPipeline_t pipeline;

	// Vertex data handles
	VkuBuffer_t vertexBuffer;

	// Instance data handles
	VkuBuffer_t instanceBuffer;
	void *instanceBufferPtr;

	// Global running instance buffer pointer, resets on submit
	vec4 *instance;
	uint32_t numChar;
} Font_t;

bool Font_Init(Font_t *font);
float Font_CharacterBaseWidth(const char ch);
float Font_StringBaseWidth(const char *string);
void Font_Print(Font_t *font, float size, float x, float y, char *string, ...);
void Font_Draw(Font_t *font, uint32_t index, uint32_t eye);
void Font_Reset(Font_t *font);
void Font_Destroy(Font_t *font);

#endif
