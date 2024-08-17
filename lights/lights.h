#ifndef __LIGHTS_H__
#define __LIGHTS_H__

#include <stdint.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../vulkan/vulkan.h"

typedef struct
{
	uint32_t ID;

	uint32_t Pad[3];

	vec3 Position;
	float Radius;
	vec4 Kd;

	vec4 SpotDirection;
	float SpotOuterCone;
	float SpotInnerCone;
	float SpotExponent;
	float SpotPad;
} Light_t;

typedef struct
{
	List_t Lights;
	VkuBuffer_t StorageBuffer;
} Lights_t;

uint32_t Lights_Add(Lights_t *Lights, vec3 Position, float Radius, vec4 Kd);
void Lights_Del(Lights_t *Lights, uint32_t ID);

void Lights_Update(Lights_t *Lights, uint32_t ID, vec3 Position, float Radius, vec4 Kd);
void Lights_UpdatePosition(Lights_t *Lights, uint32_t ID, vec3 Position);
void Lights_UpdateRadius(Lights_t *Lights, uint32_t ID, float Radius);
void Lights_UpdateKd(Lights_t *Lights, uint32_t ID, vec4 Kd);
void Lights_UpdateSpotlight(Lights_t *Lights, uint32_t ID, vec3 Direction, float OuterCone, float InnerCone, float Exponent);

void Lights_UpdateSSBO(Lights_t *Lights);
bool Lights_Init(Lights_t *Lights);
void Lights_Destroy(Lights_t *Lights);

#endif
