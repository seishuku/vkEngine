#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"

// Define constants
#define WORLD_SCALE 1000.0f
#define EXPLOSION_POWER (100.0f*WORLD_SCALE)

typedef struct
{
	vec3 Position;
	vec3 Velocity;
	vec3 Force;
	float Mass, invMass;

	float Radius;	// radius if it's a sphere
	vec3 Size;		// bounding box if it's an AABB
} RigidBody_t;

void integrate(RigidBody_t *body, float dt);
void explode(RigidBody_t *bodies);
void sphere_sphere_collision(RigidBody_t *a, RigidBody_t *b);
void sphere_aabb_collision(RigidBody_t *sphere, RigidBody_t *aabb);

#endif
