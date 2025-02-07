#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"

// Define constants
#define WORLD_SCALE 1000.0f
#define EXPLOSION_POWER (50.0f*WORLD_SCALE)

typedef enum
{
	RIGIDBODY_OBB=0,
	RIGIDBODY_SPHERE,
	MAX_RIGIDBODYTYPE
} RigidBodyType_e;

typedef struct RigidBody_s
{
	vec3 position;
	vec3 velocity;
	vec3 force;
	float mass, invMass;

	vec4 orientation;
	vec3 angularVelocity;
	float inertia, invInertia;

	RigidBodyType_e type;	// OBB or sphere
	union
	{
		float radius;
		vec3 size;				// OBB dimensions or radius
	};
} RigidBody_t;

void PhysicsIntegrate(RigidBody_t *body, const float dt);
void PhysicsExplode(RigidBody_t *body);
float PhysicsCollisionResponse(RigidBody_t *a, RigidBody_t *b);

#endif
