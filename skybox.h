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

extern VkPipelineLayout SkyboxPipelineLayout;
extern VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void);
void DestroySkybox(void);

#endif
