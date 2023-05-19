#include <stdint.h>
#include <stdlib.h>
#include "physics.h"

// For sound playback on collision
#include "../audio/audio.h"
#include "../sounds.h"

static void apply_border_constraints(RigidBody_t *Body)
{
	vec3 Center={ 0.0f, 0.0f, 0.0f };
	float Radius=2000.0f;

	// Clamp velocity to "a number".
	//   This reduces the chance of the simulation going unstable.
	if(Body->Velocity[0]>1000.0f)
		Body->Velocity[0]=1000.0f;
	else if(Body->Velocity[0]<-1000.0f)
		Body->Velocity[0]=-1000.0f;

	if(Body->Velocity[1]>1000.0f)
		Body->Velocity[1]=1000.0f;
	else if(Body->Velocity[1]<-1000.0f)
		Body->Velocity[1]=-1000.0f;

	if(Body->Velocity[2]>1000.0f)
		Body->Velocity[2]=1000.0f;
	else if(Body->Velocity[2]<-1000.0f)
		Body->Velocity[2]=-1000.0f;

	// Check for collision with outer boundary sphere and reflect velocity if needed
	vec3 Normal;
	Vec3_Setv(Normal, Body->Position);
	Vec3_Subv(Normal, Center);

	float distanceSq=Vec3_Dot(Normal, Normal);

	if(distanceSq>Radius*Radius)
	{
		float Distance=sqrt(distanceSq);

		Vec3_Muls(Normal, 1.0f/Distance);

		float VdotN=Vec3_Dot(Body->Velocity, Normal);

		Body->Velocity[0]-=2.0*VdotN*Normal[0];
		Body->Velocity[1]-=2.0*VdotN*Normal[1];
		Body->Velocity[2]-=2.0*VdotN*Normal[2];
	}
}

void PhysicsIntegrate(RigidBody_t *body, float dt)
{
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

	body->Force[0]=0.0f;
	body->Force[1]=0.0f;
	body->Force[2]=0.0f;

	apply_border_constraints(body);
}

void PhysicsExplode(RigidBody_t *body)
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

void PhysicsSphereToSphereCollisionResponse(RigidBody_t *a, RigidBody_t *b)
{
	// Calculate the distance between the camera and the sphere's center
	vec3 Normal;
	Vec3_Setv(Normal, a->Position);
	Vec3_Subv(Normal, b->Position);

	float DistanceSq=Vec3_Dot(Normal, Normal);

	// Sum of radii
	float radiusSum=a->Radius+b->Radius;

	// Check if the distance is less than the sum of the radii
	if(DistanceSq<=radiusSum*radiusSum)
	{
		// Get the distance between objects
		float Distance=sqrt(DistanceSq);

		// Normalize the normal
		Vec3_Muls(Normal, 1.0f/Distance);

		// Calculate amount of overlap
		float Penetration=radiusSum-Distance;

		// Mass calculations
		float massRatio=b->Mass/a->Mass;
		float totalMass=b->Mass+a->Mass;

		// Object A position correction
		float aMovement=Penetration*(b->Mass/totalMass)*1.01f;
		a->Position[0]+=Normal[0]*aMovement;
		a->Position[1]+=Normal[1]*aMovement;
		a->Position[2]+=Normal[2]*aMovement;

		// Object A velocity response (reflection)
		float aNdotV=Vec3_Dot(Normal, a->Velocity);
		a->Velocity[0]-=2.0f*massRatio*Normal[0]*aNdotV;
		a->Velocity[1]-=2.0f*massRatio*Normal[1]*aNdotV;
		a->Velocity[2]-=2.0f*massRatio*Normal[2]*aNdotV;

		// Object B position correction
		float bMovement=Penetration*(a->Mass/totalMass)*1.01f;
		a->Position[0]+=Normal[0]*bMovement;
		a->Position[1]+=Normal[1]*bMovement;
		a->Position[2]+=Normal[2]*bMovement;

		// Object B velocity response (reflection)
		float bNdotV=Vec3_Dot(Normal, b->Velocity);
		b->Velocity[0]-=2.0f*(1.0f-massRatio)*Normal[0]*bNdotV;
		b->Velocity[1]-=2.0f*(1.0f-massRatio)*Normal[1]*bNdotV;
		b->Velocity[2]-=2.0f*(1.0f-massRatio)*Normal[2]*bNdotV;

		//Audio_PlaySample(&Sounds[SOUND_STONES], false);
	}
}
