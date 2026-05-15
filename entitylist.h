#ifndef __ENTITYLIST_H__
#define __ENTITYLIST_H__

#include <stdbool.h>
#include <stdint.h>
#include "math/math.h"
#include "physics/physics.h"
#include "vulkan/vulkan.h"

#define MAX_ENTITY 10000

typedef matrix (*EntityTransformFunc)(const RigidBody_t *body);

typedef enum
{
	ENTITYOBJECTTYPE_PLAYER,
	ENTITYOBJECTTYPE_FIELD,
	ENTITYOBJECTTYPE_PROJECTILE,
} EntityObjectType_e;

typedef struct
{
	aabb bounds;
	RigidBody_t *body;
	EntityObjectType_e objectType;

	uint32_t modelID, textureIDs[2];

	EntityTransformFunc transformFunc;
} Entity_t;

typedef struct
{
	uint32_t modelID;
	uint32_t textureIDs[2];
	uint32_t instanceOffset;
	uint32_t instanceCount;
} EntityBatch_t;

typedef struct
{
	Entity_t entities[MAX_ENTITY];
	uint32_t entityCount;

	uint32_t sortedIndices[MAX_ENTITY];
	uint32_t sortedCount;

	EntityBatch_t *batches;
	uint32_t batchCount;
	uint32_t batchCapacity;

	bool dirty;

	struct
	{
		VkuBuffer_t instanceBuffer;
		matrix *instancePtr;
	} perFrame[8];
} EntityList_t;

bool EntityList_Init(EntityList_t *list);
void EntityList_Destroy(EntityList_t *list);

bool EntityList_Add(EntityList_t *list, RigidBody_t *body, uint32_t modelID, uint32_t tex0, uint32_t tex1, EntityObjectType_e objectType, EntityTransformFunc transformFunc);
void EntityList_Clear(EntityList_t *list);

void EntityList_RebuildBatches(EntityList_t *list);
void EntityList_UpdateInstances(EntityList_t *list, uint32_t frameIndex);

#endif
