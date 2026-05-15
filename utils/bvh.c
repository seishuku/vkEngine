#include <stdbool.h>
#include <stdint.h>
#include <float.h>
#include <assert.h>
#include "bvh.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../entitylist.h"

static int32_t indices[MAX_ENTITY];

typedef struct
{
    int32_t nodeIndex, start, count;
} BVHBuildStackObj_t;

#define BUILD_STACK_MAX 64
static BVHBuildStackObj_t buildStack[BUILD_STACK_MAX];

void BVH_Build(BVH_t *bvh, EntityList_t *entityList)
{
	bvh->numNodes=0;

	if(entityList->entityCount==0)
		return;

	for(uint32_t i=0;i<entityList->entityCount;i++)
		indices[i]=(int32_t)i;

	bvh->numNodes=1;

	int32_t stackTop=0;
	BVHBuildStackObj_t initialWork={ 0, 0, (int32_t)entityList->entityCount };
	buildStack[stackTop++]=initialWork;

	while(stackTop>0)
	{
		BVHBuildStackObj_t work=buildStack[--stackTop];
		BVHNode_t *node=&bvh->nodes[work.nodeIndex];
		aabb nodeBounds=entityList->entities[indices[work.start]].bounds;

		for(int32_t i=1;i<work.count;i++)
		{
			const aabb *b=&entityList->entities[indices[work.start+i]].bounds;
			nodeBounds=(aabb) {
				nodeBounds.min.x<b->min.x?nodeBounds.min.x:b->min.x,
				nodeBounds.min.y<b->min.y?nodeBounds.min.y:b->min.y,
				nodeBounds.min.z<b->min.z?nodeBounds.min.z:b->min.z,
				nodeBounds.max.x>b->max.x?nodeBounds.max.x:b->max.x,
				nodeBounds.max.y>b->max.y?nodeBounds.max.y:b->max.y,
				nodeBounds.max.z>b->max.z?nodeBounds.max.z:b->max.z,
			};
		}

		node->bounds=nodeBounds;

		if(work.count==1)
		{
			node->left=-1;
			node->right=-1;
			node->objectIndex=indices[work.start];
			continue;
		}

		vec3 extent=Vec3_Subv(nodeBounds.max, nodeBounds.min);
		uint32_t axis=(extent.x>=extent.y&&extent.x>=extent.z)?0:(extent.y>=extent.z)?1:2;

		float centroidMin, centroidMax;
		centroidMin=centroidMax=0.5f*(entityList->entities[indices[work.start]].bounds.min.v[axis]+entityList->entities[indices[work.start]].bounds.max.v[axis]);

		for(int32_t i=1;i<work.count;i++)
		{
			float c=0.5f*(entityList->entities[indices[work.start+i]].bounds.min.v[axis]+entityList->entities[indices[work.start+i]].bounds.max.v[axis]);

			if(c<centroidMin)
				centroidMin=c;

			if(c>centroidMax)
				centroidMax=c;
		}

		float splitPos=0.5f*(centroidMin+centroidMax);

		int32_t lo=work.start;
		int32_t hi=work.start+work.count-1;

		while(lo<=hi)
		{
			float c=0.5f*(entityList->entities[indices[lo]].bounds.min.v[axis]+entityList->entities[indices[lo]].bounds.max.v[axis]);;

			if(c<splitPos)
				lo++;
			else
			{
				int32_t tmp=indices[lo];
				indices[lo]=indices[hi];
				indices[hi]=tmp;
				hi--;
			}
		}

		int32_t leftCount=lo-work.start;
		int32_t rightCount=work.count-leftCount;

		if(leftCount==0||rightCount==0)
		{
			leftCount=work.count/2;
			rightCount=work.count-leftCount;
		}

		assert((uint32_t)(bvh->numNodes+2)<=BVH_MAX_NODES&&"BVH node pool exhausted");

		int32_t leftIndex=(int32_t)bvh->numNodes++;
		int32_t rightIndex=(int32_t)bvh->numNodes++;

		bvh->nodes[work.nodeIndex].left=leftIndex;
		bvh->nodes[work.nodeIndex].right=rightIndex;
		bvh->nodes[work.nodeIndex].objectIndex=-1;

		assert(stackTop+2<=BUILD_STACK_MAX&&"BVH build stack overflow");

		buildStack[stackTop++]=(BVHBuildStackObj_t) { leftIndex, work.start, leftCount };
		buildStack[stackTop++]=(BVHBuildStackObj_t) { rightIndex, work.start+leftCount, rightCount };
	}
}

typedef struct
{
	int32_t a;
	int32_t b;
} BVHPairStackObj_t;

#define TEST_STACK_MAX (MAX_ENTITY*4)
static BVHPairStackObj_t testStack[TEST_STACK_MAX];

void BVH_Test(BVH_t *bvh, EntityList_t *entityList, BVHLeafCallback_t callback)
{
	if(bvh->numNodes==0||entityList->entityCount==0)
		return;

	int32_t stackTop=0;

	testStack[stackTop++]=(BVHPairStackObj_t) { 0, 0 };

	while(stackTop>0)
	{
		BVHPairStackObj_t pair=testStack[--stackTop];

		const BVHNode_t *a=&bvh->nodes[pair.a];
		const BVHNode_t *b=&bvh->nodes[pair.b];

		if(!((a->bounds.min.x<=b->bounds.max.x)&
			 (a->bounds.max.x>=b->bounds.min.x)&
			 (a->bounds.min.y<=b->bounds.max.y)&
			 (a->bounds.max.y>=b->bounds.min.y)&
			 (a->bounds.min.z<=b->bounds.max.z)&
			 (a->bounds.max.z>=b->bounds.min.z)))
			continue;

		int leafA=(a->left==-1);
		int leafB=(b->left==-1);

		if(leafA&&leafB)
		{
			if(a->objectIndex!=b->objectIndex)
				callback(&entityList->entities[a->objectIndex], &entityList->entities[b->objectIndex]);
		}
		else if(pair.a==pair.b)
		{
			int32_t l=a->left;
			int32_t r=a->right;

			assert(stackTop+3<=TEST_STACK_MAX&&"BVH test stack overflow");

			testStack[stackTop++]=(BVHPairStackObj_t) { l, l };
			testStack[stackTop++]=(BVHPairStackObj_t) { r, r };
			testStack[stackTop++]=(BVHPairStackObj_t) { l, r };
		}
		else if(leafA)
		{
			assert(stackTop+2<=TEST_STACK_MAX&&"BVH test stack overflow");

			testStack[stackTop++]=(BVHPairStackObj_t) { pair.a, b->left };
			testStack[stackTop++]=(BVHPairStackObj_t) { pair.a, b->right };
		}
		else if(leafB)
		{
			assert(stackTop+2<=TEST_STACK_MAX&&"BVH test stack overflow");

			testStack[stackTop++]=(BVHPairStackObj_t) { a->left, pair.b };
			testStack[stackTop++]=(BVHPairStackObj_t) { a->right, pair.b };
		}
		else
		{
			assert(stackTop+2<=TEST_STACK_MAX&&"BVH test stack overflow – increase TEST_STACK_MAX");

			if((2.0f*Vec3_LengthSq(Vec3_Subv(a->bounds.max, a->bounds.min)))>=(2.0f*Vec3_LengthSq(Vec3_Subv(b->bounds.max, b->bounds.min))))
			{
				testStack[stackTop++]=(BVHPairStackObj_t) { a->left, pair.b };
				testStack[stackTop++]=(BVHPairStackObj_t) { a->right, pair.b };
			}
			else
			{
				testStack[stackTop++]=(BVHPairStackObj_t) { pair.a, b->left };
				testStack[stackTop++]=(BVHPairStackObj_t) { pair.a, b->right };
			}
		}
	}
}
