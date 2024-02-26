#ifndef __SKYBOX_H__
#define __SKYBOX_H__

#include "vulkan/vulkan.h"
#include "math/math.h"

typedef struct
{
	vec4 uOffset;

	// To take advantage of the unused float in a vec3,
	//   I'm doing some union/struct shenanigans here because vectors are all 16 byte aligned.
	union
	{
		vec3 uNebulaAColor;
		struct
		{
			float pad0[3]; // Padded out to locate it correctly.
			float uNebulaADensity;
		};
	};
	union
	{
		vec3 uNebulaBColor;
		struct
		{
			float pad1[3]; // Padded out to locate it correctly.
			float uNebulaBDensity;
		};
	};

	float uStarsScale;
	float uStarDensity;
	float pad2[2];

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad3[2];
	vec4 uSunColor;
} Skybox_UBO_t;

extern VkuDescriptorSet_t skyboxDescriptorSet;
extern VkPipelineLayout skyboxPipelineLayout;
extern VkuPipeline_t skyboxPipeline;

bool CreateSkyboxPipeline(void);
void DestroySkybox(void);
void DrawSkybox(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
