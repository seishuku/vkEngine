#ifndef __SKYBOX_H__
#define __SKYBOX_H__

typedef struct
{
	matrix HMD;
	matrix projection;
	matrix modelview;
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

extern Skybox_UBO_t *Skybox_UBO[2];
extern VkuBuffer_t Skybox_UBO_Buffer[2];

extern VkuDescriptorSet_t SkyboxDescriptorSet[VKU_MAX_FRAME_COUNT];

extern VkPipelineLayout SkyboxPipelineLayout;
extern VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void);
//void DrawSkybox(VkCommandBuffer CommandBuffer, uint32_t Index);
void DestroySkybox(void);

#endif
