#ifndef __BVH_H__
#define __BVH_H__

#include "../math/math.h"
#include "../entitylist.h"

#define BVH_MAX_NODES (MAX_ENTITY*2)
 
typedef struct
{
    aabb    bounds;
    int32_t left, right;
    int32_t objectIndex;
} BVHNode_t;

typedef struct
{
    uint32_t  numNodes;
    BVHNode_t nodes[BVH_MAX_NODES];
} BVH_t;

typedef void (*BVHLeafCallback_t)(Entity_t *a, Entity_t *b);
typedef void (*BVHQueryCallback_t)(Entity_t *entity, void *userdata);

void BVH_Build(BVH_t *bvh, EntityList_t *entityList);
void BVH_Test(BVH_t *bvh, EntityList_t *entityList, BVHLeafCallback_t callback);
void BVH_Query(BVH_t *bvh, EntityList_t *entityList, vec3 point, float radius, BVHQueryCallback_t callback, void *userdata);

#endif
