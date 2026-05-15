#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "perframe.h"
#include "entitylist.h"

extern VkuContext_t vkContext;

static EntityList_t *sortList;

static int EntitySortCompare(const void *a, const void *b)
{
	const Entity_t *ea=&sortList->entities[*(const uint32_t *)a];
	const Entity_t *eb=&sortList->entities[*(const uint32_t *)b];

	if(ea->modelID!=eb->modelID)
		return (int)ea->modelID-(int)eb->modelID;
	if(ea->textureIDs[0]!=eb->textureIDs[0])
		return (int)ea->textureIDs[0]-(int)eb->textureIDs[0];
	return (int)ea->textureIDs[1]-(int)eb->textureIDs[1];
}

bool EntityList_Init(EntityList_t *list)
{
	memset(list, 0, sizeof(*list));

	list->batchCapacity=64;
	list->batches=Zone_Malloc(zone, sizeof(EntityBatch_t)*list->batchCapacity);

	if(!list->batches)
		goto fail;

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(!vkuCreateHostBuffer(&vkContext, &list->perFrame[i].instanceBuffer, sizeof(matrix)*MAX_ENTITY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
			goto fail;

		list->perFrame[i].instancePtr=list->perFrame[i].instanceBuffer.memory->mappedPointer;
	}

	list->dirty=false;

	return true;

fail:
	EntityList_Destroy(list);
	return false;
}

void EntityList_Destroy(EntityList_t *list)
{
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(list->perFrame[i].instanceBuffer.buffer)
			vkuDestroyBuffer(&vkContext, &list->perFrame[i].instanceBuffer);
	}


	Zone_Free(zone, list->batches);
	memset(list, 0, sizeof(*list));
}

bool EntityList_Add(EntityList_t *list, RigidBody_t *body, uint32_t modelID, uint32_t tex0, uint32_t tex1, EntityObjectType_e objectType, EntityTransformFunc transformFunc)
{
	if(list->entityCount>=MAX_ENTITY)
	{
		DBGPRINTF(DEBUG_ERROR, "Ran out of entity space.\n");
		return false;
	}

	Entity_t entity={
		.body=body,
		.objectType=objectType,
		.modelID=modelID,
		.textureIDs[0]=tex0,
		.textureIDs[1]=tex1,
		.transformFunc=transformFunc,
	};

    if(body->type==RIGIDBODY_SPHERE)
    {
        entity.bounds.min=Vec3(body->position.x-body->radius, body->position.y-body->radius, body->position.z-body->radius);
        entity.bounds.max=Vec3(body->position.x+body->radius, body->position.y+body->radius, body->position.z+body->radius);
    }
    else if(body->type==RIGIDBODY_OBB)
    {
        vec3 axis[3];
		QuatAxes(body->orientation, axis);

        vec3 extents={
            fabsf(axis[0].x)*body->size.x+fabsf(axis[1].x)*body->size.y+fabsf(axis[2].x)*body->size.z,
            fabsf(axis[0].y)*body->size.x+fabsf(axis[1].y)*body->size.y+fabsf(axis[2].y)*body->size.z,
            fabsf(axis[0].z)*body->size.x+fabsf(axis[1].z)*body->size.y+fabsf(axis[2].z)*body->size.z
        };

        entity.bounds.min=Vec3_Subv(body->position, extents);
        entity.bounds.max=Vec3_Addv(body->position, extents);
    }
	else if(body->type==RIGIDBODY_CAPSULE)
	{
        vec3 axis[3];
		QuatAxes(body->orientation, axis);

		vec3 offset=Vec3_Muls(axis[1], body->size.y);

		vec3 a=Vec3_Subv(body->position, offset);
		vec3 b=Vec3_Addv(body->position, offset);

		entity.bounds.min=Vec3(
			fminf(a.x, b.x)-body->radius,
			fminf(a.y, b.y)-body->radius,
			fminf(a.z, b.z)-body->radius
		);
		entity.bounds.max=Vec3(
			fmaxf(a.x, b.x)+body->radius,
			fmaxf(a.y, b.y)+body->radius,
			fmaxf(a.z, b.z)+body->radius
		);
	}

	list->entities[list->entityCount++]=entity;

	list->dirty=true;

	return true;
}

void EntityList_Clear(EntityList_t *list)
{
	list->entityCount=0;
	memset(list->entities, 0, sizeof(Entity_t)*MAX_ENTITY);
}

void EntityList_RebuildBatches(EntityList_t *list)
{
	if(!list->dirty)
		return;

	list->sortedCount=0;

	for(uint32_t i=0;i<list->entityCount;i++)
		list->sortedIndices[list->sortedCount++]=i;

	sortList=list;
	qsort(list->sortedIndices, list->sortedCount, sizeof(uint32_t), EntitySortCompare);

	list->batchCount=0;

	for(uint32_t i=0;i<list->sortedCount;i++)
	{
		const Entity_t *entity=&list->entities[list->sortedIndices[i]];

		if((list->batchCount==0)||(entity->modelID!=list->batches[list->batchCount-1].modelID)||(entity->textureIDs[0]!=list->batches[list->batchCount-1].textureIDs[0])||(entity->textureIDs[1]!=list->batches[list->batchCount-1].textureIDs[1]))
		{
			if(list->batchCount==list->batchCapacity)
			{
				list->batchCapacity*=2;
				list->batches=Zone_Realloc(zone, list->batches, sizeof(EntityBatch_t)*list->batchCapacity);
			}

			EntityBatch_t *b=&list->batches[list->batchCount++];
			b->modelID=entity->modelID;
			b->textureIDs[0]=entity->textureIDs[0];
			b->textureIDs[1]=entity->textureIDs[1];
			b->instanceOffset=i;
			b->instanceCount=0;
		}

		list->batches[list->batchCount-1].instanceCount++;
	}

	DBGPRINTF(DEBUG_INFO, "Batch: %d\n", list->batchCount);
	for(uint32_t i=0;i<list->batchCount;i++)
		DBGPRINTF(DEBUG_INFO, "Batch instance count: %d offset: %d\n", list->batches[i].instanceCount, list->batches[i].instanceOffset);

	list->dirty=false;
}

void EntityList_UpdateInstances(EntityList_t *list, uint32_t frameIndex)
{
	matrix *dst=list->perFrame[frameIndex].instancePtr;

	for(uint32_t i=0;i<list->sortedCount;i++)
	{
		const Entity_t *entity=&list->entities[list->sortedIndices[i]];

		if(entity->transformFunc)
			dst[i]=entity->transformFunc(entity->body);
	}
}
