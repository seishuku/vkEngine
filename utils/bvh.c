#include "bvh.h"
#include "../system/system.h"
#include "../math/math.h"
#include <float.h>
#include <stdbool.h>
#include <stdint.h>

void BVH_Build(BVH_t *bvh, const void *AABBs, const uint32_t numAABBs, const uint32_t stride, const uint32_t offset)
{
	typedef struct
	{
		int32_t nodeIndex;
		uint32_t first;
		uint32_t count;
	} BVHBuildStackObj_t;

	BVHBuildStackObj_t stack[BVH_MAX_NODES];
	uint32_t stackTop=0;
	uint32_t numIndices=numAABBs;

	if(numIndices>=BVH_MAX_OBJECTS)
	{
		numIndices=BVH_MAX_OBJECTS;
		DBGPRINTF(DEBUG_WARNING, "BVH: numAABBs (%d) > MAX OBJECTS\n", numAABBs);
	}

	// Initialize object index list
	for(uint32_t i=0;i<numIndices;i++)
		bvh->objectIndices[i]=i;

	bvh->nodeCount=1;

	stack[stackTop++]=(BVHBuildStackObj_t)
	{
		.nodeIndex=0,
	    .first=0, .count=numIndices
	};

	while(stackTop>0)
	{
		BVHBuildStackObj_t stackObj=stack[--stackTop];
		BVHNode_t *node=&bvh->nodes[stackObj.nodeIndex];

		node->first=stackObj.first;
		node->count=stackObj.count;
		node->left=-1;
		node->right=-1;

		// Compute bounds
		node->bounds.min=Vec3b(FLT_MAX);
		node->bounds.max=Vec3b(-FLT_MAX);

		for(uint32_t i=0;i<stackObj.count;i++)
		{
			uint32_t index=bvh->objectIndices[stackObj.first+i];
			aabb *bounds=(aabb *)((uint8_t *)AABBs+index*stride+offset);;

			node->bounds.min.x=fminf(node->bounds.min.x, bounds->min.x);
			node->bounds.min.y=fminf(node->bounds.min.y, bounds->min.y);
			node->bounds.min.z=fminf(node->bounds.min.z, bounds->min.z);
			node->bounds.max.x=fmaxf(node->bounds.max.x, bounds->max.x);
			node->bounds.max.y=fmaxf(node->bounds.max.y, bounds->max.y);
			node->bounds.max.z=fmaxf(node->bounds.max.z, bounds->max.z);
		}

		// Leaf
		if(stackObj.count<=BVH_MAX_OBJECTS_PER_LEAF)
			continue;

		// Choose split axis (largest extent)
		vec3 extent=
		{
		    node->bounds.max.x-node->bounds.min.x,
		    node->bounds.max.y-node->bounds.min.y,
		    node->bounds.max.z-node->bounds.min.z
		};

		int axis=(extent.x>=extent.y&&extent.x>=extent.z)?0:(extent.y>=extent.z)?1:2;

		float splitPos;
		if(axis==0)			splitPos=0.5f*(node->bounds.min.x+node->bounds.max.x);
		else if(axis==1)	splitPos=0.5f*(node->bounds.min.y+node->bounds.max.y);
		else				splitPos=0.5f*(node->bounds.min.z+node->bounds.max.z);

		// Partition objects
		uint32_t mid=stackObj.first;
		uint32_t end=stackObj.first+stackObj.count;

		for(uint32_t i=stackObj.first;i<end;i++)
		{
			const uint32_t index=bvh->objectIndices[i];
			aabb *bounds=(aabb *)((uint8_t *)AABBs+index*stride+offset);
			vec3 centroid=(vec3)
			{
				0.5f*(bounds->min.x+bounds->max.x),
				0.5f*(bounds->min.y+bounds->max.y),
				0.5f*(bounds->min.z+bounds->max.z)
			};
			float v=(axis==0)?centroid.x:(axis==1)?centroid.y:centroid.z;

			if(v<splitPos)
			{
				uint32_t tmp=bvh->objectIndices[i];
				bvh->objectIndices[i]=bvh->objectIndices[mid];
				bvh->objectIndices[mid]=tmp;
				mid++;
			}
		}

		// Fallback split
		if(mid==stackObj.first||mid==end)
			mid=(stackObj.first+stackObj.count)/2;

		// Create children
		node->left=bvh->nodeCount++;
		node->right=bvh->nodeCount++;

		stack[stackTop++]=(BVHBuildStackObj_t) { node->right, mid, end-mid};
		stack[stackTop++]=(BVHBuildStackObj_t) { node->left, stackObj.first, mid-stackObj.first };
	}
}

void BVH_Test(const BVH_t *bvh, void *objects, const uint32_t stride, void (*testFunc)(void *a, void *b))
{
	typedef struct
	{
		int32_t a, b;
	} BVHPairStackObj_t;

	BVHPairStackObj_t stack[BVH_MAX_NODES*2];
	uint32_t stackTop=0;

	// Start with root vs root
	stack[stackTop++]=(BVHPairStackObj_t) { 0, 0};

	while(stackTop>0)
	{
		BVHPairStackObj_t stackObj=stack[--stackTop];
		const BVHNode_t *a=&bvh->nodes[stackObj.a];
		const BVHNode_t *b=&bvh->nodes[stackObj.b];

		// Avoid duplicate and symmetric tests
		if(stackObj.a>stackObj.b)
			continue;

		// Test for overlap
		if(!(
			(a->bounds.min.x<=b->bounds.max.x&&a->bounds.max.x>=b->bounds.min.x)&&
	     	(a->bounds.min.y<=b->bounds.max.y&&a->bounds.max.y>=b->bounds.min.y)&&
	     	(a->bounds.min.z<=b->bounds.max.z&&a->bounds.max.z>=b->bounds.min.z))
		)
		 	continue;

		int32_t aLeaf=(a->left==-1);
		int32_t bLeaf=(b->left==-1);

		// Leaf–leaf test
		if(aLeaf&&bLeaf)
		{
			for(uint32_t i=0;i<a->count;i++)
			{
				uint32_t ia=bvh->objectIndices[a->first+i];
				void *objA=objects+ia*stride;

				for(uint32_t j=0;j<b->count;j++)
				{
					uint32_t ib=bvh->objectIndices[b->first+j];
					void *objB=objects+ib*stride;

					/// Avoid self
					if(ia==ib)
						continue;

					testFunc(objA, objB);
				}
			}

			continue;
		}

		// Descend
		if(aLeaf)
		{
			stack[stackTop++]=(BVHPairStackObj_t){stackObj.a, b->left};
			stack[stackTop++]=(BVHPairStackObj_t){stackObj.a, b->right};
		}
		else if(bLeaf)
		{
			stack[stackTop++]=(BVHPairStackObj_t){a->left, stackObj.b};
			stack[stackTop++]=(BVHPairStackObj_t){a->right, stackObj.b};
		}
		else
		{
			stack[stackTop++]=(BVHPairStackObj_t){a->left, b->left};
			stack[stackTop++]=(BVHPairStackObj_t){a->left, b->right};
			stack[stackTop++]=(BVHPairStackObj_t){a->right, b->left};
			stack[stackTop++]=(BVHPairStackObj_t){a->right, b->right};
		}
	}
}
