#include <float.h>
#include "physics.h"

const float contactBias=200.0f;

void PhysicsSolveDistanceConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const DistanceConstraint_t *constraint)
{
	vec3 rA=QuatRotate(bodyA->orientation, constraint->localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, constraint->localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 normal=Vec3_Subv(pB, pA);
	float distance=Vec3_Normalize(&normal);

	float Cerr=distance-constraint->length;

	vec3 velocityA=Vec3_Addv(bodyA->velocity, Vec3_Cross(QuatRotate(bodyA->orientation, bodyA->angularVelocity), rA));
	vec3 velocityB=Vec3_Addv(bodyB->velocity, Vec3_Cross(QuatRotate(bodyB->orientation, bodyB->angularVelocity), rB));

	float relativeVelocity=Vec3_Dot(normal, Vec3_Subv(velocityB, velocityA));

	vec3 raCrossN=Vec3_Cross(rA, normal);
	vec3 rbCrossN=Vec3_Cross(rB, normal);

	float effectiveMass=bodyA->invMass+bodyB->invMass+bodyA->invInertia*Vec3_Dot(raCrossN, raCrossN)+bodyB->invInertia*Vec3_Dot(rbCrossN, rbCrossN);

	if(effectiveMass<FLT_EPSILON)
		return;

	float bias=contactBias*Cerr;
	float lambda=-(relativeVelocity+bias)/effectiveMass;
	vec3 impulse=Vec3_Muls(normal, lambda);

	PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse,-1.0f), pA);
	PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
}

void PhysicsSolvePointConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const PointConstraint_t *constraint)
{
	vec3 rA=QuatRotate(bodyA->orientation, constraint->localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, constraint->localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 Cerr=Vec3_Subv(pB, pA);
	const vec3 axes[3]={ Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f) };

	for(uint32_t i=0;i<3;i++)
	{
		vec3 normal=axes[i];

		vec3 velocityA=Vec3_Addv(bodyA->velocity, Vec3_Cross(QuatRotate(bodyA->orientation, bodyA->angularVelocity), rA));
		vec3 velocityB=Vec3_Addv(bodyB->velocity, Vec3_Cross(QuatRotate(bodyB->orientation, bodyB->angularVelocity), rB));

		float relativeVelocity=Vec3_Dot(normal, Vec3_Subv(velocityB, velocityA));

		vec3 raCrossN=Vec3_Cross(rA, normal);
		vec3 rbCrossN=Vec3_Cross(rB, normal);

		float effectiveMass=bodyA->invMass+bodyB->invMass+bodyA->invInertia*Vec3_Dot(raCrossN, raCrossN)+bodyB->invInertia*Vec3_Dot(rbCrossN, rbCrossN);

		if(effectiveMass<FLT_EPSILON)
			continue;

		float bias=contactBias*Vec3_Dot(Cerr, normal);
		float lambda=-(relativeVelocity+bias)/effectiveMass;
		vec3 impulse=Vec3_Muls(normal, lambda);

		PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse, -1.0f), pA);
		PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
	}
}

void PhysicsSolveHingeConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const HingeConstraint_t *constraint)
{
	PointConstraint_t pointConstraint=
	{
		.bodyA=constraint->bodyA,
		.bodyB=constraint->bodyB,
		.localAnchorA=constraint->localAnchorA,
		.localAnchorB=constraint->localAnchorB
	};
	PhysicsSolvePointConstraint(bodyA, bodyB, &pointConstraint);

	vec3 axisA=QuatRotate(bodyA->orientation, constraint->localAxisA);
	vec3 axisB=QuatRotate(bodyB->orientation, constraint->localAxisB);

	vec3 angularError=Vec3_Cross(axisA, axisB);

	vec3 u;

	if(fabsf(axisA.x)<0.5f)
		u=Vec3(1.0f, 0.0f, 0.0f);
	else
		u=Vec3(0.0f, 1.0f, 0.0f);

	vec3 crossU=Vec3_Cross(axisA, u);

	if(Vec3_Normalize(&crossU)<FLT_EPSILON)
		return;

	u=crossU;
	vec3 v=Vec3_Cross(axisA, u);

	const vec3 perpendicularAxes[2]={ u, v };

	float effectiveInertia=bodyA->invInertia+bodyB->invInertia;

	if(effectiveInertia<FLT_EPSILON)
		return;

	for(uint32_t i=0;i<2;i++)
	{
		vec3 normal=perpendicularAxes[i];

		vec3 wA=QuatRotate(bodyA->orientation, bodyA->angularVelocity);
		vec3 wB=QuatRotate(bodyB->orientation, bodyB->angularVelocity);
		vec3 relAngularVel=Vec3_Subv(wB, wA);

		float relVelAlongAxis=Vec3_Dot(relAngularVel, normal);
		float bias=contactBias*Vec3_Dot(angularError, normal);

		float lambda=-(relVelAlongAxis+bias)/effectiveInertia;
		vec3 angularImpulse=Vec3_Muls(normal, lambda);

		vec3 localImpulseA=QuatRotate(QuatInverse(bodyA->orientation), angularImpulse);
		vec3 localImpulseB=QuatRotate(QuatInverse(bodyB->orientation), angularImpulse);

		bodyA->angularVelocity=Vec3_Subv(bodyA->angularVelocity, Vec3_Muls(localImpulseA, bodyA->invInertia));
		bodyB->angularVelocity=Vec3_Addv(bodyB->angularVelocity, Vec3_Muls(localImpulseB, bodyB->invInertia));
	}
}

void PhysicsSolveHingeMotor(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 worldAxis, float targetAngularVelocity, float maxMotorTorque, float dt)
{
	vec3 wA=QuatRotate(bodyA->orientation, bodyA->angularVelocity);
	vec3 wB=QuatRotate(bodyB->orientation, bodyB->angularVelocity);
	
	float currentRelVel=Vec3_Dot(worldAxis, Vec3_Subv(wB, wA));
	float effectiveInertia=bodyA->invInertia+bodyB->invInertia;

	if(effectiveInertia<FLT_EPSILON)
		return;

	float deltaVel=targetAngularVelocity-currentRelVel;
	float lambda=deltaVel/effectiveInertia;

	float maxImpulse=maxMotorTorque*dt;
	lambda=clampf(lambda, -maxImpulse, maxImpulse);

	vec3 angularImpulse=Vec3_Muls(worldAxis, lambda);

	vec3 localImpulseA=QuatRotate(QuatInverse(bodyA->orientation), angularImpulse);
	vec3 localImpulseB=QuatRotate(QuatInverse(bodyB->orientation), angularImpulse);

	bodyA->angularVelocity=Vec3_Subv(bodyA->angularVelocity, Vec3_Muls(localImpulseA, bodyA->invInertia));
	bodyB->angularVelocity=Vec3_Addv(bodyB->angularVelocity, Vec3_Muls(localImpulseB, bodyB->invInertia));
}

void PhysicsSolvePrismaticConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const PrismaticConstraint_t *constraint)
{
	vec3 wA=QuatRotate(bodyA->orientation, bodyA->angularVelocity);
	vec3 wB=QuatRotate(bodyB->orientation, bodyB->angularVelocity);
	vec3 relAngularVel=Vec3_Subv(wB, wA);

	float effectiveInertia=bodyA->invInertia+bodyB->invInertia;

	if(effectiveInertia>FLT_EPSILON)
	{
		const vec3 angularAxes[3]=
		{
			Vec3(1.0f, 0.0f, 0.0f),
			Vec3(0.0f, 1.0f, 0.0f),
			Vec3(0.0f, 0.0f, 1.0f)
		};

		for(uint32_t i=0;i<3;i++)
		{
			vec3 normal=angularAxes[i];
			float relVelAlongAxis=Vec3_Dot(relAngularVel, normal);

			float lambda=-relVelAlongAxis/effectiveInertia;
			vec3 angularImpulse=Vec3_Muls(normal, lambda);

			vec3 localImpulseA=QuatRotate(QuatInverse(bodyA->orientation), angularImpulse);
			vec3 localImpulseB=QuatRotate(QuatInverse(bodyB->orientation), angularImpulse);

			bodyA->angularVelocity=Vec3_Subv(bodyA->angularVelocity, Vec3_Muls(localImpulseA, bodyA->invInertia));
			bodyB->angularVelocity=Vec3_Addv(bodyB->angularVelocity, Vec3_Muls(localImpulseB, bodyB->invInertia));
		}
	}

	vec3 rA=QuatRotate(bodyA->orientation, constraint->localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, constraint->localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 axisA=QuatRotate(bodyA->orientation, constraint->localAxisA);

	vec3 u;

	if(fabsf(axisA.x)<0.5f)
		u=Vec3(1.0f, 0.0f, 0.0f);
	else
		u=Vec3(0.0f, 1.0f, 0.0f);

	u=Vec3_Cross(axisA, u);
	Vec3_Normalize(&u);
	vec3 v=Vec3_Cross(axisA, u);

	const vec3 perpendicularAxes[2]={ u, v };
	vec3 Cerr=Vec3_Subv(pB, pA);

	for(uint32_t i=0;i<2;i++)
	{
		vec3 normal=perpendicularAxes[i];

		vec3 velocityA=Vec3_Addv(bodyA->velocity, Vec3_Cross(QuatRotate(bodyA->orientation, bodyA->angularVelocity), rA));
		vec3 velocityB=Vec3_Addv(bodyB->velocity, Vec3_Cross(QuatRotate(bodyB->orientation, bodyB->angularVelocity), rB));

		float relativeVelocity=Vec3_Dot(normal, Vec3_Subv(velocityB, velocityA));

		vec3 raCrossN=Vec3_Cross(rA, normal);
		vec3 rbCrossN=Vec3_Cross(rB, normal);

		float effectiveMass=bodyA->invMass+bodyB->invMass+bodyA->invInertia*Vec3_Dot(raCrossN, raCrossN)+bodyB->invInertia*Vec3_Dot(rbCrossN, rbCrossN);

		if(effectiveMass<FLT_EPSILON)
			continue;

		float bias=contactBias*Vec3_Dot(Cerr, normal);
		float lambda=-(relativeVelocity+bias)/effectiveMass;

		vec3 impulse=Vec3_Muls(normal, lambda);

		PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse, -1.0f), pA);
		PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
	}
}
