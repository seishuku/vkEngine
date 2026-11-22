#ifndef __LIGHTS_H__
#define __LIGHTS_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "../utils/pipeline.h"
#include "../utils/entitylist.h"
#include "../vulkan/vulkan.h"
#include "../perframe.h"

typedef struct
{
	uint32_t ID;
	int32_t shadowSlot;
	uint32_t pad[2];

	vec3 position;
	float radius; // Stored as 1/radius, which is what the shader wants
	vec4 Kd;

	vec4 spotDirection;
	float spotOuterCone;
	float spotInnerCone;
	float spotExponent;
	float spotPad;
} Light_t;

typedef struct
{
	ID_t baseID;

	List_t lights;

	VkuBuffer_t buffer[FRAMES_IN_FLIGHT];
	uint32_t bufferSize[FRAMES_IN_FLIGHT];

	uint32_t numShadows, shadowSize;
	uint32_t *shadowOwner;
	matrix *shadowMatrix;
	VkuBuffer_t shadowBuffer[FRAMES_IN_FLIGHT];
	VkuImage_t shadowDepth;

	VkFramebuffer *shadowFrameBuffer;
	VkImageView *shadowView;
	VkRenderPass shadowRenderPass;
	Pipeline_t shadowPipeline;
} Lights_t;

bool Lights_Init(Lights_t *lights, uint32_t numShadows, uint32_t shadowSize);
void Lights_Destroy(Lights_t *lights);

uint32_t Lights_Add(Lights_t *lights, vec3 position, float radius, vec4 Kd);
void Lights_Del(Lights_t *lights, uint32_t ID);

void Lights_Update(Lights_t *lights, uint32_t ID, vec3 position, float radius, vec4 Kd);
void Lights_UpdatePosition(Lights_t *lights, uint32_t ID, vec3 position);
void Lights_UpdateRadius(Lights_t *lights, uint32_t ID, float radius);
void Lights_UpdateKd(Lights_t *lights, uint32_t ID, vec4 Kd);
void Lights_UpdateSpotlight(Lights_t *lights, uint32_t ID, vec3 direction, float outerCone, float innerCone, float exponent);

bool Lights_SetShadow(Lights_t *lights, uint32_t ID, bool enable);

void Lights_UpdateSSBO(Lights_t *lights, uint32_t frameIndex);

void Lights_UpdateShadowMaps(Lights_t *lights, VkCommandBuffer commandBuffer, const EntityList_t *entityList, uint32_t frameIndex, VkDescriptorPool descriptorPool);

#endif
