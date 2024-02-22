#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include "physics.h"
#include "../particle/particle.h"
#include "../camera/camera.h"

// For sound playback on collision
#include "../audio/audio.h"
#include "../sounds.h"

// Particle system
extern ParticleSystem_t particleSystem;

static void apply_constraints(RigidBody_t *body)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	body->velocity=Vec3_Clamp(body->velocity, -maxVelocity, maxVelocity);

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 normal=Vec3_Subv(body->position, center);

	float distanceSq=Vec3_Dot(normal, normal);

	if(distanceSq>maxRadius*maxRadius)
	{
		float distance=sqrtf(distanceSq);

		// Normalize the normal
		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		// Simple velocity reflection to bounce off the "wall"
		body->velocity=Vec3_Reflect(normal, body->velocity);
	}

	// Apply angular velocity damping
	//const float angularDamping=0.9999f;
	//body->angularVelocity=Vec3_Muls(body->angularVelocity, angularDamping);
}

vec4 integrateAngularVelocity(const vec4 q, const vec3 w, const float dt)
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

void PhysicsIntegrate(RigidBody_t *body, float dt)
{
	const float damping=0.0f;

	// Clamp delta time, if it's longer than 16MS, clamp it to that.
	//   This reduces the chance of the simulation going unstable.
	if(dt>0.016f)
		dt=0.016f;

	// Apply damping force
	// Force+=Velocity*-damping
	body->force=Vec3_Addv(body->force, Vec3_Muls(body->velocity, -damping));

	// Euler integration of position and velocity
	// Position+=Velocity*dt+0.5f*Force/Mass*dt*dt
	// Velocity+=Force/Mass*dt
	const float massDeltaTimeSq=0.5f*body->invMass*dt*dt;

	body->position=Vec3_Addv(body->position, Vec3_Addv(Vec3_Muls(body->velocity, dt), Vec3_Muls(body->force, massDeltaTimeSq)));

	body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(body->force, body->invMass*dt));

	body->force=Vec3b(0.0f);

	// Integrate angular velocity using quaternions
	body->orientation=integrateAngularVelocity(body->orientation, body->angularVelocity, dt);

	apply_constraints(body);
}

void PhysicsExplode(RigidBody_t *body)
{
	vec3 explosion_center={ 0.0f, 0.0f, 0.0f };

	// Calculate direction from explosion center to fragment
	vec3 direction=Vec3_Subv(body->position, explosion_center);
	Vec3_Normalize(&direction);

	// Calculate acceleration and impulse force
	vec3 acceleration=Vec3_Muls(direction, EXPLOSION_POWER);

	// F=M*A bla bla...
	vec3 force=Vec3_Muls(acceleration, body->mass);

	// Add it into object's velocity
	body->velocity=Vec3_Addv(body->velocity, force);
}

#if 0
float PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	// Calculate relative position
	vec3 relativePosition=Vec3_Subv(b->position, a->position);
	const float distanceSq=Vec3_Dot(relativePosition, relativePosition);
	const float radiiSum=a->radius+b->radius;

	// Check if spheres are not colliding
	if(distanceSq>radiiSum*radiiSum)
		return 0.0f;

	const float distance=sqrtf(distanceSq);
	const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

	// Calculate relative velocity
	const vec3 relativeVelocity=Vec3_Subv(b->velocity, a->velocity);

	// Calculate relative velocity along the normal
	const float relativeSpeed=Vec3_Dot(relativeVelocity, normal);

	// If spheres are moving away from each other
	if(relativeSpeed>0.0f)
		return 0.0f;

	// Calculate impulse
	float elasticity=0.8f;
	float impulse=-(1.0f+elasticity)*relativeSpeed/(a->invMass+b->invMass);

	// Update velocities
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(normal, impulse*a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(normal, impulse*b->invMass));
	
	// Update angular velocities (considering inertia)
	const vec3 r1=Vec3_Addv(a->position, Vec3_Muls(normal, -a->radius));
	const vec3 r2=Vec3_Addv(b->position, Vec3_Muls(normal, b->radius));

	const vec3 torque1=Vec3_Cross(r1, normal);
	const vec3 torque2=Vec3_Cross(r2, normal);

	a->angularVelocity=Vec3_Addv(a->angularVelocity, Vec3_Muls(torque1, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(torque2, b->invInertia));

	return sqrtf(fabs(relativeSpeed));
}
#endif

float PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	const vec3 relativePosition=Vec3_Subv(b->position, a->position);
	const float distanceSq=Vec3_Dot(relativePosition, relativePosition);
	const float radiiSum=a->radius+b->radius;

	if(distanceSq<radiiSum*radiiSum)
	{
		const float distance=sqrtf(distanceSq);
		const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

		const vec3 contact=Vec3_Addv(a->position, Vec3_Muls(normal, a->radius-(fabsf(Vec3_Length(normal)-radiiSum)*0.5f)));

		const vec3 r1=Vec3_Subv(contact, a->position);
		const vec3 r2=Vec3_Subv(contact, b->position);

		const vec3 relativeVel=Vec3_Subv(
			Vec3_Addv(b->velocity, Vec3_Cross(b->angularVelocity, r2)),
			Vec3_Addv(a->velocity, Vec3_Cross(a->angularVelocity, r1))
		);

		const float relativeSpeed=Vec3_Dot(relativeVel, normal);

		if(relativeSpeed>0.0f)
			return 0.0f;

		vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, normal), a->invInertia), r1);
		vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, normal), b->invInertia), r2);
		const float invMassSum=a->invMass+b->invMass;
		float denominator=invMassSum+Vec3_Dot(normal, Vec3_Addv(d1, d2));

		const float e=0.8f;
		const float j=-(1.0f+e)*relativeSpeed/denominator;

		const vec3 impulse=Vec3_Muls(normal, j);

		a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulse, a->invMass));
		b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulse, b->invMass));

		a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impulse), a->invInertia));
		b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impulse), b->invInertia));

		// Friction
		vec3 t=Vec3_Subv(relativeVel, Vec3_Muls(normal, Vec3_Dot(relativeVel, normal)));

		Vec3_Normalize(&t);

		d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, t), a->invInertia), r1);
		d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, t), b->invInertia), r2);
		denominator=invMassSum+Vec3_Dot(t, Vec3_Addv(d1, d2));

		float jT=-Vec3_Dot(relativeVel, t)/denominator;

		const float friction=sqrtf(0.5f*0.5f);
		const float maxjT=friction*j;

		if(jT>maxjT)
			jT=maxjT;
		else if(jT<-maxjT)
			jT=-maxjT;

		vec3 impuseT=Vec3_Muls(t, jT);

		a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impuseT, a->invMass));
		b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impuseT, b->invMass));

		a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impuseT), a->invInertia));
		b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impuseT), b->invInertia));

		return sqrtf(-relativeSpeed);
	}

	return 0.0f;
}

void PhysicsSphereToAABBCollisionResponse(RigidBody_t *sphere, RigidBody_t *aabb)
{
	// Calculate the half extents of the AABB
	const vec3 half=Vec3_Muls(aabb->size, 0.5f);

	// Calculate the AABB's min and max points
	const vec3 aabbMin=Vec3_Subv(aabb->position, half);
	const vec3 aabbMax=Vec3_Addv(aabb->position, half);

	// Find the closest point on the AABB to the sphere
	const vec3 closest=Vec3(
		fmaxf(aabbMin.x, fminf(sphere->position.x, aabbMax.x)),
		fmaxf(aabbMin.y, fminf(sphere->position.y, aabbMax.y)),
		fmaxf(aabbMin.z, fminf(sphere->position.z, aabbMax.z))
	);

	// Calculate the distance between the closest point and the sphere's center
	vec3 normal=Vec3_Subv(closest, sphere->position);

	const float distanceSq=Vec3_Dot(normal, normal);

	// Check if the distance is less than the sphere's radius
	if(distanceSq<=sphere->radius*sphere->radius&&distanceSq)
	{
		const float distance=sqrtf(distanceSq);

		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		const float penetration=sphere->radius-distance;
		const vec3 positionImpulse=Vec3_Muls(normal, penetration*0.5f);

		sphere->position=Vec3_Subv(sphere->position, positionImpulse);
		aabb->position=Vec3_Addv(aabb->position, positionImpulse);

		const vec3 contactVelocity=Vec3_Subv(aabb->velocity, sphere->velocity);

		const float totalMass=sphere->invMass+aabb->invMass;
		const float restitution=0.6f;
		const float VdotN=Vec3_Dot(contactVelocity, normal);
		const float j=(-(1.0f+restitution)*VdotN)/totalMass;

		sphere->velocity=Vec3_Subv(sphere->velocity, Vec3_Muls(normal, j*sphere->invMass));
		aabb->velocity=Vec3_Addv(aabb->velocity, Vec3_Muls(normal, j*aabb->invMass));

		sphere->angularVelocity=Vec3_Subv(sphere->angularVelocity, Vec3_Muls(normal, VdotN*sphere->invInertia));
		aabb->angularVelocity=Vec3_Addv(aabb->angularVelocity, Vec3_Muls(normal, VdotN*aabb->invInertia));
	}
}
