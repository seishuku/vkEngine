#include <stdint.h>
#include <stdlib.h>
#include "physics.h"

void apply_border_constraints(RigidBody_t *body)
{
	vec3 center={ 0.0f, 0.0f, 0.0f };
	float radius=2000.0f;

	// Clamp velocity to "a number".
	//   This reduces the chance of the simulation going unstable.
	if(body->Velocity[0]>1000.0f)
		body->Velocity[0]=1000.0f;
	else if(body->Velocity[0]<-1000.0f)
		body->Velocity[0]=-1000.0f;

	if(body->Velocity[1]>1000.0f)
		body->Velocity[1]=1000.0f;
	else if(body->Velocity[1]<-1000.0f)
		body->Velocity[1]=-1000.0f;

	if(body->Velocity[2]>1000.0f)
		body->Velocity[2]=1000.0f;
	else if(body->Velocity[2]<-1000.0f)
		body->Velocity[2]=-1000.0f;

	vec3 normal;
	Vec3_Setv(normal, body->Position);
	Vec3_Subv(normal, center);

	float distance=sqrtf(Vec3_Dot(normal, normal));

	if(distance>radius)
	{
		Vec3_Normalize(normal);

		float dotProduct=Vec3_Dot(body->Velocity, normal);

		body->Velocity[0]-=2.0*dotProduct*normal[0];
		body->Velocity[1]-=2.0*dotProduct*normal[1];
		body->Velocity[2]-=2.0*dotProduct*normal[2];
	}
}

void integrate(RigidBody_t *body, float dt)
{
//	vec3 gravity={ 0.0f, 9.81f, 0.0f };
	const float damping=0.01f;

	// Clamp delta time, if it's longer than 16MS, clamp it to that.
	//   This reduces the chance of the simulation going unstable.
	if(dt>0.016f)
		dt=0.016f;

	// Apply damping force
	vec3 damping_force;
	Vec3_Setv(damping_force, body->Velocity);
	Vec3_Muls(damping_force, -damping);

	body->Force[0]+=damping_force[0];
	body->Force[1]+=damping_force[1];
	body->Force[2]+=damping_force[2];

	// Euler integration of position and velocity
	float dtSq=dt*dt;

	body->Position[0]+=body->Velocity[0]*dt+0.5f*body->Force[0]*body->invMass*dtSq;
	body->Position[1]+=body->Velocity[1]*dt+0.5f*body->Force[1]*body->invMass*dtSq;
	body->Position[2]+=body->Velocity[2]*dt+0.5f*body->Force[2]*body->invMass*dtSq;

	body->Velocity[0]+=body->Force[0]*body->invMass*dt;
	body->Velocity[1]+=body->Force[1]*body->invMass*dt;
	body->Velocity[2]+=body->Force[2]*body->invMass*dt;

	// Apply gravity
	//body->force[0]+=(gravity[0]*WORLD_SCALE)*body->mass*dt;
	//body->force[1]+=(gravity[1]*WORLD_SCALE)*body->mass*dt;
	//body->force[2]+=(gravity[2]*WORLD_SCALE)*body->mass*dt;
	body->Force[0]=0.0f;
	body->Force[1]=0.0f;
	body->Force[2]=0.0f;

	apply_border_constraints(body);
}

void explode(RigidBody_t *body)
{
	vec3 explosion_center={ 0.0f, 0.0f, 0.0f };

	// Random -1.0 to 1.0, normalized to get a random spherical vector
	Vec3_Set(body->Velocity, (float)rand()/RAND_MAX*2.0f-1.0f, (float)rand()/RAND_MAX*2.0f-1.0f, (float)rand()/RAND_MAX*2.0f-1.0f);
	Vec3_Normalize(body->Velocity);

	// Calculate distance and direction from explosion center to fragment
	vec3 direction;
	Vec3_Setv(direction, body->Position);
	Vec3_Subv(direction, explosion_center);
	Vec3_Normalize(direction);

	// Calculate acceleration and impulse force
	vec3 acceleration;
	Vec3_Setv(acceleration, direction);
	Vec3_Muls(acceleration, EXPLOSION_POWER);

	// F=M*A bla bla...
	vec3 force;
	Vec3_Setv(force, acceleration);
	Vec3_Muls(force, body->Mass);

	// Add it into object's velocity
	Vec3_Addv(body->Velocity, force);
}

void sphere_sphere_collision(RigidBody_t *a, RigidBody_t *b)
{
	vec3 normal;
	Vec3_Setv(normal, b->Position);
	Vec3_Subv(normal, a->Position);
	float distance_squared=Vec3_Dot(normal, normal);

	float radius_sum=a->Radius+b->Radius;
	float radius_sum_squared=radius_sum*radius_sum;

	if(distance_squared<radius_sum_squared)
	{
		float distance=sqrtf(distance_squared);
		float penetration=radius_sum-distance;

		// normalize
		Vec3_Muls(normal, 1.0f/distance);

		// calculate the relative velocity
		vec3 rel_velocity;
		Vec3_Setv(rel_velocity, b->Velocity);
		Vec3_Subv(rel_velocity, a->Velocity);

		float vel_along_normal=Vec3_Dot(rel_velocity, normal);

		// ignore the collision if the bodies are moving apart
		if(vel_along_normal>0.0f)
			return;

		// calculate the restitution coefficient
		float elasticity=0.8f; // typical values range from 0.1 to 1.0

		// calculate the impulse scalar
		float j=-(1.0f+elasticity)*vel_along_normal/(a->invMass+b->invMass);

		// apply the impulse
		vec3 a_impulse;
		Vec3_Setv(a_impulse, normal);
		Vec3_Muls(a_impulse, a->invMass);
		Vec3_Muls(a_impulse, j);
		Vec3_Subv(a->Velocity, a_impulse);

		vec3 b_impulse;
		Vec3_Setv(b_impulse, normal);
		Vec3_Muls(b_impulse, b->invMass);
		Vec3_Muls(b_impulse, j);
		Vec3_Addv(b->Velocity, b_impulse);

		// correct the position of the bodies to resolve the collision
		float k=0.1f; // typical values range from 0.01 to 0.1
		float percent=0.2f; // typical values range from 0.1 to 0.3
		float correction=penetration/(a->invMass+b->invMass)*percent*k;

		vec3 a_correction;
		Vec3_Setv(a_correction, normal);
		Vec3_Muls(a_correction, a->invMass);
		Vec3_Muls(a_correction, correction);
		Vec3_Subv(a->Position, a_correction);

		vec3 b_correction;
		Vec3_Setv(b_correction, normal);
		Vec3_Muls(b_correction, b->invMass);
		Vec3_Muls(b_correction, correction);
		Vec3_Addv(b->Position, b_correction);
	}
}

void sphere_aabb_collision(RigidBody_t *sphere, RigidBody_t *aabb)
{
	// Calculate the half extents of the AABB
	vec3 half;
	Vec3_Setv(half, aabb->Size);
	Vec3_Muls(half, 0.5f);

	// Calculate the AABB's min and max points
	vec3 aabbMin;
	Vec3_Setv(aabbMin, aabb->Position);
	Vec3_Subv(aabbMin, half);

	vec3 aabbMax;
	Vec3_Setv(aabbMax, aabb->Position);
	Vec3_Addv(aabbMax, half);

	// Find the closest point on the AABB to the sphere
	vec3 closest;
	Vec3_Set(closest,
			 fmaxf(aabbMin[0], fminf(sphere->Position[0], aabbMax[0])),
			 fmaxf(aabbMin[1], fminf(sphere->Position[1], aabbMax[1])),
			 fmaxf(aabbMin[2], fminf(sphere->Position[2], aabbMax[2])));

	// Calculate the distance between the closest point and the sphere's center
	vec3 normal;
	Vec3_Setv(normal, closest);
	Vec3_Subv(normal, sphere->Position);

	float distanceSquared=Vec3_Dot(normal, normal);

	// Check if the distance is less than the sphere's radius
	if(distanceSquared<=sphere->Radius*sphere->Radius)
	{
		float distance=sqrtf(distanceSquared);

		// Calculate the penetration depth
		float penetration=sphere->Radius-distance;

		// Normalize the collision normal
		Vec3_Muls(normal, 1.0f/distance);

		// Reflect the sphere's velocity based on the collision normal
		float VdotN=Vec3_Dot(sphere->Velocity, normal)*2.0f;
		vec3 reflection;
		Vec3_Setv(reflection, normal);
		Vec3_Muls(reflection, VdotN);

		// Apply to velocity
		Vec3_Subv(sphere->Velocity, reflection);

		VdotN=Vec3_Dot(aabb->Velocity, normal)*2.0f;
		Vec3_Setv(reflection, normal);
		Vec3_Muls(reflection, VdotN);

		// Apply to velocity
		Vec3_Subv(aabb->Velocity, reflection);

		// correct the position of the bodies to resolve the collision
		float k=0.1f; // typical values range from 0.01 to 0.1
		float percent=0.2f; // typical values range from 0.1 to 0.3
		float correction=penetration/(sphere->invMass+aabb->invMass)*percent*k;

		// Calculate the correction
		vec3 sphere_correction;
		Vec3_Setv(sphere_correction, normal);
		Vec3_Muls(sphere_correction, sphere->invMass);
		Vec3_Muls(sphere_correction, correction);

		// Adjust the sphere's position to resolve the collision
		Vec3_Addv(sphere->Position, sphere_correction);

		// Calculate the correction
		vec3 aabb_correction;
		Vec3_Setv(aabb_correction, normal);
		Vec3_Muls(aabb_correction, aabb->invMass);
		Vec3_Muls(aabb_correction, correction);

		// Adjust the aabb's position to resolve the collision
		Vec3_Addv(aabb->Position, aabb_correction);
	}
}
