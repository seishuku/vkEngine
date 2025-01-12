#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "physics.h"
#include "physicslist.h"
#include "particle.h"
#include "spatialhash.h"

static inline uint32_t hashFunction(int32_t hx, int32_t hy, int32_t hz)
{
	return abs((hx*73856093)^(hy*19349663)^(hz*83492791))%HASH_TABLE_SIZE;
}

// Clear hash table
void SpatialHash_Clear(SpatialHash_t *spatialHash)
{
	memset(spatialHash, 0, sizeof(SpatialHash_t));
}

// Add physics object to table
bool SpatialHash_AddPhysicsObject(SpatialHash_t *spatialHash, PhysicsObject_t *physicsObject)
{
	int32_t hx=(int32_t)(physicsObject->rigidBody->position.x/GRID_SIZE);
	int32_t hy=(int32_t)(physicsObject->rigidBody->position.y/GRID_SIZE);
	int32_t hz=(int32_t)(physicsObject->rigidBody->position.z/GRID_SIZE);
	uint32_t index=hashFunction(hx, hy, hz);

	if(spatialHash->hashTable[index].numObjects<100)
	{
		spatialHash->hashTable[index].objects[spatialHash->hashTable[index].numObjects++]=physicsObject;
		return true;
	}

	DBGPRINTF(DEBUG_ERROR, "Ran out of bucket space.\n");
	return false;
}

void SpatialHash_TestObjects(SpatialHash_t *spatialHash, PhysicsObject_t *physicsObject, void (*collisionFunc)(PhysicsObject_t *objA, PhysicsObject_t *objB))
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

	// Check physics object collision against neighboring cells
	// Object hash position
	int32_t hx=(int32_t)(physicsObject->rigidBody->position.x/GRID_SIZE);
	int32_t hy=(int32_t)(physicsObject->rigidBody->position.y/GRID_SIZE);
	int32_t hz=(int32_t)(physicsObject->rigidBody->position.z/GRID_SIZE);

	// Iterate over cell offsets
	for(uint32_t j=0;j<27;j++)
	{
		uint32_t hashIndex=hashFunction(hx+offsets[j][0], hy+offsets[j][1], hz+offsets[j][2]);
		Cell_t *neighborCell=&spatialHash->hashTable[hashIndex];

		// Iterate over objects in the neighbor cell
		for(uint32_t k=0;k<neighborCell->numObjects;k++)
		{
			// Object 'B'
			PhysicsObject_t *objB=neighborCell->objects[k];

			if(physicsObject->rigidBody==objB->rigidBody)
				continue;

			if(collisionFunc)
				collisionFunc(physicsObject, objB);
		}
	}
}
