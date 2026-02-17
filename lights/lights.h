#ifndef __LIGHTS_H__
#define __LIGHTS_H__

#include <stdint.h>
#include "../math/math.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "../vulkan/vulkan.h"

typedef struct
{
	uint32_t ID;
	uint32_t pad[3];

	vec3 position;
	float radius;
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
	VkuBuffer_t buffer;

	VkFramebuffer shadowFrameBuffer;
	VkuImage_t shadowDepth;

	VkuBuffer_t shadowUBOBuffer;
} Lights_t;

uint32_t Lights_Add(Lights_t *lights, vec3 position, float radius, vec4 Kd);
void Lights_Del(Lights_t *lights, uint32_t ID);

void Lights_Update(Lights_t *lights, uint32_t ID, vec3 position, float radius, vec4 Kd);
void Lights_UpdatePosition(Lights_t *lights, uint32_t ID, vec3 position);
void Lights_UpdateRadius(Lights_t *lights, uint32_t ID, float radius);
void Lights_UpdateKd(Lights_t *lights, uint32_t ID, vec4 Kd);
void Lights_UpdateSpotlight(Lights_t *lights, uint32_t ID, vec3 direction, float outerCone, float innerCone, float exponent);

void Lights_UpdateSSBO(Lights_t *lights);
bool Lights_Init(Lights_t *lights);
void Lights_Destroy(Lights_t *lights);

#endif
