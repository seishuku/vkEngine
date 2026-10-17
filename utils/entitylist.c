#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../utils/animation.h"
#include "../assetmanager.h"
#include "../perframe.h"
#include "entitylist.h"

extern VkuContext_t vkContext;
extern AnimationSystem_t animationSystem;

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

	list->culledBatchCapacity=64;
	list->culledBatches=Zone_Malloc(zone, sizeof(EntityBatch_t)*list->culledBatchCapacity);

	if(!list->batches||!list->culledBatches)
		goto fail;

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(!vkuCreateHostBuffer(&vkContext, &list->perFrame[i].instanceBuffer, sizeof(EntityInstance_t)*MAX_ENTITY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
			goto fail;

		list->perFrame[i].instancePtr=list->perFrame[i].instanceBuffer.memory->mappedPointer;

		if(!vkuCreateHostBuffer(&vkContext, &list->perFrame[i].culledInstanceBuffer, sizeof(EntityInstance_t)*MAX_ENTITY, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
			goto fail;

		list->perFrame[i].culledInstancePtr=list->perFrame[i].culledInstanceBuffer.memory->mappedPointer;
	}

	list->dirty=false;

	ID_Init(list->IDPool);

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

		if(list->perFrame[i].culledInstanceBuffer.buffer)
			vkuDestroyBuffer(&vkContext, &list->perFrame[i].culledInstanceBuffer);
	}

	Zone_Free(zone, list->batches);
	Zone_Free(zone, list->culledBatches);
	memset(list, 0, sizeof(*list));
}

uint32_t EntityList_Add(EntityList_t *list, RigidBody_t *body, bool noRender, uint32_t modelID, uint32_t tex0, uint32_t tex1, EntityObjectType_e objectType, EntityTransformFunc transformFunc)
{
	if(list->entityCount>=MAX_ENTITY)
	{
		DBGPRINTF(DEBUG_ERROR, "Ran out of entity space.\n");
		return UINT32_MAX;
	}

	Entity_t entity={
		.ID=ID_Generate(list->IDPool),
		.body=body,
		.objectType=objectType,
		.isAttractor=false,
		.influenceRadius=0.0f,
		.baseGravity=0.0f,
		.noRender=noRender,
		.modelID=modelID,
		.textureIDs[0]=tex0,
		.textureIDs[1]=tex1,
		.animationID=UINT32_MAX,
		.transformFunc=transformFunc,
	};

	if(body->type==RIGIDBODY_OBB)
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
	else if(body->type==RIGIDBODY_SPHERE)
	{
		entity.bounds.min=Vec3(body->position.x-body->radius, body->position.y-body->radius, body->position.z-body->radius);
		entity.bounds.max=Vec3(body->position.x+body->radius, body->position.y+body->radius, body->position.z+body->radius);
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

	return entity.ID;
}

bool EntityList_Remove(EntityList_t *list, uint32_t ID)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		Entity_t *entity=&list->entities[i];

		if(entity->ID!=ID)
			continue;

		// Flag entity to be removed and signal for a rebuild
		entity->remove=true;
		list->dirty=true;

		return true;
	}

	DBGPRINTF(DEBUG_ERROR, "Entity not found.\n");
	return false;
}

void EntityList_Clear(EntityList_t *list)
{
	list->entityCount=0;
	memset(list->entities, 0, sizeof(Entity_t)*MAX_ENTITY);

	ID_Init(list->IDPool);
}

bool EntityList_SetNoRender(EntityList_t *list, uint32_t ID, bool noRender)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		Entity_t *entity=&list->entities[i];

		if(entity->ID!=ID)
			continue;

		entity->noRender=noRender;
		list->dirty=true;

		return true;
	}

	DBGPRINTF(DEBUG_ERROR, "Entity not found.\n");
	return false;
}

uint32_t EntityList_AddAnimator(EntityList_t *list, uint32_t ID)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		Entity_t *entity=&list->entities[i];

		if(entity->ID!=ID)
			continue;

		// Already has one, nothing to do
		if(entity->animationID!=UINT32_MAX)
			return entity->animationID;

		uint32_t animationID=Animation_Create(&animationSystem, &AssetManager_GetAsset(assets, entity->modelID)->model, NULL);

		if(animationID==UINT32_MAX)
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to create animator for entity %u.\n", ID);
			return UINT32_MAX;
		}

		entity->animationID=animationID;

		return animationID;
	}

	DBGPRINTF(DEBUG_ERROR, "Entity not found.\n");
	return UINT32_MAX;
}

bool EntityList_RemoveAnimator(EntityList_t *list, uint32_t ID)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		Entity_t *entity=&list->entities[i];

		if(entity->ID!=ID)
			continue;

		if(entity->animationID!=UINT32_MAX)
		{
			Animation_Free(&animationSystem, entity->animationID);
			entity->animationID=UINT32_MAX;
		}

		return true;
	}

	DBGPRINTF(DEBUG_ERROR, "Entity not found.\n");
	return false;
}

Animator_t *EntityList_GetAnimator(EntityList_t *list, uint32_t ID)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		const Entity_t *entity=&list->entities[i];

		if(entity->ID!=ID)
			continue;

		if(entity->animationID==UINT32_MAX)
			return NULL;

		return Animation_Get(&animationSystem, entity->animationID);
	}

	return NULL;
}

void EntityList_RecalculateBounds(EntityList_t *list)
{
	for(uint32_t i=0;i<list->entityCount;i++)
	{
		Entity_t *entity=&list->entities[i];
		RigidBody_t *body=entity->body;
	
		if(body->type==RIGIDBODY_OBB)
		{
			vec3 axis[3];
			QuatAxes(body->orientation, axis);

			vec3 extents={
				fabsf(axis[0].x)*body->size.x+fabsf(axis[1].x)*body->size.y+fabsf(axis[2].x)*body->size.z,
				fabsf(axis[0].y)*body->size.x+fabsf(axis[1].y)*body->size.y+fabsf(axis[2].y)*body->size.z,
				fabsf(axis[0].z)*body->size.x+fabsf(axis[1].z)*body->size.y+fabsf(axis[2].z)*body->size.z
			};

			entity->bounds.min=Vec3_Subv(body->position, extents);
			entity->bounds.max=Vec3_Addv(body->position, extents);
		}
		else if(body->type==RIGIDBODY_SPHERE)
		{
			entity->bounds.min=Vec3(body->position.x-body->radius, body->position.y-body->radius, body->position.z-body->radius);
			entity->bounds.max=Vec3(body->position.x+body->radius, body->position.y+body->radius, body->position.z+body->radius);
		}
		else if(body->type==RIGIDBODY_CAPSULE)
		{
			vec3 axis[3];
			QuatAxes(body->orientation, axis);

			vec3 offset=Vec3_Muls(axis[1], body->size.y);

			vec3 a=Vec3_Subv(body->position, offset);
			vec3 b=Vec3_Addv(body->position, offset);

			entity->bounds.min=Vec3(
				fminf(a.x, b.x)-body->radius,
				fminf(a.y, b.y)-body->radius,
				fminf(a.z, b.z)-body->radius
			);
			entity->bounds.max=Vec3(
				fmaxf(a.x, b.x)+body->radius,
				fmaxf(a.y, b.y)+body->radius,
				fmaxf(a.z, b.z)+body->radius
			);
		}
	}
}

void EntityList_Rebuild(EntityList_t *list)
{
	if(!list->dirty)
		return;

	uint32_t i=0;
	while(i<list->entityCount)
	{
		Entity_t *entity=&list->entities[i];

		if(entity->remove)
		{
			ID_Remove(list->IDPool, entity->ID);

			list->entities[i]=list->entities[--list->entityCount];
			memset(&list->entities[list->entityCount], 0, sizeof(Entity_t));
		}
		else
			i++;
	}

	list->sortedCount=0;

	for(uint32_t i=0;i<list->entityCount;i++)
		list->sortedIndices[list->sortedCount++]=i;

	sortList=list;
	qsort(list->sortedIndices, list->sortedCount, sizeof(uint32_t), EntitySortCompare);

	list->batchCount=0;

	for(uint32_t i=0;i<list->sortedCount;i++)
	{
		const Entity_t *entity=&list->entities[list->sortedIndices[i]];

		if(
			(list->batchCount==0)||
			(entity->modelID!=list->batches[list->batchCount-1].modelID)||
			(entity->textureIDs[0]!=list->batches[list->batchCount-1].textureIDs[0])||
			(entity->textureIDs[1]!=list->batches[list->batchCount-1].textureIDs[1])||
			(entity->noRender!=list->batches[list->batchCount-1].noRender))
		{
			if(list->batchCount==list->batchCapacity)
			{
				list->batchCapacity*=2;
				list->batches=Zone_Realloc(zone, list->batches, sizeof(EntityBatch_t)*list->batchCapacity);
			}

			EntityBatch_t *b=&list->batches[list->batchCount++];
			b->noRender=entity->noRender;
			b->modelID=entity->modelID;
			b->textureIDs[0]=entity->textureIDs[0];
			b->textureIDs[1]=entity->textureIDs[1];
			b->animationID=entity->animationID;
			b->instanceOffset=i;
			b->instanceCount=0;
		}

		list->batches[list->batchCount-1].instanceCount++;
	}

	list->dirty=false;
}

void EntityList_FrustumCull(EntityList_t *list, const frustum frustum)
{
	list->culledCount=0;
	list->culledBatchCount=0;

	for(uint32_t b=0;b<list->batchCount;b++)
	{
		const EntityBatch_t *src=&list->batches[b];
		uint32_t srcEnd=src->instanceOffset+src->instanceCount;
		uint32_t batchStart=list->culledCount;

		for(uint32_t i=src->instanceOffset;i<srcEnd;i++)
		{
			const Entity_t *entity=&list->entities[list->sortedIndices[i]];

			if(Frustum_TestAABB(frustum, entity->bounds))
				list->culledIndices[list->culledCount++]=list->sortedIndices[i];
		}

		uint32_t remaining=list->culledCount-batchStart;

		if(remaining>0)
		{
			if(list->culledBatchCount==list->culledBatchCapacity)
			{
				list->culledBatchCapacity*=2;
				list->culledBatches=Zone_Realloc(zone, list->culledBatches, sizeof(EntityBatch_t)*list->culledBatchCapacity);
			}

			EntityBatch_t *dst=&list->culledBatches[list->culledBatchCount++];
			dst->noRender=src->noRender;
			dst->modelID=src->modelID;
			dst->textureIDs[0]=src->textureIDs[0];
			dst->textureIDs[1]=src->textureIDs[1];
			dst->animationID=src->animationID;
			dst->instanceOffset=batchStart;
			dst->instanceCount=remaining;
		}
	}
}

void EntityList_UpdateInstances(EntityList_t *list, uint32_t frameIndex)
{
	// Update all entities
	EntityInstance_t *dst=list->perFrame[frameIndex].instancePtr;

	for(uint32_t i=0;i<list->sortedCount;i++)
	{
		const Entity_t *entity=&list->entities[list->sortedIndices[i]];

		if(entity->transformFunc)
			dst[i].transform=entity->transformFunc(entity->body);

		dst[i].paletteOffset=UINT32_MAX;

		if(entity->animationID!=UINT32_MAX)
			dst[i].paletteOffset=animationSystem.animators[entity->animationID].paletteOffset;
		else
			dst[i].paletteOffset=UINT32_MAX;
	}

	// Update culled entities
	EntityInstance_t *culledDst=list->perFrame[frameIndex].culledInstancePtr;

	for(uint32_t i=0;i<list->culledCount;i++)
	{
		const Entity_t *entity=&list->entities[list->culledIndices[i]];

		if(entity->transformFunc)
			culledDst[i].transform=entity->transformFunc(entity->body);

		if(entity->animationID!=UINT32_MAX)
			culledDst[i].paletteOffset=animationSystem.animators[entity->animationID].paletteOffset;
		else
			culledDst[i].paletteOffset=UINT32_MAX;
	}
}
