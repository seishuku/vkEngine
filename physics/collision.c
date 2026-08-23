#include "../math/math.h"
#include "physics.h"
#include <float.h>

typedef enum
{
	SHAPE_SPHERE=0,
	SHAPE_CAPSULE,
	SHAPE_OBB
} ShapeType_e;

typedef struct
{
	ShapeType_e type;
	vec3 center;
	vec3 axes[3];
	float halfLength;
	vec3 halfExtents;
	float radius;
} Shape_t;

static Shape_t MakeShape(const RigidBody_t *body)
{
	switch(body->type)
	{
		case RIGIDBODY_SPHERE:
		{
			return (Shape_t)
			{
				.type=SHAPE_SPHERE,
				.center=body->position,
				.radius=body->radius
			};
		}

		case RIGIDBODY_CAPSULE:
		{
			vec3 axes[3];
			QuatAxes(body->orientation, axes);

			return (Shape_t)
			{
				.type=SHAPE_CAPSULE,
				.center=body->position,
				.axes={ axes[0], axes[1], axes[2] },
				.halfLength=body->radiusHeight.y,
				.radius=body->radiusHeight.x
			};
		}

		case RIGIDBODY_OBB:
		{
			vec3 axes[3];
			QuatAxes(body->orientation, axes);

			return (Shape_t)
			{
				.type=SHAPE_OBB,
				.center=body->position,
				.axes={ axes[0], axes[1], axes[2] },
				.halfExtents=body->size
			};
		}

		default:
			return (Shape_t) { 0 };
	}
}

// Find closest point on OBB
static void ClosestPoint_PointBox(const vec3 point, const Shape_t *box, vec3 *outOnBox)
{
	const vec3 rel=Vec3_Subv(point, box->center);
	const vec3 local=Vec3(Vec3_Dot(rel, box->axes[0]), Vec3_Dot(rel, box->axes[1]), Vec3_Dot(rel, box->axes[2]));
	const vec3 clamped=Vec3(
	    clampf(local.x, -box->halfExtents.x, box->halfExtents.x),
	    clampf(local.y, -box->halfExtents.y, box->halfExtents.y),
	    clampf(local.z, -box->halfExtents.z, box->halfExtents.z)
	);

	*outOnBox=Vec3_Addv(
		box->center,
		Vec3_Addv(
			Vec3_Addv(
				Vec3_Muls(box->axes[0], clamped.x),
				Vec3_Muls(box->axes[1], clamped.y)),
				Vec3_Muls(box->axes[2], clamped.z)
		)
	);
}

// Find closest point on capsule segment
static void ClosestPoint_PointSegment(const vec3 point, const Shape_t *seg, vec3 *outOnSeg)
{
	const vec3 offset=Vec3_Muls(seg->axes[1], seg->halfLength);
	const vec3 a=Vec3_Subv(seg->center, offset);
	const vec3 b=Vec3_Addv(seg->center, offset);

	const vec3 slope=Vec3_Subv(b, a);
	const float slopeLenSq=Vec3_LengthSq(slope);

	float t=0.0f;
	if(slopeLenSq>FLT_EPSILON)
		t=clampf(Vec3_Dot(Vec3_Subv(point, a), slope)/slopeLenSq, 0.0f, 1.0f);

	*outOnSeg=Vec3_Addv(a, Vec3_Muls(slope, t));
}

// Find closest points between two capsule segments
static void ClosestPoint_SegmentSegment(const Shape_t *segA, const Shape_t *segB, vec3 *outA, vec3 *outB)
{
	const vec3 offsetA=Vec3_Muls(segA->axes[1], segA->halfLength);
	const vec3 a0=Vec3_Subv(segA->center, offsetA);
	const vec3 a1=Vec3_Addv(segA->center, offsetA);

	const vec3 offsetB=Vec3_Muls(segB->axes[1], segB->halfLength);
	const vec3 b0=Vec3_Subv(segB->center, offsetB);
	const vec3 b1=Vec3_Addv(segB->center, offsetB);

	// Closest points between segments
	const vec3 d1=Vec3_Subv(a1, a0);
	const vec3 d2=Vec3_Subv(b1, b0);
	const vec3 r=Vec3_Subv(a0, b0);

	const float aDot=Vec3_Dot(d1, d1);
	const float eDot=Vec3_Dot(d2, d2);
	const float fDot=Vec3_Dot(d2, r);

	float s, t;
	if(aDot<=FLT_EPSILON&&eDot<=FLT_EPSILON)
	{
		s=0.0f;
		t=0.0f;
	}
	else if(aDot <= FLT_EPSILON)
	{
		s=0.0f;
		t=clampf(fDot/eDot, 0.0f, 1.0f);
	}
	else
	{
		const float cDot=Vec3_Dot(d1, r);

		if(eDot<=FLT_EPSILON)
		{
			t=0.0f;
			s=clampf(-cDot/aDot, 0.0f, 1.0f);
		}
		else
		{
			const float bDot=Vec3_Dot(d1, d2);
			const float denom=aDot*eDot-bDot*bDot;

			if(fabsf(denom)>FLT_EPSILON)
				s=clampf((bDot*fDot-cDot*eDot)/denom, 0.0f, 1.0f);
			else
				s=0.0f;

			t=(bDot*s+fDot)/eDot;
			t=clampf(t, 0.0f, 1.0f);
		}
	}

	*outA=Vec3_Addv(a0, Vec3_Muls(d1, s));
	*outB=Vec3_Addv(b0, Vec3_Muls(d2, t));
}

// Find closest points between capsule segment and OBB
static void ClosestPoint_SegmentBox(const Shape_t *seg, const Shape_t *box, vec3 *outOnSeg, vec3 *outOnBox)
{
	const vec3 offset=Vec3_Muls(seg->axes[1], seg->halfLength);
	const vec3 capA=Vec3_Subv(seg->center, offset);
	const vec3 capB=Vec3_Addv(seg->center, offset);

	// Transform capsule endpoints into OBB local space
	const vec3 relA=Vec3_Subv(capA, box->center);
	const vec3 aLocal=Vec3(
		Vec3_Dot(relA, box->axes[0]),
		Vec3_Dot(relA, box->axes[1]),
		Vec3_Dot(relA, box->axes[2])
	);

	const vec3 relB=Vec3_Subv(capB, box->center);
	const vec3 bLocal=Vec3(
		Vec3_Dot(relB, box->axes[0]),
		Vec3_Dot(relB, box->axes[1]),
		Vec3_Dot(relB, box->axes[2])
	);

	const vec3 dLocal=Vec3_Subv(bLocal, aLocal);

	// Perform ternary search along capsule segment in OBB local space
	float lo=0.0f, hi=1.0f;
	float bestT=0.0f;
	float bestDistSq=FLT_MAX;

	for(int i=0;i<20;i++)
	{
		const float t1=lo+(hi-lo)*(1.0f/3.0f);
		const float t2=hi-(hi-lo)*(1.0f/3.0f);

		// Closest point on OBB to capsule point at t1 and t2
		const vec3 p1=Vec3_Addv(aLocal, Vec3_Muls(dLocal, t1));
		const vec3 c1=Vec3(
			clampf(p1.x, -box->halfExtents.x, box->halfExtents.x),
			clampf(p1.y, -box->halfExtents.y, box->halfExtents.y),
			clampf(p1.z, -box->halfExtents.z, box->halfExtents.z)
		);
		const float d1=Vec3_LengthSq(Vec3_Subv(p1, c1));

		const vec3 p2=Vec3_Addv(aLocal, Vec3_Muls(dLocal, t2));
		const vec3 c2=Vec3(
			clampf(p2.x, -box->halfExtents.x, box->halfExtents.x),
			clampf(p2.y, -box->halfExtents.y, box->halfExtents.y),
			clampf(p2.z, -box->halfExtents.z, box->halfExtents.z)
		);
		const float d2=Vec3_LengthSq(Vec3_Subv(p2, c2));

		if(d1<d2)
		{
			hi=t2;

			if(d1<bestDistSq)
			{
				bestDistSq=d1;
				bestT=t1;
			}
		}
		else
		{
			lo=t1;

			if(d2<bestDistSq)
			{
				bestDistSq=d2;
				bestT=t2;
			}
		}
	}

	// Final closest points
	*outOnSeg=Vec3_Addv(capA, Vec3_Muls(Vec3_Subv(capB, capA), bestT));
	ClosestPoint_PointBox(*outOnSeg, box, outOnBox);
}

// General shape-to-shape collision detection, single manifold contact point
static CollisionManifold_t ShapeToShapeCollision(RigidBody_t *a, RigidBody_t *b)
{
	const Shape_t shapeA=MakeShape(a);
	const Shape_t shapeB=MakeShape(b);

	vec3 pA=shapeA.center, pB=shapeB.center;

	if(shapeA.type==SHAPE_SPHERE&&shapeB.type==SHAPE_CAPSULE)
		ClosestPoint_PointSegment(shapeA.center, &shapeB, &pB);
	else if(shapeA.type==SHAPE_CAPSULE&&shapeB.type==SHAPE_SPHERE)
		ClosestPoint_PointSegment(shapeB.center, &shapeA, &pA);
	else if(shapeA.type==SHAPE_SPHERE&&shapeB.type==SHAPE_OBB)
		ClosestPoint_PointBox(shapeA.center, &shapeB, &pB);
	else if(shapeA.type==SHAPE_OBB&&shapeB.type==SHAPE_SPHERE)
		ClosestPoint_PointBox(shapeB.center, &shapeA, &pA);
	else if(shapeA.type==SHAPE_CAPSULE&&shapeB.type==SHAPE_CAPSULE)
		ClosestPoint_SegmentSegment(&shapeA, &shapeB, &pA, &pB);
	else if(shapeA.type==SHAPE_CAPSULE&&shapeB.type==SHAPE_OBB)
		ClosestPoint_SegmentBox(&shapeA, &shapeB, &pA, &pB);
	else if(shapeA.type==SHAPE_OBB&&shapeB.type==SHAPE_CAPSULE)
		ClosestPoint_SegmentBox(&shapeB, &shapeA, &pB, &pA);

	const vec3 delta=Vec3_Subv(pB, pA);
	const float distSq=Vec3_LengthSq(delta);
	const float rSum=shapeA.radius+shapeB.radius;

	if(distSq>rSum*rSum)
		return (CollisionManifold_t) { 0 };

	const float dist=fmaxf(sqrtf(distSq), FLT_EPSILON);
	const float penetration=rSum-dist;
	const vec3 normal=distSq<FLT_EPSILON?Vec3(0.0f, 1.0f, 0.0f):Vec3_Muls(delta, 1.0f/dist);
	const vec3 contact=Vec3_Addv(pA, Vec3_Muls(normal, shapeA.radius-penetration*0.5f));

	CollisionManifold_t manifold;
	manifold.a=a;
	manifold.b=b;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

// Capsule-to-OBB collision detection, multiple manifold contact points
static CollisionManifold_t CapsuleToOBBCollision(RigidBody_t *capsule, RigidBody_t *obb)
{

	const Shape_t segShape=MakeShape(capsule);
	const Shape_t boxShape=MakeShape(obb);

	// Calculate capsule endpoints
	const vec3 offset=Vec3_Muls(segShape.axes[1], segShape.halfLength);
	const vec3 capA=Vec3_Subv(segShape.center, offset);
	const vec3 capB=Vec3_Addv(segShape.center, offset);

	ContactPoint_t candidates[3];
	uint32_t candidateCount=0;

	// Initial candidate, point anywhere along the segment.
	vec3 pSeg, pBox;
	ClosestPoint_SegmentBox(&segShape, &boxShape, &pSeg, &pBox);

	const vec3 delta=Vec3_Subv(pSeg, pBox);
	const float distSq=Vec3_LengthSq(delta);
	const float r=segShape.radius;

	if(distSq>r*r)
		return (CollisionManifold_t) { 0 };

	const float dist=fmaxf(sqrtf(distSq), FLT_EPSILON);
	const float penetration=r-dist;
	const vec3 normal=distSq<FLT_EPSILON?Vec3(0.0f, 1.0f, 0.0f):Vec3_Muls(delta, 1.0f/dist);

	candidates[candidateCount].position=Vec3_Subv(pSeg, Vec3_Muls(normal, r-penetration*0.5f));
	candidates[candidateCount].normal=normal;
	candidates[candidateCount].penetration=penetration;
	candidateCount++;

	// Additional candidates, check capsule endpoints.
	const vec3 primaryNormal=candidates[0].normal;
	const vec3 ends[2]={ capA, capB };

	for(int i=0;i<2&&candidateCount<3;i++)
	{
		vec3 pBox;
		ClosestPoint_PointBox(ends[i], &boxShape, &pBox);

		const vec3 delta=Vec3_Subv(ends[i], pBox);
		const float distSq=Vec3_LengthSq(delta);
		const float r=segShape.radius;

		if(distSq>r*r||distSq<FLT_EPSILON)
			continue;

		const float dist=sqrtf(distSq);
		const vec3 normal=Vec3_Muls(delta, 1.0f/dist);

		if(Vec3_Dot(normal, primaryNormal)<0.98f)
			continue;

		const float penetration=r-dist;
		const vec3 contact=Vec3_Subv(ends[i], Vec3_Muls(normal, r-penetration*0.5f));

		bool duplicate=false;
		for(uint32_t j=0;j<candidateCount;j++)
		{
			if(Vec3_LengthSq(Vec3_Subv(contact, candidates[j].position))<FLT_EPSILON*FLT_EPSILON)
			{
				duplicate=true;
				break;
			}
		}

		if(duplicate)
			continue;

		candidates[candidateCount].position=contact;
		candidates[candidateCount].normal=normal;
		candidates[candidateCount].penetration=penetration;
		candidateCount++;
	}

	CollisionManifold_t manifold={ 0 };
	manifold.a=obb;
	manifold.b=capsule;

	for(uint32_t i=0;i<candidateCount;i++)
		manifold.contacts[i]=candidates[i];

	manifold.contactCount=candidateCount;

	return manifold;
}

static uint32_t ClipPolygon(const vec3 *in, uint32_t inCount, vec3 *out, uint32_t maxOut, vec3 planeNormal, float planeDist)
{
    uint32_t outCount=0;

	if(inCount==0)
        return 0;

    for(uint32_t i=0;i<inCount;i++)
    {
        const vec3  curr=in[i];
        const vec3  next=in[(i+1)%inCount];
        const float dc=Vec3_Dot(curr, planeNormal)-planeDist;
        const float dn=Vec3_Dot(next, planeNormal)-planeDist;
        const bool insideC=(dc<=0.0f);
        const bool insideN=(dn<=0.0f);

        if(insideC&&outCount<maxOut)
            out[outCount++] = curr;

		if(insideC!=insideN&&outCount<maxOut)
            out[outCount++]=Vec3_Addv(curr, Vec3_Muls(Vec3_Subv(next, curr), dc/(dc-dn)));
    }

    return outCount;
}

static bool TestSATAxis(vec3 axis, const vec3 axesA[3], vec3 sA, const vec3 axesB[3], vec3 sB, vec3 relPos, uint32_t axisIndex, float *penetration, vec3 *normal, uint32_t *minAxisIndex)
{
	const float rA=fabsf(Vec3_Dot(axesA[0], axis))*sA.x+fabsf(Vec3_Dot(axesA[1], axis))*sA.y+fabsf(Vec3_Dot(axesA[2], axis))*sA.z;
    const float rB=fabsf(Vec3_Dot(axesB[0], axis))*sB.x+fabsf(Vec3_Dot(axesB[1], axis))*sB.y+fabsf(Vec3_Dot(axesB[2], axis))*sB.z;
    const float dist=fabsf(Vec3_Dot(relPos, axis));
    float overlap=rA+rB-dist;

	// Separating axis found — no collision.
    if(overlap<-FLT_EPSILON)
		return false;

    overlap=fmaxf(overlap, 0.0f);

    if(overlap<*penetration)
	{
		*penetration=overlap;
		*normal=axis;
		*minAxisIndex=axisIndex;
	}

	return true;
}

static bool TestCrossSATAxis(vec3 edgeA, vec3 edgeB, const vec3 axesA[3], vec3 sA, const vec3 axesB[3], vec3 sB, vec3 relPos, uint32_t axisIndex, float *penetration, vec3 *normal, uint32_t *minAxisIndex)
{
	vec3 axis=Vec3_Cross(edgeA, edgeB);

	if(Vec3_Normalize(&axis)<=FLT_EPSILON)
		return true;

    return TestSATAxis(axis, axesA, sA, axesB, sB, relPos, axisIndex, penetration, normal, minAxisIndex);
}

static void ReduceManifoldContacts(CollisionManifold_t *m)
{
	if(m->contactCount<=4)
		return;

	// Keep the deepest penetration first.
	uint32_t bestIndex=0;

	for(uint32_t i=1;i<m->contactCount;i++)
	{
		if(m->contacts[i].penetration>m->contacts[bestIndex].penetration)
			bestIndex=i;
	}

	ContactPoint_t reduced[4]={ m->contacts[bestIndex] };
	m->contacts[bestIndex]=m->contacts[--m->contactCount];

	// Greedily pick the point farthest from the current set.
	uint32_t reducedCount=1;

	while(reducedCount<4&&m->contactCount>0)
	{
		float bestDistance=-1.0f;
		uint32_t bestCandidate=0;

		for(uint32_t i=0;i<m->contactCount;i++)
		{
			const vec3 candidatePos=m->contacts[i].position;
			float nearestDistance=FLT_MAX;

			for(uint32_t j=0;j<reducedCount;j++)
			{
				const float distance=Vec3_LengthSq(Vec3_Subv(candidatePos, reduced[j].position));

				if(distance<nearestDistance)
					nearestDistance=distance;
			}

			if(nearestDistance>bestDistance)
			{
				bestDistance=nearestDistance;
				bestCandidate=i;
			}
		}

		reduced[reducedCount++]=m->contacts[bestCandidate];
		m->contacts[bestCandidate]=m->contacts[--m->contactCount];
	}

	m->contactCount=reducedCount;

	for(uint32_t i=0;i<reducedCount;i++)
		m->contacts[i]=reduced[i];
}

#define MAX_CLIP_VERTS 16

// OBB-to-OBB collision detection, multiple manifold contact points
static CollisionManifold_t OBBToOBBCollision(RigidBody_t *a, RigidBody_t *b)
{
    // Extract axes
    vec3 axesA[3], axesB[3];
    QuatAxes(a->orientation, axesA);
    QuatAxes(b->orientation, axesB);

    const vec3 relPos=Vec3_Subv(b->position, a->position);

	float penetration=FLT_MAX;
	vec3 normal=Vec3b(0.0f);
	uint32_t minAxisIndex=0;

    if(!TestSATAxis(axesA[0], axesA, a->size, axesB, b->size, relPos, 0, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestSATAxis(axesA[1], axesA, a->size, axesB, b->size, relPos, 1, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestSATAxis(axesA[2], axesA, a->size, axesB, b->size, relPos, 2, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestSATAxis(axesB[0], axesA, a->size, axesB, b->size, relPos, 3, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestSATAxis(axesB[1], axesA, a->size, axesB, b->size, relPos, 4, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestSATAxis(axesB[2], axesA, a->size, axesB, b->size, relPos, 5, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };

	if(!TestCrossSATAxis(axesA[0], axesB[0], axesA, a->size, axesB, b->size, relPos, 6, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[0], axesB[1], axesA, a->size, axesB, b->size, relPos, 7, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[0], axesB[2], axesA, a->size, axesB, b->size, relPos, 8, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[1], axesB[0], axesA, a->size, axesB, b->size, relPos, 9, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[1], axesB[1], axesA, a->size, axesB, b->size, relPos, 10, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[1], axesB[2], axesA, a->size, axesB, b->size, relPos, 11, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[2], axesB[0], axesA, a->size, axesB, b->size, relPos, 12, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[2], axesB[1], axesA, a->size, axesB, b->size, relPos, 13, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };
    if(!TestCrossSATAxis(axesA[2], axesB[2], axesA, a->size, axesB, b->size, relPos, 14, &penetration, &normal, &minAxisIndex)) return (CollisionManifold_t) { 0 };

	// No separating axis found

	// Ensure the collision normal points from A to B
    if(Vec3_Dot(normal, relPos)<0.0f)
        normal=Vec3_Muls(normal, -1.0f);

	Vec3_Normalize(&normal);

    // Edge to edge contact
    if(minAxisIndex>=6)
    {
        const vec3 pA=Vec3_Addv(
			Vec3_Addv(
				Vec3_Addv(a->position,
					Vec3_Muls(axesA[0], (Vec3_Dot(normal, axesA[0])>=0.0f)?a->size.x:-a->size.x)),
					Vec3_Muls(axesA[1], (Vec3_Dot(normal, axesA[1])>=0.0f)?a->size.y:-a->size.y)),
					Vec3_Muls(axesA[2], (Vec3_Dot(normal, axesA[2])>=0.0f)?a->size.z:-a->size.z)
		);
        const vec3 pB=Vec3_Addv(
			Vec3_Addv(
				Vec3_Addv(b->position,
					Vec3_Muls(axesB[0], (Vec3_Dot(normal, axesB[0])<=0.0f)?b->size.x:-b->size.x)),
					Vec3_Muls(axesB[1], (Vec3_Dot(normal, axesB[1])<=0.0f)?b->size.y:-b->size.y)),
					Vec3_Muls(axesB[2], (Vec3_Dot(normal, axesB[2])<=0.0f)?b->size.z:-b->size.z)
		);

        CollisionManifold_t manifold;
        manifold.a=a;
        manifold.b=b;
        manifold.contacts[0].position=Vec3_Muls(Vec3_Addv(pA, pB), 0.5f);
        manifold.contacts[0].normal=normal;
        manifold.contacts[0].penetration=penetration;
        manifold.contactCount=1;

		return manifold;
    }

    // Face contact
    const bool refIsA=(minAxisIndex<3);
    const RigidBody_t *ref=refIsA?a:b;
    const RigidBody_t *inc=refIsA?b:a;
    const vec3 *refAxes=refIsA?axesA:axesB;
    const vec3 *incAxes=refIsA?axesB:axesA;

    const uint32_t refFaceAxis=refIsA?minAxisIndex:minAxisIndex-3;
    const vec3 refFaceNormal=refIsA?normal:Vec3_Muls(normal, -1.0f);

	uint32_t incFaceAxis=0;
	float incFaceSign=1.0f;

	const float di0=Vec3_Dot(incAxes[0], refFaceNormal), adi0=fabsf(di0);
	const float di1=Vec3_Dot(incAxes[1], refFaceNormal), adi1=fabsf(di1);
	const float di2=Vec3_Dot(incAxes[2], refFaceNormal), adi2=fabsf(di2);

	if(adi0>=adi1&&adi0>=adi2)
	{
		incFaceAxis=0;
		incFaceSign=di0<=0.0f?1.0f:-1.0f;
	}
	else if(adi1>=adi2)
	{
		incFaceAxis=1;
		incFaceSign=di1<=0.0f?1.0f:-1.0f;
	}
	else
	{
		incFaceAxis=2;
		incFaceSign=di2<=0.0f?1.0f:-1.0f;
	}

    // Build the corners of the face
	const vec3 incFaceCenter=Vec3_Addv(inc->position, Vec3_Muls(incAxes[incFaceAxis], incFaceSign*inc->size.v[incFaceAxis]));

	const uint32_t it1=(incFaceAxis+1)%3;
    const uint32_t it2=(incFaceAxis+2)%3;
    const vec3 ie1=Vec3_Muls(incAxes[it1], inc->size.v[it1]);
    const vec3 ie2=Vec3_Muls(incAxes[it2], inc->size.v[it2]);

    vec3 buf0[MAX_CLIP_VERTS], buf1[MAX_CLIP_VERTS];
    buf0[0]=Vec3_Addv(Vec3_Addv(incFaceCenter, ie1), ie2);
    buf0[1]=Vec3_Addv(Vec3_Subv(incFaceCenter, ie1), ie2);
    buf0[2]=Vec3_Subv(Vec3_Subv(incFaceCenter, ie1), ie2);
    buf0[3]=Vec3_Subv(Vec3_Addv(incFaceCenter, ie1), ie2);
    uint32_t count=4;

    const uint32_t rt1=(refFaceAxis+1)%3;
    const uint32_t rt2=(refFaceAxis+2)%3;
    const float rs1=ref->size.v[rt1];
    const float rs2=ref->size.v[rt2];

    const float refDotRt1=Vec3_Dot(ref->position, refAxes[rt1]);
    const float refDotRt2=Vec3_Dot(ref->position, refAxes[rt2]);
    const float refFaceDist=Vec3_Dot(ref->position, refFaceNormal)+ref->size.v[refFaceAxis];

    count=ClipPolygon(buf0, count, buf1, MAX_CLIP_VERTS, refAxes[rt1], refDotRt1+rs1);
	if(!count) return (CollisionManifold_t) { 0 };
    count=ClipPolygon(buf1, count, buf0, MAX_CLIP_VERTS, Vec3_Muls(refAxes[rt1], -1.0f), -refDotRt1+rs1);
	if(!count) return (CollisionManifold_t) { 0 };
    count=ClipPolygon(buf0, count, buf1, MAX_CLIP_VERTS, refAxes[rt2], refDotRt2+rs2);
    if(!count) return (CollisionManifold_t) { 0 };
    count=ClipPolygon(buf1, count, buf0, MAX_CLIP_VERTS, Vec3_Muls(refAxes[rt2], -1.0f), -refDotRt2+rs2);
    if(!count) return (CollisionManifold_t) { 0 };
    count=ClipPolygon(buf0, count, buf1, MAX_CLIP_VERTS, refFaceNormal, refFaceDist);
    if(!count) return (CollisionManifold_t) { 0 };

    CollisionManifold_t manifold;
    manifold.a=a;
    manifold.b=b;
    manifold.contactCount=0;

    for(uint32_t i=0;i<count&&manifold.contactCount<MAX_CONTACTS_PER_MANIFOLD;i++)
    {
		manifold.contacts[manifold.contactCount].position=buf1[i];
        manifold.contacts[manifold.contactCount].normal=normal;
        manifold.contacts[manifold.contactCount].penetration=fmaxf(refFaceDist-Vec3_Dot(buf1[i], refFaceNormal), 0.0f);
        manifold.contactCount++;
    }

	ReduceManifoldContacts(&manifold);

	return manifold;
}

// Perform collision detection between two rigid bodies and return the collision manifold.
CollisionManifold_t PhysicsCollision(RigidBody_t *a, RigidBody_t *b)
{
	// Special case collisions, these make more sense to handle separately than the general shape-to-shape collision.
	if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_OBB)
		return OBBToOBBCollision(a, b);
	if(a->type==RIGIDBODY_CAPSULE&&b->type==RIGIDBODY_OBB)
		return CapsuleToOBBCollision(a, b);
	if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_CAPSULE)
		return CapsuleToOBBCollision(b, a);

	// General shape-to-shape collision detection.
	return ShapeToShapeCollision(a, b);
}
