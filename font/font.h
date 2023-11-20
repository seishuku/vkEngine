#ifndef __FONT_H__
#define __FONT_H__

#include "../vulkan/vulkan.h"
#include "../math/math.h"

typedef struct
{
	VkuDescriptorSet_t DescriptorSet;
	VkPipelineLayout PipelineLayout;
	VkuPipeline_t Pipeline;

	// Vertex data handles
	VkuBuffer_t VertexBuffer;

	// Instance data handles
	VkuBuffer_t InstanceBuffer;
	void *InstanceBufferPtr;

	// Global running instance buffer pointer, resets on submit
	vec4 *Instance;
	uint32_t NumChar;
} Font_t;

bool Font_Init(Font_t *Font);
float Font_CharacterBaseWidth(const char ch);
float Font_StringBaseWidth(const char *string);
void Font_Print(Font_t *Font, float size, float x, float y, char *string, ...);
void Font_Draw(Font_t *Font, uint32_t Index);
void Font_Destroy(Font_t *Font);

#endif
