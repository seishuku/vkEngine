#ifndef __SPATIALHASH_H__
#define __SPATIALHASH_H__

#include <stdbool.h>
#include <stdint.h>
#include "../math/math.h"

#ifndef MAX_SPATIALHASH_CELL_OBJECTS
#define MAX_SPATIALHASH_CELL_OBJECTS 1000
#endif

typedef struct
{
	uint32_t numObjects;
	void *objects[MAX_SPATIALHASH_CELL_OBJECTS];
} Cell_t;

typedef struct
{
	uint32_t hashTableSize;
	float gridSize, invGridSize;
	Cell_t *hashTable;
} SpatialHash_t;

bool SpatialHash_Create(SpatialHash_t *spatialHash, const uint32_t tableSize, const float gridSize);
void SpatialHash_Destroy(SpatialHash_t *spatialHash);
void SpatialHash_Clear(SpatialHash_t *spatialHash);
bool SpatialHash_AddObject(SpatialHash_t *spatialHash, const vec3 value, void *object);
void SpatialHash_TestObjects(SpatialHash_t *spatialHash, vec3 value, void *a, void (*testFunc)(void *a, void *b));

#endif
