#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include "physics.h"

static void ApplyConstraints(RigidBody_t *body, const float dt)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	body->velocity=Vec3_Clamp(body->velocity, -maxVelocity, maxVelocity);

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 normal=Vec3_Subv(body->position, center);

	float distanceSq=Vec3_LengthSq(normal);
	const float maxRadiusMinusRadius = maxRadius - body->radius;
	const float boundarySq = maxRadiusMinusRadius * maxRadiusMinusRadius;

	if(distanceSq > boundarySq)
	{
		const float distance=Vec3_Normalize(&normal);
		const float penetration = distance + body->radius - maxRadius;
		body->force=Vec3_Addv(body->force, Vec3_Muls(normal, -penetration * 100.0f)); // Arbitrary stiffness
	}

	// Dampen velocity
	const float lambda=0.1f;
	const float decay=expf(-lambda*dt);

	body->velocity=Vec3_Muls(body->velocity, decay);
	body->angularVelocity=Vec3_Muls(body->angularVelocity, decay);
}

static vec4 IntegrateAngularVelocity(const vec4 q, const vec3 w, const float dt)
{
	const float halfDT=0.5f*dt;

	// First Midpoint step
	vec4 k1=Vec4_Muls(Vec4(
		 q.w*w.x+q.y*w.z-q.z*w.y,
		 q.w*w.y-q.x*w.z+q.z*w.x,
		 q.w*w.z+q.x*w.y-q.y*w.x,
		-q.x*w.x-q.y*w.y-q.z*w.z
	), halfDT);

	vec4 result=Vec4_Addv(q, k1);

	// Second Midpoint step
	vec4 k2=Vec4_Muls(Vec4(
		 result.w*w.x+result.y*w.z-result.z*w.y,
		 result.w*w.y-result.x*w.z+result.z*w.x,
		 result.w*w.z+result.x*w.y-result.y*w.x,
		-result.x*w.x-result.y*w.y-result.z*w.z
	), halfDT);

	result=Vec4_Addv(q, k2);

	Vec4_Normalize(&result);

	return result;
}

void PhysicsIntegrate(RigidBody_t *body, const float dt)
{
	// const vec3 gravity=Vec3(0.0f, -9.81f*WORLD_SCALE, 0.0f);
	const vec3 gravity=Vec3b(0.0f);

	// Apply gravity
	body->force=Vec3_Addv(body->force, Vec3_Muls(gravity, body->mass));

	// Implicit Euler integration of position and velocity
	// Velocity+=Force/Mass*dt
	// Position+=Velocity*dt

	body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(body->force, body->invMass*dt));
	body->position=Vec3_Addv(body->position, Vec3_Muls(body->velocity, dt));

	body->force=Vec3b(0.0f);

	// Integrate angular velocity using quaternions
	body->orientation=IntegrateAngularVelocity(body->orientation, body->angularVelocity, dt);

	ApplyConstraints(body, dt);
}

void PhysicsExplode(RigidBody_t *body)
{
	const vec3 explosion_center={ 0.0f, 0.0f, 0.0f };

	// Calculate direction from explosion center to fragment
	vec3 direction=Vec3_Subv(body->position, explosion_center);
	Vec3_Normalize(&direction);

	// Calculate acceleration and impulse force
	const vec3 acceleration=Vec3_Muls(direction, EXPLOSION_POWER);

	// F=M*A bla bla...
	const vec3 force=Vec3_Muls(acceleration, body->mass);

	// Add it into object's velocity
	body->velocity=Vec3_Addv(body->velocity, force);
}

void PhysicsApplyImpulse(RigidBody_t *body, const vec3 impulse, const vec3 point)
{
	// Linear impulse
	body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(impulse, body->invMass));

	// Torque arm in local space
	const vec3 torque=Vec3_Cross(Vec3_Subv(point, body->position), impulse);
	const vec3 localTorque=QuatRotate(QuatInverse(body->orientation), torque);

	// Update angular velocity
	body->angularVelocity=Vec3_Addv(body->angularVelocity, Vec3_Muls(localTorque, body->invInertia));
}

float PhysicsResolveCollision(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact)
{
    // Torque arms
	const vec3 r1=Vec3_Subv(contact.position, a->position);
	const vec3 r2=Vec3_Subv(contact.position, b->position);

    const vec3 wA=QuatRotate(a->orientation, a->angularVelocity);
    const vec3 wB=QuatRotate(b->orientation, b->angularVelocity);

	const vec3 relativeVel=Vec3_Subv(
        Vec3_Addv(b->velocity, Vec3_Cross(wB, r2)),
        Vec3_Addv(a->velocity, Vec3_Cross(wA, r1))
    );

	const float relativeSpeed=Vec3_Dot(relativeVel, contact.normal);

	if(relativeSpeed>0.0f)
        return 0.0f;

    // Masses
	const vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, contact.normal), a->invInertia), r1);
	const vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, contact.normal), b->invInertia), r2);
	const float invMassSum=a->invMass+b->invMass;

	const float e=fminf(a->restitution, b->restitution);
	const float j=-(1.0f+e)*relativeSpeed/(invMassSum+Vec3_Dot(contact.normal, Vec3_Addv(d1, d2)));

	const vec3 impulse=Vec3_Muls(contact.normal, j);

	// Head-on collision velocities

    // Linear velocity
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulse, a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulse, b->invMass));

    // Pre-calculate inverse orientation quats
	const vec4 invOrientationA=QuatInverse(a->orientation);
	const vec4 invOrientationB=QuatInverse(b->orientation);

	// Transform torque to local space
	vec3 localTorqueA=QuatRotate(invOrientationA, Vec3_Cross(r1, impulse));
	vec3 localTorqueB=QuatRotate(invOrientationB, Vec3_Cross(r2, impulse));

    // Angular velocity
	a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(localTorqueA, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(localTorqueB, b->invInertia));

	// Calculate tangential velocities
	vec3 tangentialVel=Vec3_Subv(relativeVel, Vec3_Muls(contact.normal, Vec3_Dot(relativeVel, contact.normal)));
    Vec3_Normalize(&tangentialVel);

	const vec3 d1T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, tangentialVel), a->invInertia), r1);
	const vec3 d2T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, tangentialVel), b->invInertia), r2);

	const float friction=sqrtf(a->friction*b->friction);
	const float maxjT=friction*j;

	const float jT=clampf(-Vec3_Dot(relativeVel, tangentialVel)/(invMassSum+Vec3_Dot(tangentialVel, Vec3_Addv(d1T, d2T))), -maxjT, maxjT);

	const vec3 impulseT=Vec3_Muls(tangentialVel, jT);

	// Linear frictional velocity
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulseT, a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulseT, b->invMass));

	// Angular frictional velocity
	localTorqueA=QuatRotate(invOrientationA, Vec3_Cross(r1, impulseT));
	localTorqueB=QuatRotate(invOrientationB, Vec3_Cross(r2, impulseT));

	a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(localTorqueA, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(localTorqueB, b->invInertia));

    return sqrtf(-relativeSpeed);
}

void PhysicsPositionCorrection(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact)
{
	const float penetration=contact.penetration;
	const vec3 normal=contact.normal;
	const float invMassSum=a->invMass+b->invMass;

	if(invMassSum>FLT_EPSILON)
	{
		const float penetrationSlop=0.01f;
		const float percent=0.2f;
		const float correctionAmount=fmaxf(penetration-penetrationSlop, 0.0f)*percent;
		const vec3 correction=Vec3_Muls(normal, correctionAmount/invMassSum);

		a->position=Vec3_Subv(a->position, Vec3_Muls(correction, a->invMass));
		b->position=Vec3_Addv(b->position, Vec3_Muls(correction, b->invMass));
	}
}

static CollisionManifold_t SphereToSphereCollision(RigidBody_t *a, RigidBody_t *b)
{
	const vec3 relativePosition=Vec3_Subv(b->position, a->position);
	const float distanceSq=Vec3_LengthSq(relativePosition);
	const float radiiSum=a->radius+b->radius;

	if(distanceSq>radiiSum*radiiSum)
		return (CollisionManifold_t) { 0 };

	// Penetration
	const float distance=fmaxf(sqrtf(distanceSq), FLT_EPSILON);
	const float penetration=distance-radiiSum;

	// Normal
	const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

	// Contact point
	const vec3 contact=Vec3_Addv(a->position, Vec3_Muls(normal, a->radius-penetration*0.5f));

	CollisionManifold_t manifold;
	manifold.a=a;
	manifold.b=b;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

static CollisionManifold_t SphereToOBBCollision(RigidBody_t *sphere, RigidBody_t *obb)
{
	vec3 axes[3];
	QuatAxes(obb->orientation, axes);

	const vec3 relativeCenter=Vec3_Subv(sphere->position, obb->position);
	vec3 closestPoint=obb->position;
	closestPoint=Vec3_Addv(closestPoint, Vec3_Muls(axes[0], fmaxf(-obb->size.x, fminf(Vec3_Dot(relativeCenter, axes[0]), obb->size.x))));
	closestPoint=Vec3_Addv(closestPoint, Vec3_Muls(axes[1], fmaxf(-obb->size.y, fminf(Vec3_Dot(relativeCenter, axes[1]), obb->size.y))));
	closestPoint=Vec3_Addv(closestPoint, Vec3_Muls(axes[2], fmaxf(-obb->size.z, fminf(Vec3_Dot(relativeCenter, axes[2]), obb->size.z))));

	const vec3 relativePosition=Vec3_Subv(sphere->position, closestPoint);
	const float distanceSq=Vec3_LengthSq(relativePosition);

	if(distanceSq>sphere->radius*sphere->radius)
	{
		// No collision
		return (CollisionManifold_t) { 0 };
	}

	// Penetration
	const float distance=fmaxf(sqrtf(distanceSq), FLT_EPSILON);
	const float penetration=distance-sphere->radius;

	// Normal
	const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

	// Contact point
	const vec3 contact=Vec3_Subv(closestPoint, Vec3_Muls(normal, penetration*0.5f));

	CollisionManifold_t manifold;
	manifold.b=sphere;
	manifold.a=obb;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

static uint32_t ClipPolygon(const vec3 *in, uint32_t inCount, vec3 *out, vec3 planeNormal, float planeDist)
{
    uint32_t outCount=0;

	if(inCount==0)
		return 0;

    for(uint32_t i=0;i<inCount;i++)
    {
        const vec3 curr=in[i];
        const vec3 next=in[(i+1)%inCount];

		const float dc=Vec3_Dot(curr, planeNormal)-planeDist;
		const float dn=Vec3_Dot(next, planeNormal)-planeDist;

		const bool insideC=dc<=0.0f;
		const bool insideN=dn<=0.0f;

		if(insideC)
			out[outCount++]=curr;

		if(insideC!=insideN)
			out[outCount++]=Vec3_Addv(curr, Vec3_Muls(Vec3_Subv(next, curr), dc/(dc-dn)));
	}

	return outCount;
}

static CollisionManifold_t OBBToOBBCollision(RigidBody_t *a, RigidBody_t *b)
{
	// Extract axes
    vec3 axesA[3], axesB[3];
    QuatAxes(a->orientation, axesA);
    QuatAxes(b->orientation, axesB);

	// Compute relative position
	const vec3 relativePosition=Vec3_Subv(b->position, a->position);

	// List of axes to test, at minimum test base axes of A and B OBBs
    uint32_t axisCount=6;
    vec3 axes[15]={
		axesA[0], axesA[1], axesA[2],
		axesB[0], axesB[1], axesB[2],
    };

	// Cross products add additional axes to test
	for(uint32_t i=0;i<3;i++)
	{
		for(uint32_t j=0;j<3;j++)
        {
			vec3 axis=Vec3_Cross(axesA[i], axesB[j]);
			const float length=Vec3_Normalize(&axis);

			if(length>FLT_EPSILON)
				axes[axisCount++]=axis;
        }
	}

	// Minimum penetration depth and corresponding axis
	float penetration=FLT_MAX;
	vec3 normal=Vec3b(0.0f);
	uint32_t minAxisIndex=0;

	// Test axes
	const float collisionEpsilon=1e-4f;
	for(uint32_t i=0;i<axisCount;i++)
    {
		// Project OBBs onto axis
		const float rA=fabsf(Vec3_Dot(axesA[0], axes[i]))*a->size.x+fabsf(Vec3_Dot(axesA[1], axes[i]))*a->size.y+fabsf(Vec3_Dot(axesA[2], axes[i]))*a->size.z;
		const float rB=fabsf(Vec3_Dot(axesB[0], axes[i]))*b->size.x+fabsf(Vec3_Dot(axesB[1], axes[i]))*b->size.y+fabsf(Vec3_Dot(axesB[2], axes[i]))*b->size.z;
		const float distance=fabsf(Vec3_Dot(relativePosition, axes[i]));

		float overlap=rA+rB-distance;

		// Separating axis found, no collision
		if(overlap<-collisionEpsilon)
			return (CollisionManifold_t) { 0 };

		// Clamp small negative overlaps to zero
		overlap=fmaxf(overlap, 0.0f);

		if(overlap<penetration)
        {
			// Update minimum penetration depth and collision normal
			penetration=overlap;
            normal=axes[i];
			minAxisIndex=i;
		}
	}

	// No separating axis found

	// Ensure the collision normal points from A to B
	if(Vec3_Dot(normal, relativePosition)<0.0f)
		normal=Vec3_Muls(normal, -1.0f);

	Vec3_Normalize(&normal);

	// Single point contact
	if(minAxisIndex>=6)
	{
		const vec3 sA=Vec3(
			(Vec3_Dot(normal, axesA[0])>=0.0f)?a->size.x:-a->size.x,
			(Vec3_Dot(normal, axesA[1])>=0.0f)?a->size.y:-a->size.y,
			(Vec3_Dot(normal, axesA[2])>=0.0f)?a->size.z:-a->size.z
		);

		vec3 pA=a->position;
		pA=Vec3_Addv(pA, Vec3_Muls(axesA[0], sA.x));
		pA=Vec3_Addv(pA, Vec3_Muls(axesA[1], sA.y));
		pA=Vec3_Addv(pA, Vec3_Muls(axesA[2], sA.z));

		const vec3 sB=Vec3(
			(Vec3_Dot(normal, axesB[0])<=0.0f)?b->size.x:-b->size.x,
			(Vec3_Dot(normal, axesB[1])<=0.0f)?b->size.y:-b->size.y,
			(Vec3_Dot(normal, axesB[2])<=0.0f)?b->size.z:-b->size.z
		);

		vec3 pB=b->position;
		pB=Vec3_Addv(pB, Vec3_Muls(axesB[0], sB.x));
		pB=Vec3_Addv(pB, Vec3_Muls(axesB[1], sB.y));
		pB=Vec3_Addv(pB, Vec3_Muls(axesB[2], sB.z));

		const vec3 contact=Vec3_Muls(Vec3_Addv(pA, pB), 0.5f);

		CollisionManifold_t manifold;
		manifold.a=a;
		manifold.b=b;
		manifold.contacts[0].position=contact;
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

    float minDot=FLT_MAX;
    uint32_t incFaceAxis=0;
    float incFaceSign=1.0f;

	for(uint32_t i=0;i<3;i++)
    {
        const float d=Vec3_Dot(incAxes[i], refFaceNormal);

		if(d<minDot)
		{
			minDot=d;
			incFaceAxis=i;
			incFaceSign=1.0f;
		}

		if(-d<minDot)
		{
			minDot=-d;
			incFaceAxis=i;
			incFaceSign=-1.0f;
		}
    }

    // Build the corners of the face
    const vec3 incFaceCenter=Vec3_Addv(inc->position, Vec3_Muls(incAxes[incFaceAxis], incFaceSign*inc->size.v[incFaceAxis]));

    const uint32_t it1=(incFaceAxis+1)%3;
    const uint32_t it2=(incFaceAxis+2)%3;
    const vec3 ie1=Vec3_Muls(incAxes[it1], inc->size.v[it1]);
    const vec3 ie2=Vec3_Muls(incAxes[it2], inc->size.v[it2]);

    // Ping-pong between two buffers while clipping
    vec3 buf0[8], buf1[8];
    buf0[0]=Vec3_Addv(Vec3_Addv(incFaceCenter,  ie1),  ie2);
    buf0[1]=Vec3_Addv(Vec3_Subv(incFaceCenter,  ie1),  ie2);
    buf0[2]=Vec3_Subv(Vec3_Subv(incFaceCenter,  ie1),  ie2);
    buf0[3]=Vec3_Subv(Vec3_Addv(incFaceCenter,  ie1),  ie2);
    uint32_t count=4;

    // Clip against the 4 side planes of the reference face
    const uint32_t rt1=(refFaceAxis+1)%3;
    const uint32_t rt2=(refFaceAxis+2)%3;
    const float rs1=ref->size.v[rt1];
    const float rs2=ref->size.v[rt2];
    const float refDotRt1=Vec3_Dot(ref->position, refAxes[rt1]);
    const float refDotRt2=Vec3_Dot(ref->position, refAxes[rt2]);

    count=ClipPolygon(buf0, count, buf1, refAxes[rt1], refDotRt1+rs1);

	if(!count)
		return (CollisionManifold_t) { 0 };

	count=ClipPolygon(buf1, count, buf0, Vec3_Muls(refAxes[rt1], -1.0f), -refDotRt1+rs1);

	if(!count)
		return (CollisionManifold_t) { 0 };

	count=ClipPolygon(buf0, count, buf1, refAxes[rt2], refDotRt2+rs2);

	if(!count)
		return (CollisionManifold_t) { 0 };

	count=ClipPolygon(buf1, count, buf0, Vec3_Muls(refAxes[rt2], -1.0f), -refDotRt2+rs2);

	if(!count)
		return (CollisionManifold_t) { 0 };

    const float refFaceDist=Vec3_Dot(ref->position, refFaceNormal)+ref->size.v[refFaceAxis];

	// Clip against the reference face itself (depth plane)
	count=ClipPolygon(buf0, count, buf1, refFaceNormal, refFaceDist);

	if(!count)
		return (CollisionManifold_t) { 0 };

	CollisionManifold_t manifold;
	manifold.a=a;
	manifold.b=b;
	manifold.contactCount=0;

	for(uint32_t i=0;i<(uint32_t)count;i++)
    {
        const float d=Vec3_Dot(buf1[i], refFaceNormal);

		if(d<=refFaceDist+FLT_EPSILON)
        {
			manifold.contacts[manifold.contactCount].position=buf1[i];
			manifold.contacts[manifold.contactCount].normal=normal;
			manifold.contacts[manifold.contactCount].penetration=refFaceDist-d;
			manifold.contactCount++;
        }
    }

	return manifold;
}

static CollisionManifold_t CapsuleToSphereCollision(RigidBody_t *capsule, RigidBody_t *sphere)
{
	// Capsule endpoints
	vec3 axes[3];
	QuatAxes(capsule->orientation, axes);

	vec3 offset=Vec3_Muls(axes[1], capsule->size.y);

	vec3 a=Vec3_Subv(capsule->position, offset);
	vec3 b=Vec3_Addv(capsule->position, offset);

	// Closest point on capsule segment to sphere center
	vec3 slope=Vec3_Subv(b, a);
	float slopeLen=Vec3_Dot(slope, slope);
	vec3 closest=Vec3_Addv(a, Vec3_Muls(slope, clampf(Vec3_Dot(Vec3_Subv(sphere->position, a), slope)/slopeLen, 0.0, 1.0)));

	vec3 delta=Vec3_Subv(sphere->position, closest);

	float distSq=Vec3_LengthSq(delta);
	float r=capsule->radius+sphere->radius;

	if(distSq>r*r)
		return (CollisionManifold_t) { 0 };

	float dist=fmaxf(sqrtf(distSq), FLT_EPSILON); // Avoid zero for degenerate case
	float penetration=r-dist;

	vec3 normal=distSq < FLT_EPSILON ? Vec3(0.0f, 1.0f, 0.0f) : Vec3_Muls(delta, 1.0f/dist); // Fallback to world up if coincident
	vec3 contact=Vec3_Subv(sphere->position, Vec3_Muls(normal, sphere->radius-penetration*0.5f));

	CollisionManifold_t manifold;
	manifold.a=capsule;
	manifold.b=sphere;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

static CollisionManifold_t CapsuleToCapsuleCollision(RigidBody_t *a, RigidBody_t *b)
{
	vec3 axes[3], offset;

	// Capsule A endpoints
	QuatAxes(a->orientation, axes);
	offset=Vec3_Muls(axes[1], a->size.y);
	vec3 a0=Vec3_Subv(a->position, offset);
	vec3 a1=Vec3_Addv(a->position, offset);

	// Capsule B endpoints
	QuatAxes(b->orientation, axes);
	offset=Vec3_Muls(axes[1], b->size.y);
	vec3 b0=Vec3_Subv(b->position, offset);
	vec3 b1=Vec3_Addv(b->position, offset);

	// Closest points between segments
	vec3 d1=Vec3_Subv(a1, a0);
	vec3 d2=Vec3_Subv(b1, b0);
	vec3 r=Vec3_Subv(a0, b0);

	float aDot=Vec3_Dot(d1, d1);
	float eDot=Vec3_Dot(d2, d2);
	float fDot=Vec3_Dot(d2, r);

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
		float cDot=Vec3_Dot(d1, r);

		if(eDot<=FLT_EPSILON)
		{
			t=0.0f;
			s=clampf(-cDot/aDot, 0.0f, 1.0f);
		}
		else
		{
			float bDot=Vec3_Dot(d1, d2);
			float denom=aDot*eDot-bDot*bDot;

			if(fabsf(denom)>FLT_EPSILON)
				s=clampf((bDot*fDot-cDot*eDot)/denom, 0.0f, 1.0f);
			else
				s=0.0f;

			t=(bDot*s+fDot)/eDot;
			t=clampf(t, 0.0f, 1.0f);
		}
	}

	vec3 pA=Vec3_Addv(a0, Vec3_Muls(d1, s));
	vec3 pB=Vec3_Addv(b0, Vec3_Muls(d2, t));

	vec3 delta=Vec3_Subv(pB, pA);
	float distSq=Vec3_LengthSq(delta);

	float rSum=a->radius+b->radius;

	if(distSq>rSum*rSum)
		return (CollisionManifold_t) { 0 };

	float dist=fmaxf(sqrtf(distSq), FLT_EPSILON); // Avoid zero for degenerate case
	float penetration=rSum-dist;

	vec3 normal=distSq < FLT_EPSILON ? Vec3(0.0f, 1.0f, 0.0f) : Vec3_Muls(delta, 1.0f/dist); // Fallback to world up if coincident
	vec3 contact=Vec3_Muls(Vec3_Addv(pA, pB), 0.5f);

	CollisionManifold_t manifold;
	manifold.a=a;
	manifold.b=b;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

static CollisionManifold_t CapsuleToOBBCollision(RigidBody_t *capsule, RigidBody_t *obb)
{
	// Capsule endpoints
	vec3 capAxes[3];
	QuatAxes(capsule->orientation, capAxes);

	vec3 offset=Vec3_Muls(capAxes[1], capsule->size.y);

	vec3 capA=Vec3_Subv(capsule->position, offset);
	vec3 capB=Vec3_Addv(capsule->position, offset);

	// OBB axes
	vec3 axes[3];
	QuatAxes(obb->orientation, axes);

	// Transform capsule endpoints into OBB local space
	vec3 relA=Vec3_Subv(capA, obb->position);
	vec3 aLocal=Vec3(
		Vec3_Dot(relA, axes[0]),
		Vec3_Dot(relA, axes[1]),
		Vec3_Dot(relA, axes[2])
	);

	vec3 relB=Vec3_Subv(capB, obb->position);
	vec3 bLocal=Vec3(
		Vec3_Dot(relB, axes[0]),
		Vec3_Dot(relB, axes[1]),
		Vec3_Dot(relB, axes[2])
	);

	vec3 dLocal=Vec3_Subv(bLocal, aLocal);

	// Perform ternary search along capsule segment in OBB local space
	float lo=0.0f, hi=1.0f;
	float bestT=0.0f;
	float bestDistSq=FLT_MAX;

	for(int i=0;i<20;i++)
	{
		float t1=lo+(hi-lo)*(1.0f/3.0f);
		float t2=hi-(hi-lo)*(1.0f/3.0f);

		// Closest point on OBB to capsule point at t1 and t2
		vec3 p1=Vec3_Addv(aLocal, Vec3_Muls(dLocal, t1));
		vec3 c1=Vec3(
			clampf(p1.x, -obb->size.x, obb->size.x),
			clampf(p1.y, -obb->size.y, obb->size.y),
			clampf(p1.z, -obb->size.z, obb->size.z)
		);
		float d1=Vec3_LengthSq(Vec3_Subv(p1, c1));

		vec3 p2=Vec3_Addv(aLocal, Vec3_Muls(dLocal, t2));
		vec3 c2=Vec3(
			clampf(p2.x, -obb->size.x, obb->size.x),
			clampf(p2.y, -obb->size.y, obb->size.y),
			clampf(p2.z, -obb->size.z, obb->size.z)
		);
		float d2=Vec3_LengthSq(Vec3_Subv(p2, c2));

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
	vec3 pLocal=Vec3_Addv(aLocal, Vec3_Muls(dLocal, bestT));
	vec3 closestLocal=Vec3(
		clampf(pLocal.x, -obb->size.x, obb->size.x),
		clampf(pLocal.y, -obb->size.y, obb->size.y),
		clampf(pLocal.z, -obb->size.z, obb->size.z)
	);
	float distSq=Vec3_LengthSq(Vec3_Subv(pLocal, closestLocal));

	if(distSq>capsule->radius*capsule->radius||distSq<FLT_EPSILON)
		return (CollisionManifold_t) { 0 };

	float dist=sqrtf(distSq);
	float penetration=capsule->radius-dist;

	// Convert closest point on OBB back to world space
	vec3 closestWorld=obb->position;
	closestWorld=Vec3_Addv(closestWorld, Vec3_Muls(axes[0], closestLocal.x));
	closestWorld=Vec3_Addv(closestWorld, Vec3_Muls(axes[1], closestLocal.y));
	closestWorld=Vec3_Addv(closestWorld, Vec3_Muls(axes[2], closestLocal.z));

	// Closest point on capsule axis in world space
	vec3 pSegWorld=Vec3_Addv(capA, Vec3_Muls(Vec3_Subv(capB, capA), bestT));

	vec3 delta=Vec3_Subv(pSegWorld, closestWorld);
	vec3 normal=Vec3_Muls(delta, 1.0f/dist);

	vec3 contact=Vec3_Subv(pSegWorld, Vec3_Muls(normal, capsule->radius-penetration*0.5f));

	CollisionManifold_t manifold;
	manifold.a=obb;
	manifold.b=capsule;
	manifold.contacts[0].position=contact;
	manifold.contacts[0].normal=normal;
	manifold.contacts[0].penetration=penetration;
	manifold.contactCount=1;

	return manifold;
}

CollisionManifold_t PhysicsCollision(RigidBody_t *a, RigidBody_t *b)
{
	if(a->type==RIGIDBODY_SPHERE&&b->type==RIGIDBODY_SPHERE)
		return SphereToSphereCollision(a, b);
	else if(a->type==RIGIDBODY_SPHERE&&b->type==RIGIDBODY_OBB)
		return SphereToOBBCollision(a, b);
	else if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_SPHERE)
		return SphereToOBBCollision(b, a);
	else if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_OBB)
		return OBBToOBBCollision(a, b);
	else if(a->type==RIGIDBODY_CAPSULE&&b->type==RIGIDBODY_SPHERE)
		return CapsuleToSphereCollision(a, b);
	else if(a->type==RIGIDBODY_SPHERE&&b->type==RIGIDBODY_CAPSULE)
		return CapsuleToSphereCollision(b, a);
	else if(a->type==RIGIDBODY_CAPSULE&&b->type==RIGIDBODY_CAPSULE)
		return CapsuleToCapsuleCollision(a, b);
	else if(a->type==RIGIDBODY_CAPSULE&&b->type==RIGIDBODY_OBB)
		return CapsuleToOBBCollision(a, b);
	else if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_CAPSULE)
		return CapsuleToOBBCollision(b, a);

	return (CollisionManifold_t) { 0 };
}

void SpringIntegrate(Spring_t *s, vec3 target, float dt)
{
	vec3 displacement=Vec3_Subv(s->position, target);
	const float length=Vec3_Normalize(&displacement);

	const float stretch=length-s->length;
	const vec3 force=Vec3_Muls(displacement, -s->stiffness*stretch);
	const vec3 dampingForce=Vec3_Muls(s->velocity, -s->damping);

	vec3 acceleration=Vec3_Muls(Vec3_Addv(force, dampingForce), s->invMass);

	s->velocity=Vec3_Addv(s->velocity, Vec3_Muls(acceleration, dt));
	s->position=Vec3_Addv(s->position, Vec3_Muls(s->velocity, dt));
}
