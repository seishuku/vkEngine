#include <stdint.h>
#include <stdlib.h>
#include "physics.h"

// For sound playback on collision
#include "../audio/audio.h"
#include "../sounds.h"

static void apply_constraints(RigidBody_t *Body)
{
	vec3 Center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	Body->Velocity.x=min(maxVelocity, max(-maxVelocity, Body->Velocity.x));
	Body->Velocity.y=min(maxVelocity, max(-maxVelocity, Body->Velocity.y));
	Body->Velocity.z=min(maxVelocity, max(-maxVelocity, Body->Velocity.z));

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 Normal;
	Vec3_Setv(&Normal, Body->Position);
	Vec3_Subv(&Normal, Center);

	float distanceSq=Vec3_Dot(Normal, Normal);

	if(distanceSq>maxRadius*maxRadius)
	{
		float Distance=rsqrtf(distanceSq);
		// Normalize the normal
		Vec3_Muls(&Normal, Distance);

		// Simple velocity reflection to bounce off the "wall"
		Vec3_Reflect(Normal, Body->Velocity, &Body->Velocity);
	}
}

void PhysicsIntegrate(RigidBody_t *body, float dt)
{
	const float damping=0.01f;

	// Clamp delta time, if it's longer than 16MS, clamp it to that.
	//   This reduces the chance of the simulation going unstable.
	if(dt>0.016f)
		dt=0.016f;

	// Apply damping force
	// Force+=Velocity*-damping
	vec3 damping_force;
	Vec3_Setv(&damping_force, body->Velocity);
	Vec3_Muls(&damping_force, -damping);
	Vec3_Addv(&body->Force, damping_force);

	// Euler integration of position and velocity
	// Position+=Velocity*dt+0.5f*Force/Mass*dt*dt
	// Velocity+=Force/Mass*dt
	vec3 forceMass, velocityScaled;
	float massDeltaTimeSq=0.5f*body->invMass*dt*dt;
	Vec3_Setv(&forceMass, body->Force);
	Vec3_Muls(&forceMass, massDeltaTimeSq);
	Vec3_Setv(&velocityScaled, body->Velocity);
	Vec3_Muls(&velocityScaled, dt);
	Vec3_Addv(&velocityScaled, forceMass);
	Vec3_Addv(&body->Position, velocityScaled);

	Vec3_Setv(&forceMass, body->Force);
	Vec3_Muls(&forceMass, body->invMass*dt);
	Vec3_Addv(&body->Velocity, forceMass);

	Vec3_Sets(&body->Force, 0.0f);

	apply_constraints(body);
}

void PhysicsExplode(RigidBody_t *body)
{
	vec3 explosion_center={ 0.0f, 0.0f, 0.0f };

	// Calculate direction from explosion center to fragment
	vec3 direction;
	Vec3_Setv(&direction, body->Position);
	Vec3_Subv(&direction, explosion_center);
	Vec3_Normalize(&direction);

	// Calculate acceleration and impulse force
	vec3 acceleration;
	Vec3_Setv(&acceleration, direction);
	Vec3_Muls(&acceleration, EXPLOSION_POWER);

	// F=M*A bla bla...
	vec3 force;
	Vec3_Setv(&force, acceleration);
	Vec3_Muls(&force, body->Mass);

	// Add it into object's velocity
	Vec3_Addv(&body->Velocity, force);
}

void PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	static uint32_t SoundCooldown=0;

	// Calculate the distance between the camera and the sphere's center
	vec3 Normal;
	Vec3_Setv(&Normal, b->Position);
	Vec3_Subv(&Normal, a->Position);

	const float DistanceSq=Vec3_Dot(Normal, Normal);

	// Sum of radii
	const float radiusSum=a->Radius+b->Radius;

	// Check if the distance is less than the sum of the radii
	if(DistanceSq<radiusSum*radiusSum)
	{
		float distance=sqrtf(DistanceSq);

		Vec3_Muls(&Normal, 1.0f/distance);

		const float Penetration=radiusSum-distance;
		vec3 positionImpulse;
		Vec3_Setv(&positionImpulse, Normal);
		Vec3_Muls(&positionImpulse, Penetration*0.5f);

		Vec3_Subv(&a->Position, positionImpulse);
		Vec3_Addv(&b->Position, positionImpulse);

		vec3 contactVelocity;
		Vec3_Setv(&contactVelocity, b->Velocity);
		Vec3_Subv(&contactVelocity, a->Velocity);

		const float totalMass=a->invMass+b->invMass;
		const float Restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, Normal);
		float j=(-(1.0f+Restitution)*VdotN)/totalMass;

		vec3 aVelocityImpulse;
		Vec3_Setv(&aVelocityImpulse, Normal);
		Vec3_Muls(&aVelocityImpulse, j*a->invMass);
		Vec3_Subv(&a->Velocity, aVelocityImpulse);

		vec3 bVelocityImpulse;
		Vec3_Setv(&bVelocityImpulse, Normal);
		Vec3_Muls(&bVelocityImpulse, j*b->invMass);
		Vec3_Addv(&b->Velocity, bVelocityImpulse);

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
			Audio_PlaySample(&Sounds[RandRange(SOUND_STONE1, SOUND_STONE3)], false, relVelMag/200.0f, &a->Position);
		}
}

// Camera<->Rigid body collision detection and response
void PhysicsCameraToSphereCollisionResponse(Camera_t *Camera, RigidBody_t *Body)
{
	// Camera mass constants, since camera struct doesn't store these
	const float Camera_Mass=100.0f;
	const float Camera_invMass=1.0f/Camera_Mass;


	// Calculate the distance between the camera and the sphere's center
	vec3 Normal;
	Vec3_Setv(&Normal, Body->Position);
	Vec3_Subv(&Normal, Camera->Position);

	float DistanceSq=Vec3_Dot(Normal, Normal);

	// Sum of radii
	float radiusSum=Camera->Radius+Body->Radius;

	// Check if the distance is less than the sum of the radii
	if(DistanceSq<=radiusSum*radiusSum)
	{
		float distance=sqrtf(DistanceSq);

		Vec3_Muls(&Normal, 1.0f/distance);

		const float Penetration=radiusSum-distance;
		vec3 positionImpulse;
		Vec3_Setv(&positionImpulse, Normal);
		Vec3_Muls(&positionImpulse, Penetration*0.5f);

		Vec3_Subv(&Camera->Position, positionImpulse);
		Vec3_Addv(&Body->Position, positionImpulse);

		vec3 contactVelocity;
		Vec3_Setv(&contactVelocity, Body->Velocity);
		Vec3_Subv(&contactVelocity, Camera->Velocity);

		const float totalMass=Camera_invMass+Body->invMass;
		const float Restitution=0.66f;
		const float VdotN=Vec3_Dot(contactVelocity, Normal);
		float j=(-(1.0f+Restitution)*VdotN)/totalMass;

		vec3 aVelocityImpulse;
		Vec3_Setv(&aVelocityImpulse, Normal);
		Vec3_Muls(&aVelocityImpulse, j*Camera_invMass);
		Vec3_Subv(&Camera->Velocity, aVelocityImpulse);

		vec3 bVelocityImpulse;
		Vec3_Setv(&bVelocityImpulse, Normal);
		Vec3_Muls(&bVelocityImpulse, j*Body->invMass);
		Vec3_Addv(&Body->Velocity, bVelocityImpulse);

		const float relVelMag=sqrtf(fabsf(VdotN));

		if(relVelMag>1.0f)
			Audio_PlaySample(&Sounds[SOUND_CRASH], false, relVelMag/200.0f, &Camera->Position);
	}
}
