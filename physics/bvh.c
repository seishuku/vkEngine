#include "bvh.h"
#include "physicslist.h"
#include "../math/math.h"
#include <float.h>
#include <stdbool.h>
#include <stdint.h>

void BVH_Build(BVH_t *bvh)
{
	typedef struct
	{
		int32_t nodeIndex;
		uint32_t first;
		uint32_t count;
	} BVHBuildStackObj_t;

	BVHBuildStackObj_t stack[BVH_MAX_NODES];
	uint32_t stackTop=0;

	// Initialize object index list
	for(uint32_t i=0;i<numPhysicsObjects;i++)
		bvh->objectIndices[i]=i;

	bvh->nodeCount=1;

	stack[stackTop++]=(BVHBuildStackObj_t)
	{
		.nodeIndex=0,
	    .first=0, .count=numPhysicsObjects
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
		node->min=Vec3b(FLT_MAX);
		node->max=Vec3b(-FLT_MAX);

		for(uint32_t i=0;i<stackObj.count;i++)
		{
			PhysicsObject_t *obj=&physicsObjects[bvh->objectIndices[stackObj.first+i]];

			node->min.x=fminf(node->min.x, obj->min.x);
			node->min.y=fminf(node->min.y, obj->min.y);
			node->min.z=fminf(node->min.z, obj->min.z);
			node->max.x=fmaxf(node->max.x, obj->max.x);
			node->max.y=fmaxf(node->max.y, obj->max.y);
			node->max.z=fmaxf(node->max.z, obj->max.z);
		}

		// Leaf
		if(stackObj.count<=BVH_MAX_OBJECTS_PER_LEAF)
			continue;

		// Choose split axis (largest extent)
		vec3 extent=
		{
		    node->max.x-node->min.x,
		    node->max.y-node->min.y,
		    node->max.z-node->min.z
		};

		int axis=(extent.x>=extent.y&&extent.x>=extent.z)?0:(extent.y>=extent.z)?1:2;

		float splitPos;
		if(axis==0)			splitPos=0.5f*(node->min.x+node->max.x);
		else if(axis==1)	splitPos=0.5f*(node->min.y+node->max.y);
		else				splitPos=0.5f*(node->min.z+node->max.z);

		// Partition objects
		uint32_t mid=stackObj.first;
		uint32_t end=stackObj.first+stackObj.count;

		for(uint32_t i=stackObj.first;i<end;i++)
		{
			PhysicsObject_t *obj=&physicsObjects[bvh->objectIndices[i]];
			vec3 centroid=(vec3)
			{
				0.5f*(obj->min.x+obj->max.x),
				0.5f*(obj->min.y+obj->max.y),
				0.5f*(obj->min.z+obj->max.z)
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

void TestCollision(void *a, void *b);

void BVH_Test(const BVH_t *bvh)
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
			(a->min.x<=b->max.x&&a->max.x>=b->min.x)&&
	     	(a->min.y<=b->max.y&&a->max.y>=b->min.y)&&
	     	(a->min.z<=b->max.z&&a->max.z>=b->min.z))
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
				PhysicsObject_t *objA=&physicsObjects[ia];

				for(uint32_t j=0;j<b->count;j++)
				{
					uint32_t ib=bvh->objectIndices[b->first+j];
					PhysicsObject_t *objB=&physicsObjects[ib];

					/// Avoid self
					if(ia==ib)
						continue;

					TestCollision(objA, objB);
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
