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
	vec3 position;
	vec3 velocity;
	vec3 force;

	vec4 orientation;
	vec3 angularVelocity;
	float inertia, invInertia;

	float mass, invMass;

	float radius;	// radius if it's a sphere
	vec3 size;		// bounding box if it's an AABB
} RigidBody_t;

typedef enum
{
	PHYSICS_OBJECT_SPHERE=0,
	PHYSICS_OBJECT_AABB,
	PHYSICS_OBJECT_CAMERA,
	PHYSICS_OBJECT_PARTICLE,
	NUM_PHYSICS_OBJECT_TYPE,
} PhysicsObjectType;

void PhysicsIntegrate(RigidBody_t *body, float dt);
void PhysicsExplode(RigidBody_t *bodies);
void PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b);
void PhysicsCameraToSphereCollisionResponse(Camera_t *camera, RigidBody_t *body);
void PhysicsCameraToCameraCollisionResponse(Camera_t *cameraA, Camera_t *cameraB);
void PhysicsParticleToSphereCollisionResponse(Particle_t *particle, RigidBody_t *body);

#endif
