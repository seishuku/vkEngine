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
#include "../ui/ui.h"
#include "../font/font.h"
#include "../console/console.h"
#include "input.h"

// External data from engine.c
extern uint32_t renderWidth, renderHeight;

void GenerateSkyParams(void);

extern Camera_t camera;

extern ParticleSystem_t particleSystem;

#define NUM_ASTEROIDS 1000
extern RigidBody_t asteroids[NUM_ASTEROIDS];

extern UI_t UI;
extern uint32_t cursorID;

extern Console_t console;
//////////////////////////////

// Emitter callback for the launched emitter's particles
void emitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle)
{
	particle->position=Vec3b(0.0f);

	// Simple -1.0 to 1.0 random spherical pattern, scaled by 100, fairly short lifespan.
	vec3 velocity=Vec3(RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
	Vec3_Normalize(&velocity);
	particle->velocity=Vec3_Muls(velocity, 10.0f);

	particle->life=RandFloat()*0.25f+0.01f;
}

// Launch a "missile"
// This is basically the same as an emitter callback, but done manually by working on the particle array directly.
void FireParticleEmitter(vec3 position, vec3 direction)
{
	// Get pointer to the emitter that's being used to drive the other emitters
	ParticleEmitter_t *emitter=List_GetPointer(&particleSystem.emitters, 0);

	// Create a new particle emitter
	vec3 randVec=Vec3(RandFloat(), RandFloat(), RandFloat());
	Vec3_Normalize(&randVec);
	randVec=Vec3_Muls(randVec, 100.0f);

	// Fire the emitter camera radius units away from Position in the direction of Direction
	uint32_t ID=ParticleSystem_AddEmitter(&particleSystem, position, Vec3b(0.0f), randVec, 5.0f, 500, false, emitterCallback);

	// Emitter list full?
	if(ID==UINT32_MAX)
		return;

	// Search list for first dead particle
	for(uint32_t i=0;i<emitter->numParticles;i++)
	{
		// When found, assign the new emitter ID to that particle, set position/direction/life and break out
		if(emitter->particles[i].life<0.0f)
		{
			emitter->particles[i].ID=ID;
			emitter->particles[i].position=position;
			Vec3_Normalize(&direction);
			emitter->particles[i].velocity=Vec3_Muls(direction, 100.0f);

			emitter->particles[i].life=10.0f;

			break;
		}
	}
}

void Event_KeyDown(void *arg)
{
	uint32_t *key=(uint32_t *)arg;

	if(console.active)
	{
		switch(*key)
		{
			// Toggle console
			case KB_GRAVE_ACCENT:
				console.active=!console.active;
				break;

			case KB_UP:
				Console_History(&console, true);
				break;

			case KB_DOWN:
				Console_History(&console, false);
				break;

			case KB_PAGE_UP:
				Console_Scroll(&console, true);
				break;

			case KB_PAGE_DOWN:
				Console_Scroll(&console, false);
				break;

			case KB_BACKSPACE:
				Console_Backspace(&console);
				break;

			case KB_ENTER:
				Console_Process(&console);
				break;

			default:
				Console_KeyInput(&console, *key);
		}

		// Ignore the rest of event while console is up
		return;
	}

	switch(*key)
	{
		// Toggle console
		case KB_GRAVE_ACCENT:
			console.active=!console.active;
			break;

		case KB_SPACE:
			Audio_PlaySample(&sounds[RandRange(SOUND_PEW1, SOUND_PEW3)], false, 1.0f, &camera.position);
			FireParticleEmitter(Vec3_Addv(camera.position, Vec3_Muls(camera.forward, camera.radius)), camera.forward);
			break;
		case KB_P:		GenerateSkyParams();	break;
		case KB_W:		camera.key_w=true;		break;
		case KB_S:		camera.key_s=true;		break;
		case KB_A:		camera.key_a=true;		break;
		case KB_D:		camera.key_d=true;		break;
		case KB_V:		camera.key_v=true;		break;
		case KB_C:		camera.key_c=true;		break;
		case KB_Q:		camera.key_q=true;		break;
		case KB_E:		camera.key_e=true;		break;
		case KB_UP:		camera.key_up=true;		break;
		case KB_DOWN:	camera.key_down=true;	break;
		case KB_LEFT:	camera.key_left=true;	break;
		case KB_RIGHT:	camera.key_right=true;	break;
		case KB_LSHIFT:
		case KB_RSHIFT:	camera.shift=true;		break;
		case KB_Z:
			for(int i=0;i<NUM_ASTEROIDS;i++)
				PhysicsExplode(&asteroids[i]);
			break;
		default:		break;
	}
}

void Event_KeyUp(void *arg)
{
	uint32_t *key=(uint32_t *)arg;

	switch(*key)
	{
		case KB_W:		camera.key_w=false;		break;
		case KB_S:		camera.key_s=false;		break;
		case KB_A:		camera.key_a=false;		break;
		case KB_D:		camera.key_d=false;		break;
		case KB_V:		camera.key_v=false;		break;
		case KB_C:		camera.key_c=false;		break;
		case KB_Q:		camera.key_q=false;		break;
		case KB_E:		camera.key_e=false;		break;
		case KB_UP:		camera.key_up=false;	break;
		case KB_DOWN:	camera.key_down=false;	break;
		case KB_LEFT:	camera.key_left=false;	break;
		case KB_RIGHT:	camera.key_right=false;	break;
		case KB_LSHIFT:
		case KB_RSHIFT:	camera.shift=false;		break;

		default:		break;
	}
}

static uint32_t activeID=UINT32_MAX;
vec2 mousePosition={ 0.0f, 0.0f };

void Event_MouseDown(void *arg)
{
	MouseEvent_t *mouseEvent=arg;

	if(mouseEvent->button&MOUSE_BUTTON_LEFT)
		activeID=UI_TestHit(&UI, mousePosition);
}

void Event_MouseUp(void *arg)
{
	MouseEvent_t *mouseEvent=arg;

	if(mouseEvent->button&MOUSE_BUTTON_LEFT)
		activeID=UINT32_MAX;
}

void Event_Mouse(void *arg)
{
	MouseEvent_t *mouseEvent=arg;

	// Calculate relative movement
	mousePosition=Vec2_Add(mousePosition, (float)mouseEvent->dx, (float)mouseEvent->dy);

	mousePosition.x=min(max(mousePosition.x, 0.0f), (float)renderWidth);
	mousePosition.y=min(max(mousePosition.y, 0.0f), (float)renderHeight);

	UI_UpdateCursorPosition(&UI, cursorID, mousePosition);

	if(activeID!=UINT32_MAX&&mouseEvent->button&MOUSE_BUTTON_LEFT)
		UI_ProcessControl(&UI, activeID, mousePosition);
	else if(mouseEvent->button&MOUSE_BUTTON_LEFT)
	{
		camera.yaw-=(float)mouseEvent->dx/8000.0f;
		camera.pitch+=(float)mouseEvent->dy/8000.0f;
	}
}
