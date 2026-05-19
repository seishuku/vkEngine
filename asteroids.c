#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "physics/physics.h"
#include "assetmanager.h"
#include "asteroids.h"

uint32_t numAsteroids=1000;
RigidBody_t asteroids[MAX_ASTEROIDS];

AsteroidModel_t asteroidModels[MAX_ASTEROIDS];

void ResetAsteroids(void)
{
	// Set up rigid body reps for asteroids
	const float asteroidFieldMinRadius=100.0f;
	const float asteroidFieldMaxRadius=2000.0f;
	const float asteroidMinRadius=0.05f;
	const float asteroidMaxRadius=40.0f;

	uint32_t i=0, tries=0;

	memset(asteroids, 0, sizeof(RigidBody_t)*numAsteroids);

	// Randomly place asteroids in a sphere without any overlapping.
	while(i<numAsteroids)
	{
		vec3 randomDirection=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(&randomDirection);

		RigidBody_t asteroid={ 0 };

		asteroid.position=Vec3(
			randomDirection.x*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius),
			randomDirection.y*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius),
			randomDirection.z*RandFloatRange(asteroidFieldMinRadius, asteroidFieldMaxRadius)
		);
		asteroid.radius=RandFloatRange(asteroidMinRadius, asteroidMaxRadius);

		bool overlapping=false;

		for(uint32_t j=0;j<i;j++)
		{
			if(Vec3_Distance(asteroid.position, asteroids[j].position)<asteroid.radius+asteroids[j].radius)
				overlapping=true;
		}

		if(!overlapping)
			asteroids[i++]=asteroid;

		tries++;

		if(tries>numAsteroids*numAsteroids)
			break;
	}
	//////

	// Set up asteroids rigid body
	for(uint32_t i=0;i<numAsteroids;i++)
	{
		vec3 randomDirection=Vec3(
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f)
		);
		Vec3_Normalize(&randomDirection);

		asteroids[i].velocity=Vec3_Muls(randomDirection, RandFloat());
		asteroids[i].force=Vec3b(0.0f);

		asteroids[i].orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		asteroids[i].angularVelocity=Vec3_Muls(randomDirection, RandFloat());

		asteroids[i].mass=(1.0f/3000.0f)*(1.33333333f*PI*asteroids[i].radius);
		asteroids[i].invMass=1.0f/asteroids[i].mass;

		asteroids[i].inertia=0.4f*asteroids[i].mass*(asteroids[i].radius*asteroids[i].radius);
		asteroids[i].invInertia=1.0f/asteroids[i].inertia;

		asteroids[i].restitution=0.8f;
		asteroids[i].friction=0.5f;

		asteroids[i].type=RIGIDBODY_SPHERE;

		const uint32_t randAsteroid=RandRange(0, 3);
		asteroidModels[i].modelID=MODEL_ASTEROID1+randAsteroid;
		asteroidModels[i].tex0ID=TEXTURE_ASTEROID1+(2*randAsteroid+0);
		asteroidModels[i].tex1ID=TEXTURE_ASTEROID1+(2*randAsteroid+1);
	}
	//////
}

void AddAsteroid(vec3 position, vec3 velocity, float radius, uint32_t variant)
{
	if(numAsteroids>=MAX_ASTEROIDS)
		return;
 
	vec3 randomDirection=Vec3(
		RandFloatRange(-1.0f, 1.0f),
		RandFloatRange(-1.0f, 1.0f),
		RandFloatRange(-1.0f, 1.0f)
	);
	Vec3_Normalize(&randomDirection);
 
	const float mass=(1.0f/3000.0f)*(1.33333333f*PI*radius);
 
	RigidBody_t asteroid={
		.position=position,
 
		.velocity=velocity,
		.force=Vec3b(0.0f),
 
		.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f),
		.angularVelocity=Vec3_Muls(randomDirection, RandFloat()),
 
		.mass=mass,
		.inertia=0.4f*mass*(radius*radius),
 
		.restitution=0.8f,
		.friction=0.5f,
 
		.type=RIGIDBODY_SPHERE,
		.radius=radius,
	};
 
	asteroid.invMass=1.0f/asteroid.mass;
	asteroid.invInertia=1.0f/asteroid.inertia;
 
	variant=min(3, max(0, variant));
	asteroidModels[numAsteroids].modelID=MODEL_ASTEROID1+variant;
	asteroidModels[numAsteroids].tex0ID=TEXTURE_ASTEROID1+(2*variant+0);
	asteroidModels[numAsteroids].tex1ID=TEXTURE_ASTEROID1+(2*variant+1);
 
	asteroids[numAsteroids]=asteroid;
	numAsteroids++;
}

#define MIN_SPLIT_RADIUS 2.0f

void SplitAsteroid(uint32_t index, ContactPoint_t contact, float impactSpeed)
{
	if(index>=numAsteroids)
		return;
 
	// Save what we need from the host before removing it.
	const float hostRadius=asteroids[index].radius;
	const vec3  hostVelocity=asteroids[index].velocity;
 
	// Remove the rigid body, but keep the model and textures
	numAsteroids--;
	asteroids[index]=asteroids[numAsteroids];
 
	// Asteroid too small
	if(hostRadius*0.5f<MIN_SPLIT_RADIUS)
		return;
 
	const uint32_t numFragments=RandRange(2, 40);
 
	const float volumeTransfer=RandFloatRange(0.15f, 0.35f);
 
	float weights[40];
	float totalWeight=0.0f;
	for(uint32_t i=0;i<numFragments;i++)
	{
		const float r=RandFloat();
		weights[i]=r*r;
		totalWeight+=weights[i];
	}
 
	const float spread=1.5f/(1.0f+impactSpeed*0.1f);
 
	for(uint32_t i=0;i<numFragments;i++)
	{
		const float volumeFraction=(weights[i]/totalWeight)*volumeTransfer;
		const float fragmentRadius=cbrtf(volumeFraction)*hostRadius;
 
		if(fragmentRadius<MIN_SPLIT_RADIUS)
			continue;
 
		vec3 randVec=Vec3(
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f),
			RandFloatRange(-1.0f, 1.0f)
		);
		vec3 perp=Vec3_Subv(randVec, Vec3_Muls(contact.normal, Vec3_Dot(randVec, contact.normal)));
		Vec3_Normalize(&perp);
 
		vec3 ejectDir=Vec3_Addv(contact.normal, Vec3_Muls(perp, spread));
		Vec3_Normalize(&ejectDir);
 
		// v ∝ 1/r: smaller fragments travel faster for the same impulse.
		const float ejectionSpeed=impactSpeed*(hostRadius/fragmentRadius);
		vec3 spawnVel=Vec3_Addv(hostVelocity, Vec3_Muls(ejectDir, ejectionSpeed));
		vec3 spawnPos=Vec3_Addv(contact.position, Vec3_Muls(ejectDir, fragmentRadius*1.0f));
 
		// Spawn new asteroids, keeping parent model variant
		AddAsteroid(spawnPos, spawnVel, fragmentRadius, asteroidModels[index].modelID-MODEL_ASTEROID1);
 
		vec3 tumbleAxis=Vec3_Cross(ejectDir, contact.normal);
		Vec3_Normalize(&tumbleAxis);
		const float tumbleRate=impactSpeed*(hostRadius*hostRadius/(fragmentRadius*fragmentRadius));
		asteroids[numAsteroids-1].angularVelocity=Vec3_Muls(tumbleAxis, tumbleRate*0.05f);
	}
}
