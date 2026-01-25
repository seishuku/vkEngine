#ifndef __BVH_H__
#define __BVH_H__

#include "../math/math.h"

#define BVH_MAX_OBJECTS 10000
#define BVH_MAX_OBJECTS_PER_LEAF 4
#define BVH_MAX_NODES (BVH_MAX_OBJECTS*2)

typedef struct
{
    aabb bounds;
    int32_t left, right;
    uint32_t first, count;
} BVHNode_t;

typedef struct
{
	BVHNode_t nodes[BVH_MAX_NODES];
    uint32_t nodeCount;

	uint32_t objectIndices[BVH_MAX_OBJECTS];
} BVH_t;

void BVH_Build(BVH_t *bvh, const void *boundingBoxes, const uint32_t numObjects, const uint32_t stride, const uint32_t offset);
void BVH_Test(const BVH_t *bvh, void *objects, const uint32_t stride, void (*testFunc)(void *a, void *b));

#endif
