#ifndef __SKYBOX_H__
#define __SKYBOX_H__

#include "vulkan/vulkan.h"
#include "math/math.h"

typedef struct
{
	vec4 uOffset;

	vec3 uNebulaAColor;
	float uNebulaADensity;
	vec3 uNebulaBColor;
	float uNebulaBDensity;

	float uStarsScale;
	float uStarDensity;
	float pad0[2];

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad1[2];
	vec4 uSunColor;
} Skybox_UBO_t;

extern VkuDescriptorSet_t SkyboxDescriptorSet;
extern VkPipelineLayout SkyboxPipelineLayout;
extern VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void);
void DestroySkybox(void);

#endif
