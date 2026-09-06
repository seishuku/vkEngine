#include <float.h>
#include "physics.h"

void PhysicsSolveDistanceConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const DistanceConstraint_t *constraint)
{
	vec3 rA=QuatRotate(bodyA->orientation, constraint->localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, constraint->localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 normal=Vec3_Subv(pB, pA);
	float distance=Vec3_Normalize(&normal);

	float C=distance-constraint->length;

	vec3 velocityA=Vec3_Addv(bodyA->velocity, Vec3_Cross(QuatRotate(bodyA->orientation, bodyA->angularVelocity), rA));
	vec3 velocityB=Vec3_Addv(bodyB->velocity, Vec3_Cross(QuatRotate(bodyB->orientation, bodyB->angularVelocity), rB));

	float relativeVelocity=Vec3_Dot(normal, Vec3_Subv(velocityB, velocityA));

	vec3 raCrossN=Vec3_Cross(rA, normal);
	vec3 rbCrossN=Vec3_Cross(rB, normal);

	float effectiveMass=bodyA->invMass+bodyB->invMass+bodyA->invInertia*Vec3_Dot(raCrossN, raCrossN)+bodyB->invInertia*Vec3_Dot(rbCrossN, rbCrossN);

	if(effectiveMass<FLT_EPSILON)
		return;

	float bias=0.2f*C;
	float lambda=-(relativeVelocity+bias)/effectiveMass;
	vec3 impulse=Vec3_Muls(normal, lambda);

	PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse,-1.0f), pA);
	PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
}
