#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../physics/physicslist.h"

typedef struct
{
	PhysicsObject_t *object;
	float value;
	bool isMin;
} Endpoint_t;

Endpoint_t endpointsX[MAX_PHYSICSOBJECTS*2];
Endpoint_t endpointsY[MAX_PHYSICSOBJECTS*2];
Endpoint_t endpointsZ[MAX_PHYSICSOBJECTS*2];

#define BIT_MATRIX_SIZE ((MAX_PHYSICSOBJECTS*MAX_PHYSICSOBJECTS+7)/8)

uint8_t overlapX[BIT_MATRIX_SIZE], overlapY[BIT_MATRIX_SIZE], overlapZ[BIT_MATRIX_SIZE];

static void setBit(uint8_t *bitMatrix, int i, int j)
{
	int index=i*MAX_PHYSICSOBJECTS+j;
	int byteIndex=index/8;
	int bitIndex=index%8;

	bitMatrix[byteIndex]|=(1<<bitIndex);
}

int compareEndpoint(const void *a, const void *b)
{
	const Endpoint_t *epA=(const Endpoint_t *)a;
	const Endpoint_t *epB=(const Endpoint_t *)b;

	if(epA->value<epB->value)
		return -1;

	if(epA->value>epB->value)
		return 1;

	if(epA->isMin&&!epB->isMin)
		return -1;

	if(!epA->isMin&&epB->isMin)
		return 1;

	int idxA=epA->object-physicsObjects;
	int idxB=epB->object-physicsObjects;

	return idxA-idxB;
}

void SweepAxis(Endpoint_t *endpoints, uint8_t *bitMatrix, int numObjects)
{
	PhysicsObject_t *activeSet[MAX_PHYSICSOBJECTS];
	int activeSetSize=0;

	for(int i=0;i<numObjects*2;i++)
	{
		Endpoint_t *ep=&endpoints[i];

		if(ep->isMin)
		{
			for(int j=0;j<activeSetSize;j++)
			{
				PhysicsObject_t *objA=ep->object;
				PhysicsObject_t *objB=activeSet[j];
				int idxA=objA-physicsObjects;
				int idxB=objB-physicsObjects;

				setBit(bitMatrix, idxA, idxB);
				setBit(bitMatrix, idxB, idxA);
			}

			activeSet[activeSetSize++]=ep->object;
		}
		else
		{
			for(int j=0;j<activeSetSize;j++)
			{
				if(activeSet[j]==ep->object)
				{
					activeSet[j]=activeSet[activeSetSize-1];
					activeSetSize--;
					break;
				}
			}
		}
	}
}

void TestCollision(void *a, void *b);

void SweepAndPrune3D()
{
	for(int i=0;i<numPhysicsObjects;i++)
	{
		PhysicsObject_t *obj=&physicsObjects[i];

		endpointsX[2*i+0].value=obj->min.x;
		endpointsX[2*i+0].object=obj;
		endpointsX[2*i+0].isMin=true;
		endpointsX[2*i+1].value=obj->max.x;
		endpointsX[2*i+1].object=obj;
		endpointsX[2*i+1].isMin=false;

		endpointsY[2*i+0].value=obj->min.y;
		endpointsY[2*i+0].object=obj;
		endpointsY[2*i+0].isMin=true;
		endpointsY[2*i+1].value=obj->max.y;
		endpointsY[2*i+1].object=obj;
		endpointsY[2*i+1].isMin=false;

		endpointsZ[2*i+0].value=obj->min.z;
		endpointsZ[2*i+0].object=obj;
		endpointsZ[2*i+0].isMin=true;
		endpointsZ[2*i+1].value=obj->max.z;
		endpointsZ[2*i+1].object=obj;
		endpointsZ[2*i+1].isMin=false;
	}

	qsort(endpointsX, numPhysicsObjects*2, sizeof(Endpoint_t), compareEndpoint);
	qsort(endpointsY, numPhysicsObjects*2, sizeof(Endpoint_t), compareEndpoint);
	qsort(endpointsZ, numPhysicsObjects*2, sizeof(Endpoint_t), compareEndpoint);

	memset(overlapX, 0, BIT_MATRIX_SIZE);
	memset(overlapY, 0, BIT_MATRIX_SIZE);
	memset(overlapZ, 0, BIT_MATRIX_SIZE);

	SweepAxis(endpointsX, overlapX, numPhysicsObjects);
	SweepAxis(endpointsY, overlapY, numPhysicsObjects);
	SweepAxis(endpointsZ, overlapZ, numPhysicsObjects);

	int numWords=(MAX_PHYSICSOBJECTS*MAX_PHYSICSOBJECTS+63)/64;
	uint64_t *xWords=(uint64_t *)overlapX;
	uint64_t *yWords=(uint64_t *)overlapY;
	uint64_t *zWords=(uint64_t *)overlapZ;

	for(int w=0;w<numWords;w++)
	{
		uint64_t word=xWords[w]&yWords[w]&zWords[w];
		int bitBase=w*64, bitOffset=0;

		while(word)
		{
			while((word&1)==0)
			{
				word>>=1;
				bitOffset++;
			}

			int bitIndex=bitOffset+bitBase;

			int i=bitIndex/MAX_PHYSICSOBJECTS;
			int j=bitIndex%MAX_PHYSICSOBJECTS;

			if(i<j)
				TestCollision(&physicsObjects[i], &physicsObjects[j]);

			word>>=1;
			bitOffset++;
		}
	}
}
