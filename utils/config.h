#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	uint32_t windowWidth;
	uint32_t windowHeight;
	uint32_t msaaSamples;
	uint32_t deviceIndex;
} Config_t;

bool Config_ReadINI(Config_t *config, const char *filename);

#endif
