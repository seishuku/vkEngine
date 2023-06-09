#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "../camera/camera.h"
#include "list.h"
#include "../particle/particle.h"
#include "../audio/audio.h"
#include "../sounds.h"
#include "../physics/physics.h"
#include "input.h"

// External data from engine.c
void GenerateSkyParams(void);
extern Camera_t Camera;
extern ParticleSystem_t ParticleSystem;

#define NUM_ASTEROIDS 1000
extern RigidBody_t Asteroids[NUM_ASTEROIDS];

//////////////////////////////

// Emitter callback for the launched emitter's particles
void EmitterCallback(uint32_t Index, uint32_t NumParticles, Particle_t *Particle)
{
	Particle->pos=Vec3b(0.0f);

	// Simple -1.0 to 1.0 random spherical pattern, scaled by 100, fairly short lifespan.
	vec3 vel=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
	Vec3_Normalize(&vel);
	Particle->vel=Vec3_Muls(vel, 10.0f);

	Particle->life=RandFloat()*0.25f+0.01f;
}

// Launch a "missle"
// This is basically the same as an emitter callback, but done manually by working on the particle array directly.
static void FireParticleEmitter(vec3 Position, vec3 Direction)
{
	// Get pointer to the emitter that's being used to drive the other emitters
	ParticleEmitter_t *Emitter=List_GetPointer(&ParticleSystem.Emitters, 0);

	// Create a new particle emitter
	vec3 RandVec=Vec3(RandFloat(), RandFloat(), RandFloat());
	Vec3_Normalize(&RandVec);
	RandVec=Vec3_Muls(RandVec, 100.0f);
	uint32_t ID=ParticleSystem_AddEmitter(&ParticleSystem, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), RandVec, 5.0f, 500, false, EmitterCallback);

	// Search list for first dead particle
	for(uint32_t i=0;i<Emitter->NumParticles;i++)
	{
		// When found, assign the new emitter ID to that particle, set position/direction/life and break out
		if(Emitter->Particles[i].life<0.0f)
		{
			Emitter->Particles[i].ID=ID;
			Emitter->Particles[i].pos=Position;
			Vec3_Normalize(&Direction);
			Emitter->Particles[i].vel=Vec3_Muls(Direction, 100.0f);

			Emitter->Particles[i].life=10.0f;

			break;
		}
	}
}

void Event_KeyDown(void *Arg)
{
	uint32_t *Key=(uint32_t *)Arg;

	switch(*Key)
	{
		case KB_SPACE:
			Audio_PlaySample(&Sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &Camera.Position);
			FireParticleEmitter(Camera.Position, Camera.Forward);
			break;
		case KB_P:		GenerateSkyParams();	break;
		case KB_W:		Camera.key_w=true;		break;
		case KB_S:		Camera.key_s=true;		break;
		case KB_A:		Camera.key_a=true;		break;
		case KB_D:		Camera.key_d=true;		break;
		case KB_V:		Camera.key_v=true;		break;
		case KB_C:		Camera.key_c=true;		break;
		case KB_Q:		Camera.key_q=true;		break;
		case KB_E:		Camera.key_e=true;		break;
		case KB_UP:		Camera.key_up=true;		break;
		case KB_DOWN:	Camera.key_down=true;	break;
		case KB_LEFT:	Camera.key_left=true;	break;
		case KB_RIGHT:	Camera.key_right=true;	break;
		case KB_LSHIFT:
		case KB_RSHIFT:	Camera.shift=true;		break;
		case KB_Z:
			for(int i=0;i<NUM_ASTEROIDS;i++)
				PhysicsExplode(&Asteroids[i]);
			break;
		default:		break;
	}
}

void Event_KeyUp(void *Arg)
{
	uint32_t *Key=(uint32_t *)Arg;

	switch(*Key)
	{
		case KB_W:		Camera.key_w=false;		break;
		case KB_S:		Camera.key_s=false;		break;
		case KB_A:		Camera.key_a=false;		break;
		case KB_D:		Camera.key_d=false;		break;
		case KB_V:		Camera.key_v=false;		break;
		case KB_C:		Camera.key_c=false;		break;
		case KB_Q:		Camera.key_q=false;		break;
		case KB_E:		Camera.key_e=false;		break;
		case KB_UP:		Camera.key_up=false;	break;
		case KB_DOWN:	Camera.key_down=false;	break;
		case KB_LEFT:	Camera.key_left=false;	break;
		case KB_RIGHT:	Camera.key_right=false;	break;
		case KB_LSHIFT:
		case KB_RSHIFT:	Camera.shift=false;		break;

		default:		break;
	}
}

void Event_Mouse(void *Arg)
{
	MouseEvent_t *MouseEvent=Arg;

	if(MouseEvent->button&MOUSE_BUTTON_LEFT)
	{
		Camera.Yaw-=(float)MouseEvent->dx/8000.0f;
		Camera.Pitch-=(float)MouseEvent->dy/8000.0f;
	}
}