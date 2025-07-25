#ifndef __TEXTURES_H__
#define __TEXTURES_H__

#include "vulkan/vulkan.h"

typedef enum
{
	TEXTURE_ASTEROID1=0,
	TEXTURE_ASTEROID1_NORMAL,
	TEXTURE_ASTEROID2,
	TEXTURE_ASTEROID2_NORMAL,
	TEXTURE_ASTEROID3,
	TEXTURE_ASTEROID3_NORMAL,
	TEXTURE_ASTEROID4,
	TEXTURE_ASTEROID4_NORMAL,
	TEXTURE_VOLUME,
	TEXTURE_CROSSHAIR,
	TEXTURE_FIGHTER1,
	TEXTURE_FIGHTER1_NORMAL,
	TEXTURE_FIGHTER2,
	TEXTURE_FIGHTER2_NORMAL,
	TEXTURE_FIGHTER3,
	TEXTURE_FIGHTER3_NORMAL,
	TEXTURE_FIGHTER4,
	TEXTURE_FIGHTER4_NORMAL,
	TEXTURE_FIGHTER5,
	TEXTURE_FIGHTER5_NORMAL,
	TEXTURE_FIGHTER6,
	TEXTURE_FIGHTER6_NORMAL,
	TEXTURE_FIGHTER7,
	TEXTURE_FIGHTER7_NORMAL,
	TEXTURE_FIGHTER8,
	TEXTURE_FIGHTER8_NORMAL,
	TEXTURE_CUBE,
	TEXTURE_CUBE_NORMAL,
	NUM_TEXTURES
} TextureIDs;

typedef struct
{
	const char *filename;
	uint32_t flags;
	VkuImage_t image;
} Textures_t;

extern Textures_t textures[NUM_TEXTURES];

#endif
