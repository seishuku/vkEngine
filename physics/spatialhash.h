#ifndef __SPATIALHASH_H__
#define __SPATIALHASH_H__

#ifndef HASH_TABLE_SIZE
#define HASH_TABLE_SIZE (NUM_ASTEROIDS/2)
#endif

#ifndef GRID_SIZE
#define GRID_SIZE 50.0f
#endif

typedef struct
{
	uint32_t numObjects;
	PhysicsObject_t *objects[100];
} Cell_t;

typedef struct
{
	Cell_t hashTable[HASH_TABLE_SIZE];
} SpatialHash_t;

void SpatialHash_Clear(SpatialHash_t *spatialHash);
bool SpatialHash_AddPhysicsObject(SpatialHash_t *spatialHash, PhysicsObject_t *physicsObject);
void SpatialHash_TestObjects(SpatialHash_t *spatialHash, PhysicsObject_t *physicsObject, void (*collisionFunc)(PhysicsObject_t *objA, PhysicsObject_t *objB));

#endif
