#include <float.h>
#include "physics.h"

const float correctionFactor=0.1f;

static void SolveDistanceConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const float length, const float dt)
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

	float bias=(correctionFactor/dt)*Cerr;
	float lambda=-(relativeVelocity+bias)/effectiveMass;
	vec3 impulse=Vec3_Muls(normal, lambda);

	PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse,-1.0f), pA);
	PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
}

static void SolveDistancePosition(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const float length)
{
    vec3 pA=Vec3_Addv(bodyA->position, QuatRotate(bodyA->orientation, localAnchorA));
    vec3 pB=Vec3_Addv(bodyB->position, QuatRotate(bodyB->orientation, localAnchorB));

    vec3 normal=Vec3_Subv(pB, pA);

    float distance=Vec3_Normalize(&normal);

    if(distance<FLT_EPSILON)
        return;

    float C=distance-length;

    float totalInvMass=bodyA->invMass+bodyB->invMass;

    if(totalInvMass<=0.0f)
		return;

	float correctionMagnitude=C*correctionFactor;
    vec3 correction=Vec3_Muls(normal, correctionMagnitude);

    bodyA->position=Vec3_Addv(bodyA->position, Vec3_Muls(correction, bodyA->invMass/totalInvMass));
    bodyB->position=Vec3_Subv(bodyB->position, Vec3_Muls(correction, bodyB->invMass/totalInvMass));
}

static void SolvePointConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const float dt)
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

		float bias=(correctionFactor/dt)*Vec3_Dot(Cerr, normal);
		float lambda=-(relativeVelocity+bias)/effectiveMass;
		vec3 impulse=Vec3_Muls(normal, lambda);

		PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse, -1.0f), pA);
		PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
	}
}

static void SolvePointPosition(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB)
{
    vec3 rA=QuatRotate(bodyA->orientation, localAnchorA);
    vec3 rB=QuatRotate(bodyB->orientation, localAnchorB);

    vec3 pA=Vec3_Addv(bodyA->position, rA);
    vec3 pB=Vec3_Addv(bodyB->position, rB);

    vec3 error=Vec3_Subv(pB, pA);

    float invMass=bodyA->invMass+bodyB->invMass;

    if(invMass<=0.0f)
        return;

    vec3 lambda=Vec3_Muls(error, correctionFactor/invMass);

    bodyA->position=Vec3_Addv(bodyA->position, Vec3_Muls(lambda, bodyA->invMass));
    bodyB->position=Vec3_Subv(bodyB->position, Vec3_Muls(lambda, bodyB->invMass));
}

static bool BuildPerpendicularBasis(vec3 axis, vec3 *u, vec3 *v)
{
    vec3 t=(fabsf(axis.x)<0.5f)?Vec3(1.0f, 0.0f, 0.0f):Vec3(0.0f, 1.0f, 0.0f);

    vec3 crossT=Vec3_Cross(axis, t);

    if(Vec3_Normalize(&crossT)<FLT_EPSILON)
        return false;

    *u=crossT;
    *v=Vec3_Cross(axis, *u);

    return true;
}

static void SolveHingeAxis(RigidBody_t *bodyA, RigidBody_t *bodyB, vec3 normal, vec3 angularError, float effectiveInertia, float dt)
{
	vec3 wA=QuatRotate(bodyA->orientation, bodyA->angularVelocity);
	vec3 wB=QuatRotate(bodyB->orientation, bodyB->angularVelocity);
	vec3 relAngularVel=Vec3_Subv(wB, wA);

	float relVelAlongAxis=Vec3_Dot(relAngularVel, normal);
	float bias=(correctionFactor/dt)*Vec3_Dot(angularError, normal);

	float lambda=-(relVelAlongAxis+bias)/effectiveInertia;
	vec3 angularImpulse=Vec3_Muls(normal, lambda);

	vec3 localImpulseA=QuatRotate(QuatInverse(bodyA->orientation), angularImpulse);
	vec3 localImpulseB=QuatRotate(QuatInverse(bodyB->orientation), angularImpulse);

	bodyA->angularVelocity=Vec3_Subv(bodyA->angularVelocity, Vec3_Muls(localImpulseA, bodyA->invInertia));
	bodyB->angularVelocity=Vec3_Addv(bodyB->angularVelocity, Vec3_Muls(localImpulseB, bodyB->invInertia));
}

static void SolveHingeConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA, const vec3 localAxisB, const float dt)
{
	SolvePointConstraint(bodyA, bodyB, localAnchorA, localAnchorB, dt);

	vec3 axisA=QuatRotate(bodyA->orientation, localAxisA);
	vec3 axisB=QuatRotate(bodyB->orientation, localAxisB);

	vec3 angularError=Vec3_Cross(axisA, axisB);

	vec3 u, v;

	if(!BuildPerpendicularBasis(axisA, &u, &v))
		return;

	float effectiveInertia=bodyA->invInertia+bodyB->invInertia;

	if(effectiveInertia<FLT_EPSILON)
		return;

	SolveHingeAxis(bodyA, bodyB, u, angularError, effectiveInertia, dt);
	SolveHingeAxis(bodyA, bodyB, v, angularError, effectiveInertia, dt);
}

static void ApplyAngularCorrection(RigidBody_t *body, vec3 correction, float sign)
{
    vec3 axis=Vec3_Muls(correction, sign*body->invInertia);
    float angle=Vec3_Length(axis);

    body->orientation=QuatMultiply(QuatAnglev(angle, axis), body->orientation);
    Vec4_Normalize(&body->orientation);
}

static void SolveHingePosition(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA, const vec3 localAxisB)
{
    SolvePointPosition(bodyA, bodyB, localAnchorA, localAnchorB);

    float invInertia=bodyA->invInertia+bodyB->invInertia;

    if(invInertia<=0.0f)
        return;

    vec3 axisA=QuatRotate(bodyA->orientation, localAxisA);
    vec3 axisB=QuatRotate(bodyB->orientation, localAxisB);

    vec3 angularError=Vec3_Cross(axisA, axisB);

    vec3 u, v;

    if(!BuildPerpendicularBasis(axisA, &u, &v))
        return;

    float lambdaA=-Vec3_Dot(angularError, u)*correctionFactor/invInertia;
    float lambdaB=-Vec3_Dot(angularError, v)*correctionFactor/invInertia;

    vec3 angularCorrection=Vec3_Addv(Vec3_Muls(u, lambdaA), Vec3_Muls(v, lambdaB));

    ApplyAngularCorrection(bodyA, angularCorrection, -1.0f);
    ApplyAngularCorrection(bodyB, angularCorrection, 1.0f);
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

static void SolvePrismaticConstraint(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA, const float dt)
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

		float bias=(correctionFactor/dt)*Vec3_Dot(Cerr, normal);
		float lambda=-(relativeVelocity+bias)/effectiveMass;

		vec3 impulse=Vec3_Muls(normal, lambda);

		PhysicsApplyImpulse(bodyA, Vec3_Muls(impulse, -1.0f), pA);
		PhysicsApplyImpulse(bodyB, Vec3_Muls(impulse, 1.0f), pB);
	}
}

static void SolvePrismaticPosition(RigidBody_t *bodyA, RigidBody_t *bodyB, const vec3 localAnchorA, const vec3 localAnchorB, const vec3 localAxisA)
{
    vec3 rA=QuatRotate(bodyA->orientation, localAnchorA);
    vec3 rB=QuatRotate(bodyB->orientation, localAnchorB);

    vec3 pA=Vec3_Addv(bodyA->position, rA);
    vec3 pB=Vec3_Addv(bodyB->position, rB);

    vec3 axisA=QuatRotate(bodyA->orientation, localAxisA);
    Vec3_Normalize(&axisA);

    vec3 error=Vec3_Subv(pB, pA);

    float errorAlongAxis=Vec3_Dot(error, axisA);

    vec3 correction=Vec3_Subv(error, Vec3_Muls(axisA, errorAlongAxis));
    float correctionLength=Vec3_Length(correction);

    if(correctionLength<FLT_EPSILON)
        return;

    float effectiveMass=bodyA->invMass+bodyB->invMass;

    if(effectiveMass<FLT_EPSILON)
        return;

    vec3 impulse=Vec3_Muls(correction, correctionFactor/effectiveMass);

    bodyA->position=Vec3_Subv(bodyA->position, Vec3_Muls(impulse, bodyA->invMass));
    bodyB->position=Vec3_Addv(bodyB->position, Vec3_Muls(impulse, bodyB->invMass));
}

void PhysicsSolveConstraint(Constraint_t *constraint, const float dt)
{
	if(dt<=0.0f)
		return;

	switch(constraint->type)
	{
		case CONSTRAINT_DISTANCE:
			SolveDistanceConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->distance, dt);
			break;

		case CONSTRAINT_POINT:
			SolvePointConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, dt);
			break;

		case CONSTRAINT_HINGE:
			SolveHingeConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA, constraint->localAxisB, dt);
			break;

		case CONSTRAINT_PRISMATIC:
			SolvePrismaticConstraint(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA, dt);
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

void PhysicsSolvePositionConstraint(Constraint_t *constraint)
{
    switch(constraint->type)
    {
        case CONSTRAINT_DISTANCE:
            SolveDistancePosition(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->distance);
            break;

        case CONSTRAINT_POINT:
            SolvePointPosition(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB);
            break;

        case CONSTRAINT_HINGE:
            SolveHingePosition(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA, constraint->localAxisB);
            break;

        case CONSTRAINT_PRISMATIC:
            SolvePrismaticPosition(constraint->bodyA, constraint->bodyB, constraint->localAnchorA, constraint->localAnchorB, constraint->localAxisA);
            break;

        case CONSTRAINT_ANGULAR_MOTOR:
		case CONSTRAINT_LINEAR_MOTOR:
        default:
            break;
    }
}
