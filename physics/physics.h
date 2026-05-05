#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"

// Define constants
#define WORLD_SCALE 80.0f
#define EXPLOSION_POWER (50.0f*WORLD_SCALE)

typedef enum
{
	RIGIDBODY_OBB=0,
	RIGIDBODY_SPHERE,
	RIGIDBODY_CAPSULE,
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

	RigidBodyType_e type;	// OBB, sphere, capsule
	union
	{
		float radius;
		vec3 size;				// OBB dimensions or radius
	};
} RigidBody_t;

typedef struct
{
	vec3 position, normal;
	float penetration;
} ContactPoint_t;

#define MAX_CONTACTS_PER_MANIFOLD 8

typedef struct
{
	RigidBody_t *a, *b;
	ContactPoint_t contacts[MAX_CONTACTS_PER_MANIFOLD];
	uint32_t contactCount;
} CollisionManifold_t;

void PhysicsIntegrate(RigidBody_t *body, const float dt);
void PhysicsExplode(RigidBody_t *body);
void PhysicsApplyImpulse(RigidBody_t *body, const vec3 impulse, const vec3 point);
float PhysicsResolveCollision(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact);
void PhysicsPositionCorrection(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact);
CollisionManifold_t PhysicsCollision(RigidBody_t *a, RigidBody_t *b);

typedef struct
{
	vec3 position;
	vec3 velocity;
	float stiffness;
	float damping;
	float length;
	float mass, invMass;
} Spring_t;

void SpringIntegrate(Spring_t *spring, vec3 target, float dt);

#endif
