#include <float.h>
#include "../math/math.h"
#include "physics.h"

float PhysicsResolveCollision(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact)
{
	// Torque arms
	const vec3 r1=Vec3_Subv(contact.position, a->position);
	const vec3 r2=Vec3_Subv(contact.position, b->position);

    const vec3 wA=QuatRotate(a->orientation, a->angularVelocity);
    const vec3 wB=QuatRotate(b->orientation, b->angularVelocity);

	const vec3 relativeVel=Vec3_Subv(
        Vec3_Addv(b->velocity, Vec3_Cross(wB, r2)),
        Vec3_Addv(a->velocity, Vec3_Cross(wA, r1))
    );

	const float relativeSpeed=Vec3_Dot(relativeVel, contact.normal);

	if(relativeSpeed>0.0f)
		return 0.0f;

	// Masses
	const vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, contact.normal), a->invInertia), r1);
	const vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, contact.normal), b->invInertia), r2);
	const float invMassSum=a->invMass+b->invMass;

	const float e=fminf(a->restitution, b->restitution);
	const float j=-(1.0f+e)*relativeSpeed/(invMassSum+Vec3_Dot(contact.normal, Vec3_Addv(d1, d2)));

	const vec3 impulse=Vec3_Muls(contact.normal, j);

	// Head-on collision velocities

    // Linear velocity
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulse, a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulse, b->invMass));

    // Pre-calculate inverse orientation quats
	const vec4 invOrientationA=QuatInverse(a->orientation);
	const vec4 invOrientationB=QuatInverse(b->orientation);

	// Transform torque to local space
	vec3 localTorqueA=QuatRotate(invOrientationA, Vec3_Cross(r1, impulse));
	vec3 localTorqueB=QuatRotate(invOrientationB, Vec3_Cross(r2, impulse));

    // Angular velocity
	a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(localTorqueA, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(localTorqueB, b->invInertia));

	// Calculate tangential velocities
	vec3 tangentialVel=Vec3_Subv(relativeVel, Vec3_Muls(contact.normal, Vec3_Dot(relativeVel, contact.normal)));
    Vec3_Normalize(&tangentialVel);

	const vec3 d1T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, tangentialVel), a->invInertia), r1);
	const vec3 d2T=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, tangentialVel), b->invInertia), r2);

	const float friction=sqrtf(a->friction*b->friction);
	const float maxjT=friction*j;

	const float jT=clampf(-Vec3_Dot(relativeVel, tangentialVel)/(invMassSum+Vec3_Dot(tangentialVel, Vec3_Addv(d1T, d2T))), -maxjT, maxjT);

	const vec3 impulseT=Vec3_Muls(tangentialVel, jT);

	// Linear frictional velocity
	a->velocity=Vec3_Subv(a->velocity, Vec3_Muls(impulseT, a->invMass));
	b->velocity=Vec3_Addv(b->velocity, Vec3_Muls(impulseT, b->invMass));

	// Angular frictional velocity
	localTorqueA=QuatRotate(invOrientationA, Vec3_Cross(r1, impulseT));
	localTorqueB=QuatRotate(invOrientationB, Vec3_Cross(r2, impulseT));

	a->angularVelocity=Vec3_Subv(a->angularVelocity, Vec3_Muls(localTorqueA, a->invInertia));
	b->angularVelocity=Vec3_Addv(b->angularVelocity, Vec3_Muls(localTorqueB, b->invInertia));

    return sqrtf(-relativeSpeed);
}

void PhysicsPositionCorrection(RigidBody_t *a, RigidBody_t *b, ContactPoint_t contact)
{
	const float penetration=contact.penetration;
	const vec3 normal=contact.normal;
	const float invMassSum=a->invMass+b->invMass;

	if(invMassSum>FLT_EPSILON)
	{
		const float penetrationSlop=0.01f;
		const float percent=0.2f;
		const float correctionAmount=fmaxf(penetration-penetrationSlop, 0.0f)*percent;
		const vec3 correction=Vec3_Muls(normal, correctionAmount/invMassSum);

		a->position=Vec3_Subv(a->position, Vec3_Muls(correction, a->invMass));
		b->position=Vec3_Addv(b->position, Vec3_Muls(correction, b->invMass));
	}
}
