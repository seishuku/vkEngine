#include <stdint.h>
#include <stdlib.h>
#include "physics.h"

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
	vec3 axis=body->angularVelocity;
	Vec3_Normalize(&axis);

	body->orientation=QuatMultiply(body->orientation, QuatAnglev(dt, axis));

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

void PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	// Calculate the distance between the two sphere's centers
	vec3 normal=Vec3_Subv(b->position, a->position);

	const float distanceSq=Vec3_Dot(normal, normal);

	// Sum of radii
	const float radiusSum=a->radius+b->radius;

	// Check if the distance is less than the sum of the radii
	if(distanceSq<radiusSum*radiusSum)
	{
		const float distance=sqrtf(distanceSq);

		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		const float penetration=radiusSum-distance;
		const vec3 positionImpulse=Vec3_Muls(normal, penetration*0.5f);

		a->position=Vec3_Subv(a->position, positionImpulse);
		b->position=Vec3_Addv(b->position, positionImpulse);

		const vec3 contactVelocity=Vec3_Subv(b->velocity, a->velocity);

		const float totalMass=a->invMass+b->invMass;
		const float restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, normal);
		const float j=(-(1.0f+restitution)*VdotN)/totalMass;

		a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(normal, j*a->invMass));
		b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(normal, j*b->invMass));

		a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(normal, VdotN*a->invInertia));
		b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(normal, VdotN*b->invInertia));

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
			Audio_PlaySample(&Sounds[RandRange(SOUND_STONE1, SOUND_STONE3)], false, relVelMag/50.0f, &a->position);
	}
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

// Camera<->Rigid body collision detection and response
void PhysicsCameraToSphereCollisionResponse(Camera_t *camera, RigidBody_t *body)
{
	// Camera mass constants, since camera struct doesn't store these
	const float cameraMass=100.0f;
	const float cameraInvMass=1.0f/cameraMass;

	// Calculate the distance between the camera and the sphere's center
	vec3 normal=Vec3_Subv(body->position, camera->Position);

	const float distanceSq=Vec3_Dot(normal, normal);

	// Sum of radii
	const float radiusSum=camera->Radius+body->radius;

	// Check if the distance is less than the sum of the radii
	if(distanceSq<=radiusSum*radiusSum&&distanceSq)
	{
		const float distance=sqrtf(distanceSq);

		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		const float penetration=radiusSum-distance;
		const vec3 positionImpulse=Vec3_Muls(normal, penetration*0.5f);

		camera->Position=Vec3_Subv(camera->Position, positionImpulse);
		body->position=Vec3_Addv(body->position, positionImpulse);

		const vec3 contactVelocity=Vec3_Subv(body->velocity, camera->Velocity);

		const float totalMass=cameraInvMass+body->invMass;
		const float restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, normal);
		const float j=(-(1.0f+restitution)*VdotN)/totalMass;

		camera->Velocity=Vec3_Subv(camera->Velocity, Vec3_Muls(normal, j*cameraInvMass));
		body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(normal, j*body->invMass));

		//Camera->AngularVelocity=Vec3_Subv(Camera->AngularVelocity, Vec3_Muls(Normal, j*Camera_invInertia));
		body->angularVelocity=Vec3_Addv(body->angularVelocity, Vec3_Muls(normal, VdotN*body->invInertia));

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
			Audio_PlaySample(&Sounds[SOUND_CRASH], false, relVelMag/50.0f, &body->position);
	}
}

void PhysicsCameraToCameraCollisionResponse(Camera_t *cameraA, Camera_t *cameraB)
{
	// Camera mass constants, since camera struct doesn't store these
	const float cameraMass=100.0f;
	const float cameraInvMass=1.0f/cameraMass;

	// Calculate the distance between the camera and the sphere's center
	vec3 normal=Vec3_Subv(cameraB->Position, cameraA->Position);

	const float distanceSq=Vec3_Dot(normal, normal);

	// Sum of radii
	const float radiusSum=cameraA->Radius+cameraB->Radius;

	// Check if the distance is less than the sum of the radii
	if(distanceSq<=radiusSum*radiusSum&&distanceSq)
	{
		const float distance=sqrtf(distanceSq);

		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		const float penetration=radiusSum-distance;
		const vec3 positionImpulse=Vec3_Muls(normal, penetration*0.5f);

		cameraA->Position=Vec3_Subv(cameraA->Position, positionImpulse);
		cameraB->Position=Vec3_Addv(cameraB->Position, positionImpulse);

		const vec3 contactVelocity=Vec3_Subv(cameraB->Velocity, cameraA->Velocity);

		const float totalMass=cameraInvMass+cameraInvMass;
		const float restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, normal);
		const float j=(-(1.0f+restitution)*VdotN)/totalMass;

		cameraA->Velocity=Vec3_Subv(cameraA->Velocity, Vec3_Muls(normal, j*cameraInvMass));
		cameraB->Velocity=Vec3_Addv(cameraB->Velocity, Vec3_Muls(normal, j*cameraInvMass));

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
			Audio_PlaySample(&Sounds[SOUND_CRASH], false, relVelMag/50.0f, &cameraB->Position);
	}
}

void ExplodeEmitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle)
{
	particle->position=Vec3b(0.0f);

	particle->velocity=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
	Vec3_Normalize(&particle->velocity);
	particle->velocity=Vec3_Muls(particle->velocity, RandFloat()*50.0f);

	particle->life=RandFloat()*0.5f+0.01f;
}

// Particle<->Rigid body collision detection and response
void PhysicsParticleToSphereCollisionResponse(Particle_t *particle, RigidBody_t *body)
{
	// Particle constants, since particle struct doesn't store these
	const float particleRadius=2.0f;
	const float particleMass=1.0f;
	const float particleInvMass=1.0f/particleMass;

	// Calculate the distance between the particle and the sphere's center
	vec3 normal=Vec3_Subv(body->position, particle->position);

	const float distanceSq=Vec3_Dot(normal, normal);

	// Sum of radii
	const float radiusSum=particleRadius+body->radius;

	// Check if the distance is less than the sum of the radii
	if(distanceSq<=radiusSum*radiusSum&&distanceSq)
	{
		const float distance=sqrtf(distanceSq);

		if(distance)
			normal=Vec3_Muls(normal, 1.0f/distance);

		const float penetration=radiusSum-distance;
		const vec3 positionImpulse=Vec3_Muls(normal, penetration*0.5f);

		particle->position=Vec3_Subv(particle->position, positionImpulse);
		body->position=Vec3_Addv(body->position, positionImpulse);

		const vec3 contactVelocity=Vec3_Subv(body->velocity, particle->velocity);

		const float totalMass=particleInvMass+body->invMass;
		const float restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, normal);
		const float j=(-(1.0f+restitution)*VdotN)/totalMass;

		particle->velocity=Vec3_Subv(particle->velocity, Vec3_Muls(normal, j*particleInvMass));
		body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(normal, j*body->invMass));

		//a->AngularVelocity=Vec3_Subv(a->AngularVelocity, Vec3_Muls(Normal, VdotN*a->invInertia));
		body->angularVelocity=Vec3_Addv(body->angularVelocity, Vec3_Muls(normal, VdotN*body->invInertia));

		particle->life=-1.0f;

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
		{
			Audio_PlaySample(&Sounds[RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3)], false, relVelMag/50.0f, &particle->position);

			// FIXME: Is this causing derelict emitters that never go away?
			//			I don't think it is, but need to check.
			ParticleSystem_ResetEmitter(
				&particleSystem,
				ParticleSystem_AddEmitter(
					&particleSystem,
					particle->position,				// Position
					Vec3(100.0f, 12.0f, 5.0f),	// Start color
					Vec3(0.0f, 0.0f, 0.0f),		// End color
					5.0f,						// Radius of particles
					1000,						// Number of particles in system
					true,						// Is burst?
					ExplodeEmitterCallback		// Callback for particle generation
				)
			);

			// Silly radius reduction on hit
			//body->radius=fmaxf(body->radius-10.0f, 0.0f);
		}
	}
}
