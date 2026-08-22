#include "../math/math.h"
#include "physics.h"

void SpringIntegrate(Spring_t *s, vec3 target, float dt)
{
	vec3 displacement=Vec3_Subv(s->position, target);
	const float length=Vec3_Normalize(&displacement);

	const float stretch=length-s->length;
	const vec3 force=Vec3_Muls(displacement, -s->stiffness*stretch);
	const vec3 dampingForce=Vec3_Muls(s->velocity, -s->damping);

	vec3 acceleration=Vec3_Muls(Vec3_Addv(force, dampingForce), s->invMass);

	s->velocity=Vec3_Addv(s->velocity, Vec3_Muls(acceleration, dt));
	s->position=Vec3_Addv(s->position, Vec3_Muls(s->velocity, dt));
}

static void ApplyConstraints(RigidBody_t *body, const float dt)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	const float maxRadius=2000.0f;
	const float maxVelocity=500.0f;

	// Clamp velocity, this reduces the chance of the simulation going unstable
	body->velocity=Vec3_Clamp(body->velocity, -maxVelocity, maxVelocity);

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 normal=Vec3_Subv(body->position, center);

	float distanceSq=Vec3_LengthSq(normal);
	const float maxRadiusMinusRadius = maxRadius - body->radius;
	const float boundarySq = maxRadiusMinusRadius * maxRadiusMinusRadius;

	if(distanceSq > boundarySq)
	{
		const float distance=Vec3_Normalize(&normal);
		const float penetration = distance + body->radius - maxRadius;
		body->force=Vec3_Addv(body->force, Vec3_Muls(normal, -penetration * 100.0f)); // Arbitrary stiffness
	}

	// Dampen velocity
	const float lambda=0.1f;
	const float decay=expf(-lambda*dt);

	body->velocity=Vec3_Muls(body->velocity, decay);
	body->angularVelocity=Vec3_Muls(body->angularVelocity, decay);
}

static vec4 IntegrateAngularVelocity(const vec4 q, const vec3 w, const float dt)
{
	const float halfDT=0.5f*dt;

	// First Midpoint step
	vec4 k1=Vec4_Muls(Vec4(
		 q.w*w.x+q.y*w.z-q.z*w.y,
		 q.w*w.y-q.x*w.z+q.z*w.x,
		 q.w*w.z+q.x*w.y-q.y*w.x,
		-q.x*w.x-q.y*w.y-q.z*w.z
	), halfDT);

	vec4 result=Vec4_Addv(q, k1);

	// Second Midpoint step
	vec4 k2=Vec4_Muls(Vec4(
		 result.w*w.x+result.y*w.z-result.z*w.y,
		 result.w*w.y-result.x*w.z+result.z*w.x,
		 result.w*w.z+result.x*w.y-result.y*w.x,
		-result.x*w.x-result.y*w.y-result.z*w.z
	), halfDT);

	result=Vec4_Addv(q, k2);

	Vec4_Normalize(&result);

	return result;
}

#include "../camera/camera.h"
extern Camera_t camera;

void PhysicsIntegrate(RigidBody_t *body, const float dt)
{
	// const vec3 gravity=Vec3(0.0f, -9.81f*WORLD_SCALE, 0.0f);
	const vec3 gravity=Vec3b(0.0f);

	// Apply gravity
	if(body!=&camera.body)
		body->force=Vec3_Addv(body->force, Vec3_Muls(gravity, body->mass));

	// Implicit Euler integration of position and velocity
	// Velocity+=Force/Mass*dt
	// Position+=Velocity*dt

	body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(body->force, body->invMass*dt));
	body->position=Vec3_Addv(body->position, Vec3_Muls(body->velocity, dt));

	body->force=Vec3b(0.0f);

	// Integrate angular velocity using quaternions
	body->orientation=IntegrateAngularVelocity(body->orientation, body->angularVelocity, dt);

	ApplyConstraints(body, dt);
}

void PhysicsExplode(RigidBody_t *body)
{
	const vec3 explosion_center={ 0.0f, 0.0f, 0.0f };

	// Calculate direction from explosion center to fragment
	vec3 direction=Vec3_Subv(body->position, explosion_center);
	Vec3_Normalize(&direction);

	// Calculate acceleration and impulse force
	const vec3 acceleration=Vec3_Muls(direction, EXPLOSION_POWER);

	// F=M*A bla bla...
	const vec3 force=Vec3_Muls(acceleration, body->mass);

	// Add it into object's velocity
	body->velocity=Vec3_Addv(body->velocity, force);
}

void PhysicsApplyImpulse(RigidBody_t *body, const vec3 impulse, const vec3 point)
{
	// Linear impulse
	body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(impulse, body->invMass));

	// Torque arm in local space
	const vec3 torque=Vec3_Cross(Vec3_Subv(point, body->position), impulse);
	const vec3 localTorque=QuatRotate(QuatInverse(body->orientation), torque);

	// Update angular velocity
	body->angularVelocity=Vec3_Addv(body->angularVelocity, Vec3_Muls(localTorque, body->invInertia));
}
