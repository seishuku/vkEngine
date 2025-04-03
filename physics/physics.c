#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include "physics.h"

static void ApplyConstraints(RigidBody_t *body)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	body->velocity=Vec3_Clamp(body->velocity, -maxVelocity, maxVelocity);

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 normal=Vec3_Subv(body->position, center);

	float distanceSq=Vec3_LengthSq(normal);
	const float radiiSum=(maxRadius*maxRadius)-(body->radius*body->radius);

	if(distanceSq>radiiSum)
	{
		const float distance=Vec3_Normalize(&normal);
		body->force=Vec3_Addv(body->force, Vec3_Muls(normal, -distance));
	}

	// Apply linear velocity damping
	const float linearDamping=0.999f;
	body->velocity=Vec3_Muls(body->velocity, linearDamping);

	// Apply angular velocity damping
	const float angularDamping=0.998f;
	body->angularVelocity=Vec3_Muls(body->angularVelocity, angularDamping);
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
	//const vec3 gravity=Vec3(0.0f, 9.81f*WORLD_SCALE, 0.0f);
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

	ApplyConstraints(body);
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

static float ResolveCollision(RigidBody_t *a, RigidBody_t *b, const vec3 contact, const vec3 normal, const float penetration)
{
	// Pre-calculate inverse orientation quats
	const vec4 invOrientationA=QuatInverse(a->orientation);
	const vec4 invOrientationB=QuatInverse(b->orientation);

	// Torque arms
	const vec3 r1=Vec3_Subv(contact, a->position);
	const vec3 r2=Vec3_Subv(contact, b->position);

	const vec3 relativeVel=Vec3_Subv(
		Vec3_Addv(b->velocity, Vec3_Cross(b->angularVelocity, r2)),
		Vec3_Addv(a->velocity, Vec3_Cross(a->angularVelocity, r1))
	);

	const float relativeSpeed=Vec3_Dot(relativeVel, normal);

	if(relativeSpeed>0.0f)
		return 0.0f;

	// Masses
	const vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, normal), a->invInertia), r1);
	const vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, normal), b->invInertia), r2);
	const float invMassSum=a->invMass+b->invMass;

	const float e=0.8f;
	const float j=-(1.0f+e)*relativeSpeed/(invMassSum+Vec3_Dot(normal, Vec3_Addv(d1, d2)));

	const vec3 impulse=Vec3_Muls(normal, j);

	// Head-on collision velocities

	// Linear velocity
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulse, a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulse, b->invMass));

	// Transform torque to local space
	vec3 localTorqueA=QuatRotate(invOrientationA, Vec3_Cross(r1, impulse));
	vec3 localTorqueB=QuatRotate(invOrientationB, Vec3_Cross(r2, impulse));

	// Angular velocity
	a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(localTorqueA, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(localTorqueB, b->invInertia));

	// Calculate tangential velocities
	vec3 tangentialVel=Vec3_Subv(relativeVel, Vec3_Muls(normal, Vec3_Dot(relativeVel, normal)));
	Vec3_Normalize(&tangentialVel);

	const vec3 d1T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, tangentialVel), a->invInertia), r1);
	const vec3 d2T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, tangentialVel), b->invInertia), r2);

	const float friction=sqrtf(0.5f*0.5f);
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

	const vec3 correctionVector=Vec3_Muls(normal, penetration/invMassSum*1.0f);

	a->position=Vec3_Subv(a->position, Vec3_Muls(correctionVector, a->invMass));
	b->position=Vec3_Addv(b->position, Vec3_Muls(correctionVector, b->invMass));

	return sqrtf(-relativeSpeed);
}

static float SphereToSphereCollision(RigidBody_t *a, RigidBody_t *b)
{
	const vec3 relativePosition=Vec3_Subv(b->position, a->position);
	const float distanceSq=Vec3_LengthSq(relativePosition);
	const float radiiSum=a->radius+b->radius;

	if(distanceSq<FLT_EPSILON||distanceSq>radiiSum*radiiSum)
	{
		// No collision
		return 0.0f;
	}

	// Penetration
	const float distance=sqrtf(distanceSq);
	const float penetration=fabsf(distance-radiiSum)*0.5f;

	// Normal
	const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

	// Contact point
	const vec3 contact=Vec3_Addv(a->position, Vec3_Muls(normal, a->radius-penetration));

	return ResolveCollision(a, b, contact, normal, penetration);
}

static float SphereToOBBCollision(RigidBody_t *sphere, RigidBody_t *obb)
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

	if(distanceSq<FLT_EPSILON||distanceSq>sphere->radius*sphere->radius)
	{
		// No collision
		return 0.0f;
	}

	// Penetration
	const float distance=sqrtf(distanceSq);
	const float penetration=sphere->radius-distance;

	// Normal
	const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

	// Contact point
	const vec3 contact=Vec3_Subv(closestPoint, Vec3_Muls(normal, penetration*0.5f));

	return ResolveCollision(obb, sphere, contact, normal, penetration);
}

static float OBBToOBBCollision(RigidBody_t *a, RigidBody_t *b)
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

	// Test axes
	for(uint32_t i=0;i<axisCount;i++)
	{
		// Project OBBs onto axis
		const float rA=fabsf(Vec3_Dot(axesA[0], axes[i]))*a->size.x+fabsf(Vec3_Dot(axesA[1], axes[i]))*a->size.y+fabsf(Vec3_Dot(axesA[2], axes[i]))*a->size.z;
		const float rB=fabsf(Vec3_Dot(axesB[0], axes[i]))*b->size.x+fabsf(Vec3_Dot(axesB[1], axes[i]))*b->size.y+fabsf(Vec3_Dot(axesB[2], axes[i]))*b->size.z;
		const float distance=fabsf(Vec3_Dot(relativePosition, axes[i]));

		const float overlap=rA+rB-distance;

		if(overlap<0.0f)
		{
			// Separating axis found, no collision
			return 0.0f;
		}
		else if(overlap<penetration)
		{
			// Update minimum penetration depth and collision normal
			penetration=overlap;
			normal=axes[i];
		}
	}

	// No separating axis found

	// Ensure the collision normal points from A to B
	if(Vec3_Dot(normal, relativePosition)<0.0f)
		normal=Vec3_Muls(normal, -1.0f);

	// Point of contact (TODO: some of this feels redundant)
	const vec3 pointA=Vec3_Addv(a->position, Vec3_Mulv(normal, a->size));
	const vec3 pointB=Vec3_Subv(b->position, Vec3_Mulv(normal, b->size));
	const vec3 contact=Vec3_Muls(Vec3_Addv(pointA, pointB), 0.5f);

	return ResolveCollision(a, b, contact, normal, penetration);
}

float PhysicsCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	if(a->type==RIGIDBODY_SPHERE&&b->type==RIGIDBODY_SPHERE)
		return SphereToSphereCollision(a, b);
	else if(a->type==RIGIDBODY_SPHERE&&b->type==RIGIDBODY_OBB)
		return SphereToOBBCollision(a, b);
	else if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_SPHERE)
		return SphereToOBBCollision(b, a);
	else if(a->type==RIGIDBODY_OBB&&b->type==RIGIDBODY_OBB)
		return OBBToOBBCollision(b, a);

	return 0.0f;
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
