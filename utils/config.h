#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>
#include <stdint.h>
#include "../vulkan/vulkan.h"

typedef struct
{
	// From config.ini
	uint32_t windowWidth;
	uint32_t windowHeight;
	uint32_t msaaSamples;
	uint32_t deviceIndex;

	// System config states
	uint32_t renderWidth;
	uint32_t renderHeight;

	VkSampleCountFlags MSAA;
	VkFormat colorFormat;
	VkFormat depthFormat;

	bool isVR;
} Config_t;

bool Config_ReadINI(Config_t *config, const char *filename);

#endif
