#include <float.h>
#include "physics.h"

const float contactBias=200.0f;

static void SolveDistanceConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const float length)
{
	vec3 rA=QuatRotate(bodyA->orientation, localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 normal=Vec3_Subv(pB, pA);
	float distance=Vec3_Normalize(&normal);

	float Cerr=distance-length;

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

static void SolvePointConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB)
{
	vec3 rA=QuatRotate(bodyA->orientation, localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, localAnchorB);

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

static void SolveHingeConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA, const vec3 localAxisB)
{
	SolvePointConstraint(bodyA, bodyB, localAnchorA, localAnchorB);

	vec3 axisA=QuatRotate(bodyA->orientation, localAxisA);
	vec3 axisB=QuatRotate(bodyB->orientation, localAxisB);

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

static void SolveAngularMotor(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 worldAxis, const float targetAngularVelocity, const float maxMotorForce, const float dt)
{
	vec3 wA=QuatRotate(bodyA->orientation, bodyA->angularVelocity);
	vec3 wB=QuatRotate(bodyB->orientation, bodyB->angularVelocity);
	
	float currentRelVel=Vec3_Dot(worldAxis, Vec3_Subv(wB, wA));
	float effectiveInertia=bodyA->invInertia+bodyB->invInertia;

	if(effectiveInertia<FLT_EPSILON)
		return;

	float deltaVel=targetAngularVelocity-currentRelVel;
	float lambda=deltaVel/effectiveInertia;

	float maxImpulse=maxMotorForce*dt;
	lambda=clampf(lambda, -maxImpulse, maxImpulse);

	vec3 angularImpulse=Vec3_Muls(worldAxis, lambda);

	vec3 localImpulseA=QuatRotate(QuatInverse(bodyA->orientation), angularImpulse);
	vec3 localImpulseB=QuatRotate(QuatInverse(bodyB->orientation), angularImpulse);

	bodyA->angularVelocity=Vec3_Subv(bodyA->angularVelocity, Vec3_Muls(localImpulseA, bodyA->invInertia));
	bodyB->angularVelocity=Vec3_Addv(bodyB->angularVelocity, Vec3_Muls(localImpulseB, bodyB->invInertia));
}

static void SolveLinearMotor(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 worldAxis, const float targetLinearVelocity, const float maxMotorForce, const float dt)
{
	float currentRelVel=Vec3_Dot(worldAxis, Vec3_Subv(bodyB->velocity, bodyA->velocity));

	float effectiveMass=bodyA->invMass+bodyB->invMass;

	if(effectiveMass<FLT_EPSILON)
		return;

	float deltaVel=targetLinearVelocity-currentRelVel;
	float lambda=deltaVel/effectiveMass;
	float maxImpulse=maxMotorForce*dt;

	vec3 impulse=Vec3_Muls(worldAxis, clampf(lambda, -maxImpulse, maxImpulse));

	PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse, -1.0f), bodyA->position);
	PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), bodyB->position);
}

static void SolvePrismaticConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA)
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

	vec3 rA=QuatRotate(bodyA->orientation, localAnchorA);
	vec3 rB=QuatRotate(bodyB->orientation, localAnchorB);

	vec3 pA=Vec3_Addv(bodyA->position, rA);
	vec3 pB=Vec3_Addv(bodyB->position, rB);

	vec3 axisA=QuatRotate(bodyA->orientation, localAxisA);

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

void PhysicsSolveConstraint(Constraint_t *constraint, const float dt)
{
	switch(constraint->type)
	{
		case CONSTRAINT_DISTANCE:
			SolveDistanceConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->distance);
			break;

		case CONSTRAINT_POINT:
			SolvePointConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB);
			break;

		case CONSTRAINT_HINGE:
			SolveHingeConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA, constraint->localAxisB);
			break;

		case CONSTRAINT_PRISMATIC:
			SolvePrismaticConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA);
			break;

		case CONSTRAINT_ANGULAR_MOTOR:
			if(constraint->motorEnabled)
				SolveAngularMotor(constraint->bodyA, constraint->bodyB, constraint->localAxisA, constraint->motorVelocity, constraint->maxMotorForce, dt);
			break;

		case CONSTRAINT_LINEAR_MOTOR:
			if(constraint->motorEnabled)
				SolveLinearMotor(constraint->bodyA, constraint->bodyB, constraint->localAxisA, constraint->motorVelocity, constraint->maxMotorForce, dt);
			break;

		default:
			break;
	}
}
