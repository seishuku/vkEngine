#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "spatialhash.h"

bool SpatialHash_Create(SpatialHash_t *spatialHash, const uint32_t tableSize, const float gridSize)
{
	if(spatialHash==NULL)
		return false;

	if(!tableSize)
		return false;

	// TODO: Should smaller grid sizes be allowed?
	if(gridSize<1.0f)
		return false;

	spatialHash->gridSize=gridSize;
	spatialHash->invGridSize=1.0f/gridSize;

	spatialHash->hashTableSize=tableSize;

	spatialHash->hashTable=(Cell_t *)Zone_Malloc(zone, sizeof(Cell_t)*tableSize);

	if(spatialHash->hashTable==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for hash table.\n");
		return false;
	}

	return true;
}

void SpatialHash_Destroy(SpatialHash_t *spatialHash)
{
	Zone_Free(zone, spatialHash->hashTable);
	memset(spatialHash, 0, sizeof(SpatialHash_t));
}

static inline uint32_t hashFunction(const int32_t hx, const int32_t hy, const int32_t hz, const uint32_t tableSize)
{
	return abs((hx*73856093)^(hy*19349663)^(hz*83492791))%tableSize;
}

// Clear hash table
void SpatialHash_Clear(SpatialHash_t *spatialHash)
{
	memset(spatialHash->hashTable, 0, sizeof(Cell_t)*spatialHash->hashTableSize);
}

// Add object to table
bool SpatialHash_AddObject(SpatialHash_t *spatialHash, const vec3 value, void *object)
{
	const int32_t hx=(const int32_t)(value.x*spatialHash->invGridSize);
	const int32_t hy=(const int32_t)(value.y*spatialHash->invGridSize);
	const int32_t hz=(const int32_t)(value.z*spatialHash->invGridSize);
	const uint32_t index=hashFunction(hx, hy, hz, spatialHash->hashTableSize);

	if(index>spatialHash->hashTableSize)
	{
		DBGPRINTF(DEBUG_ERROR, "Invalid hash index.\n");
		return false;
	}

	if(spatialHash->hashTable[index].numObjects<100)
	{
		spatialHash->hashTable[index].objects[spatialHash->hashTable[index].numObjects++]=object;
		return true;
	}

	DBGPRINTF(DEBUG_ERROR, "Ran out of bucket space.\n");
	return false;
}

void SpatialHash_TestObjects(SpatialHash_t *spatialHash, vec3 value, void *a, void (*testFunc)(void *a, void *b))
{
	// Neighbor cell offsets
	const int32_t offsets[27][3]=
	{
		{-1,-1,-1 }, {-1,-1, 0 }, {-1,-1, 1 },
		{-1, 0,-1 }, {-1, 0, 0 }, {-1, 0, 1 },
		{-1, 1,-1 }, {-1, 1, 0 }, {-1, 1, 1 },
		{ 0,-1,-1 }, { 0,-1, 0 }, { 0,-1, 1 },
		{ 0, 0,-1 }, { 0, 0, 0 }, { 0, 0, 1 },
		{ 0, 1,-1 }, { 0, 1, 0 }, { 0, 1, 1 },
		{ 1,-1,-1 }, { 1,-1, 0 }, { 1,-1, 1 },
		{ 1, 0,-1 }, { 1, 0, 0 }, { 1, 0, 1 },
		{ 1, 1,-1 }, { 1, 1, 0 }, { 1, 1, 1 }
	};

	// Object hash position
	const int32_t hx=(const int32_t)(value.x*spatialHash->invGridSize);
	const int32_t hy=(const int32_t)(value.y*spatialHash->invGridSize);
	const int32_t hz=(const int32_t)(value.z*spatialHash->invGridSize);

	// Iterate over cell offsets
	for(uint32_t j=0;j<27;j++)
	{
		const uint32_t hashIndex=hashFunction(hx+offsets[j][0], hy+offsets[j][1], hz+offsets[j][2], spatialHash->hashTableSize);

		if(hashIndex>spatialHash->hashTableSize)
		{
			DBGPRINTF(DEBUG_ERROR, "Invalid hash index.\n");
			return;
		}

		Cell_t *neighborCell=&spatialHash->hashTable[hashIndex];

		// Iterate over objects in the neighbor cell
		for(uint32_t k=0;k<neighborCell->numObjects;k++)
		{
			// Object 'B'
			void *b=neighborCell->objects[k];

			if(a==b)
				continue;

			if(testFunc)
				testFunc(a, b);
		}
	}
}
