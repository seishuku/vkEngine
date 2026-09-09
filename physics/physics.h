#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include "../math/math.h"

// Define constants
#define WORLD_SCALE 10.0f
#define EXPLOSION_POWER (1500.0f*WORLD_SCALE)

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

	float restitution;
	float friction;

	RigidBodyType_e type;	// OBB, sphere, capsule
	union
	{
		float radius;		// Sphere radius
		vec3 size;			// OBB half size
		vec2 radiusHeight;	// Capsule radius and half height
	}; // Type dimensions
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

vec3 AttractorOBBComputeGravity(vec3 position, vec3 center, vec3 halfExtents, vec4 orientation, float baseGravity, float influenceRadius);
vec3 AttractorCapsuleComputeGravity(vec3 position, vec3 center, vec4 orientation, float radius, float halfHeight, float baseGravity, float influenceRadius);
vec3 AttractorSphereComputeGravity(vec3 position, vec3 center, float radius, float baseGravity, float influenceRadius);

typedef enum
{
	CONSTRAINT_DISTANCE=0,
	CONSTRAINT_POINT,
	CONSTRAINT_HINGE,
	CONSTRAINT_PRISMATIC,
	CONSTRAINT_ANGULAR_MOTOR,
	CONSTRAINT_LINEAR_MOTOR,
	MAX_CONSTRAINTTYPE
} ConstraintType_e;

typedef struct
{
	ConstraintType_e type;

	RigidBody_t *bodyA;
	RigidBody_t *bodyB;

	vec3 localAnchorA;
	vec3 localAnchorB;

	union
	{
		float distance;

		struct
		{
			vec3 localAxisA;
			vec3 localAxisB;
		};

		struct
		{
			vec3 worldAxis;

			bool motorEnabled;
			float motorVelocity;
			float maxMotorForce;
		};
	};
} Constraint_t;

void PhysicsSolveConstraint(Constraint_t *constraint, const float dt);

#endif
