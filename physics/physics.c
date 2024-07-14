#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "physics.h"

static void applyConstraints(RigidBody_t *body)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	body->velocity=Vec3_Clamp(body->velocity, -maxVelocity, maxVelocity);

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 normal=Vec3_Subv(body->position, center);

	float distanceSq=Vec3_Dot(normal, normal);
	const float radiiSum=(maxRadius*maxRadius)-(body->radius*body->radius);

	if(distanceSq>radiiSum)
	{
		const float distance=Vec3_Normalize(&normal);
		body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(normal, -distance));
	}

	// Apply linear velocity damping
	//const float linearDamping=0.998f;
	//body->velocity=Vec3_Muls(body->velocity, linearDamping);

	// Apply angular velocity damping
	const float angularDamping=0.998f;
	body->angularVelocity=Vec3_Muls(body->angularVelocity, angularDamping);
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
	body->orientation=integrateAngularVelocity(body->orientation, body->angularVelocity, dt);

	applyConstraints(body);
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

float PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	const vec3 relativePosition=Vec3_Subv(b->position, a->position);
	const float distanceSq=Vec3_Dot(relativePosition, relativePosition);
	const float radiiSum=a->radius+b->radius;

	if(distanceSq<radiiSum*radiiSum)
	{
		const float distance=sqrtf(distanceSq);
		const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

		// Point of contact
		const float penetration=fabsf(distance-radiiSum)*0.5f;
		const vec3 contact=Vec3_Addv(a->position, Vec3_Muls(normal, a->radius-penetration));

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
		a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulse, a->invMass));
		b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulse, b->invMass));

		a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impulse), a->invInertia));
		b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impulse), b->invInertia));

		// Calculate tangential velocities
		vec3 tangentialVel=Vec3_Subv(relativeVel, Vec3_Muls(normal, Vec3_Dot(relativeVel, normal)));
		Vec3_Normalize(&tangentialVel);

		const vec3 d1T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, tangentialVel), a->invInertia), r1);
		const vec3 d2T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, tangentialVel), b->invInertia), r2);

		const float friction=sqrtf(0.5f*0.5f);
		const float maxjT=friction*j;

		float jT=-Vec3_Dot(relativeVel, tangentialVel)/(invMassSum+Vec3_Dot(tangentialVel, Vec3_Addv(d1T, d2T)));

		if(jT>maxjT)
			jT=maxjT;
		else if(jT<-maxjT)
			jT=-maxjT;

		const vec3 impulseT=Vec3_Muls(tangentialVel, jT);

		a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulseT, a->invMass));
		b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulseT, b->invMass));

		a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impulseT), a->invInertia));
		b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impulseT), b->invInertia));

		const vec3 correctionVector=Vec3_Muls(normal, penetration/invMassSum*1.0f);

		a->position=Vec3_Subv(a->position, Vec3_Muls(correctionVector, a->invMass));
		b->position=Vec3_Addv(b->position, Vec3_Muls(correctionVector, b->invMass));

		return sqrtf(-relativeSpeed);
	}

	return 0.0f;
}

float PhysicsSphereToAABBCollisionResponse(RigidBody_t *sphere, RigidBody_t *aabb)
{
	// Calculate the half extents of the AABB
	const vec3 half=Vec3_Muls(aabb->size, 0.5f);

	// Calculate the AABB's min and max points
	const vec3 aabbMin=Vec3_Subv(aabb->position, half);
	const vec3 aabbMax=Vec3_Addv(aabb->position, half);

	// Find the closest point on the AABB to the sphere
	const vec3 closest=Vec3_Clampv(sphere->position, aabbMin, aabbMax);

	// Calculate the distance between the closest point and the sphere's center
	vec3 relativePosition=Vec3_Subv(closest, sphere->position);

	const float distanceSq=Vec3_Dot(relativePosition, relativePosition);

	// Check if the distance is less than the sphere's radius
	if(distanceSq<=sphere->radius*sphere->radius&&distanceSq)
	{
		const float distance=sqrtf(distanceSq);
		const vec3 normal=Vec3_Muls(relativePosition, 1.0f/distance);

		// Point of contact
		const float penetration=fabsf(distance-sphere->radius)*0.5f;
		const vec3 contact=Vec3_Addv(sphere->position, Vec3_Muls(normal, penetration));

		// Torque arms
		const vec3 r1=Vec3_Subv(contact, sphere->position);
		const vec3 r2=Vec3_Subv(contact, aabb->position);

		const vec3 relativeVel=Vec3_Subv(
			Vec3_Addv(aabb->velocity, Vec3_Cross(aabb->angularVelocity, r2)),
			Vec3_Addv(sphere->velocity, Vec3_Cross(sphere->angularVelocity, r1))
		);

		const float relativeSpeed=Vec3_Dot(relativeVel, normal);

		if(relativeSpeed>0.0f)
			return 0.0f;

		// Masses
		const vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, normal), sphere->invInertia), r1);
		const vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, normal), aabb->invInertia), r2);
		const float invMassSum=sphere->invMass+aabb->invMass;

		const float e=0.8f;
		const float j=-(1.0f+e)*relativeSpeed/(invMassSum+Vec3_Dot(normal, Vec3_Addv(d1, d2)));

		const vec3 impulse=Vec3_Muls(normal, j);

		// Head-on collision velocities
		sphere->velocity=Vec3_Subv(sphere->velocity, Vec3_Muls(impulse, sphere->invMass));
		aabb->velocity=Vec3_Addv(aabb->velocity, Vec3_Muls(impulse, aabb->invMass));

		sphere->angularVelocity=Vec3_Subv(sphere->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impulse), sphere->invInertia));
		aabb->angularVelocity=Vec3_Addv(aabb->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impulse), aabb->invInertia));

		// Calculate tangential velocities
		vec3 tangentialVel=Vec3_Subv(relativeVel, Vec3_Muls(normal, Vec3_Dot(relativeVel, normal)));
		Vec3_Normalize(&tangentialVel);

		const vec3 d1T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, tangentialVel), sphere->invInertia), r1);
		const vec3 d2T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, tangentialVel), aabb->invInertia), r2);

		const float friction=sqrtf(0.5f*0.5f);
		const float maxjT=friction*j;

		float jT=-Vec3_Dot(relativeVel, tangentialVel)/(invMassSum+Vec3_Dot(tangentialVel, Vec3_Addv(d1T, d2T)));

		if(jT>maxjT)
			jT=maxjT;
		else if(jT<-maxjT)
			jT=-maxjT;

		const vec3 impulseT=Vec3_Muls(tangentialVel, jT);

		sphere->velocity=Vec3_Subv(sphere->velocity, Vec3_Muls(impulseT, sphere->invMass));
		aabb->velocity=Vec3_Addv(aabb->velocity, Vec3_Muls(impulseT, aabb->invMass));

		sphere->angularVelocity=Vec3_Subv(sphere->angularVelocity, Vec3_Muls(Vec3_Cross(r1, impulseT), sphere->invInertia));
		aabb->angularVelocity=Vec3_Addv(aabb->angularVelocity, Vec3_Muls(Vec3_Cross(r2, impulseT), aabb->invInertia));

		const vec3 correctionVector=Vec3_Muls(normal, penetration/invMassSum*1.0f);

		sphere->position=Vec3_Subv(sphere->position, Vec3_Muls(correctionVector, sphere->invMass));
		//aabb->position=Vec3_Addv(aabb->position, Vec3_Muls(correctionVector, aabb->invMass));

		return sqrtf(-relativeSpeed);
	}

	return 0.0f;
}
