#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"

// Define constants
#define WORLD_SCALE 1000.0f
#define EXPLOSION_POWER (50.0f*WORLD_SCALE)

typedef struct RigidBody_s
{
	vec3 position;

	vec3 velocity;
	vec3 force;
	float mass, invMass;

	vec4 orientation;
	vec3 angularVelocity;
	float inertia, invInertia;

	float radius;	// radius if it's a sphere
	vec3 size;		// bounding box if it's an AABB
} RigidBody_t;

void PhysicsIntegrate(RigidBody_t *body, const float dt);
void PhysicsExplode(RigidBody_t *body);
float PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b);
float PhysicsSphereToAABBCollisionResponse(RigidBody_t *sphere, RigidBody_t *aabb);

#endif
