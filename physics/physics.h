#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"
#include "../camera/camera.h"
#include "../particle/particle.h"

// Define constants
#define WORLD_SCALE 1000.0f
#define EXPLOSION_POWER (50.0f*WORLD_SCALE)

typedef struct
{
	vec3 Position;
	vec3 Velocity;
	vec3 Force;
	float Mass, invMass;

	float Radius;	// radius if it's a sphere
	vec3 Size;		// bounding box if it's an AABB
} RigidBody_t;

void PhysicsIntegrate(RigidBody_t *body, float dt);
void PhysicsExplode(RigidBody_t *bodies);
void PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b);
void PhysicsCameraToSphereCollisionResponse(Camera_t *Camera, RigidBody_t *Body);
void PhysicsCameraToCameraCollisionResponse(Camera_t *CameraA, Camera_t *CameraB);
void PhysicsParticleToSphereCollisionResponse(Particle_t *Particle, RigidBody_t *Body);

#endif
