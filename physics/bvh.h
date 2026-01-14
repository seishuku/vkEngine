#ifndef __BVH_H__
#define __BVH_H__

#include "physicslist.h"
#include "../math/math.h"

#define BVH_MAX_OBJECTS_PER_LEAF 4
#define BVH_MAX_NODES (MAX_PHYSICSOBJECTS * 2)

typedef struct
{
    vec3 min, max;
    int32_t left, right;
    uint32_t first, count;
} BVHNode_t;

typedef struct
{
	BVHNode_t nodes[BVH_MAX_NODES];
    uint32_t nodeCount;

	uint32_t objectIndices[MAX_PHYSICSOBJECTS];
} BVH_t;

void BVH_Build(BVH_t *bvh);
void BVH_Test(const BVH_t *bvh);

#endif
