#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "../camera/camera.h"
#include "../physics/physics.h"
#include "../physics/particle.h"
#include "../audio/audio.h"
#include "../ui/ui.h"
#include "../font/font.h"
#include "../console/console.h"
#include "../assetmanager.h"
#include "event.h"

void GenerateWorld(void);
void ResetPhysicsCubes(void);

extern Camera_t camera;

extern ParticleSystem_t particleSystem;

#define MAX_EMITTERS 1000
typedef struct
{
	RigidBody_t body;
	uint32_t ID;
	float life;
} PhyParticleEmitter_t;

extern PhyParticleEmitter_t emitters[MAX_EMITTERS];

extern RigidBody_t asteroids[NUM_ASTEROIDS];

extern UI_t UI;
extern uint32_t cursorID;

extern Console_t console;

extern bool isControlPressed;
extern bool pausePhysics;

extern RigidBody_t cubeBody0, cubeBody1;

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
	// Create a new particle emitter
	vec3 randVec=Vec3(RandFloat(), RandFloat(), RandFloat());
	Vec3_Normalize(&randVec);
	randVec=Vec3_Muls(randVec, 100.0f);

	// Fire the emitter camera radius units away from Position in the direction of Direction
	uint32_t ID=ParticleSystem_AddEmitter(&particleSystem, position, Vec3b(0.0f), randVec, 5.0f, 500, PARTICLE_EMITTER_CONTINOUS, emitterCallback);

	// Emitter list full?
	if(ID==UINT32_MAX)
		return;

	// Search for first dead particle emitter
	for(uint32_t i=0;i<MAX_EMITTERS;i++)
	{
		// When found, assign the new emitter ID to that particle, set position/direction/life and break out
		if(emitters[i].life<0.0f)
		{
			emitters[i].ID=ID;
			emitters[i].body.position=position;

			Vec3_Normalize(&direction);

			emitters[i].body.velocity=Vec3_Muls(direction, 100.0f);

			emitters[i].life=15.0f;

			break;
		}
	}

	// Finally, play the audio SFX
	Audio_PlaySample(&assets[assetIndices[RandRange(SOUND_PEW1, SOUND_PEW3)]].sound, false, 1.0f, position);
}

static uint32_t activeID=UINT32_MAX;
static vec2 mousePosition={ 0.0f, 0.0f };

#ifdef ANDROID
static vec2 oldMousePosition={ 0.0f, 0.0f };
#endif

bool Event_Trigger(EventID ID, void *arg)
{
	if(ID<0||ID>=NUM_EVENTS)
		return false;

	MouseEvent_t *mouseEvent=(MouseEvent_t *)arg;
	uint32_t key=*((uint32_t *)arg);

	switch(ID)
	{
		case EVENT_KEYDOWN:
		{
			if(console.active)
			{
				switch(key)
				{
					// Toggle console
					case KB_GRAVE_ACCENT:
						console.active=!console.active;
						break;

					case KB_UP:
						ConsoleHistory(&console, true);
						break;

					case KB_DOWN:
						ConsoleHistory(&console, false);
						break;

					case KB_PAGE_UP:
						ConsoleScroll(&console, true);
						break;

					case KB_PAGE_DOWN:
						ConsoleScroll(&console, false);
						break;

					case KB_BACKSPACE:
						ConsoleKeyInput(&console, '\b');
						break;

					case KB_TAB:
						ConsoleKeyInput(&console, '\t');
						break;

					case KB_ENTER:
						ConsoleKeyInput(&console, '\n');
						break;

					default:
						ConsoleKeyInput(&console, tolower(key));
				}

				// Ignore the rest of event while console is up
				return true;
			}

			UI_Control_t *control=UI_FindControlByID(&UI, activeID);

			if(activeID!=UINT32_MAX&&control->type==UI_CONTROL_EDITTEXT)
			{
				switch(key)
				{
					case KB_LEFT:
						UI_EditTextMoveCursor(&UI, activeID, -1);
						break;

					case KB_RIGHT:
						UI_EditTextMoveCursor(&UI, activeID, 1);
						break;

					case KB_BACKSPACE:
						UI_EditTextBackspace(&UI, activeID);
						break;

					case KB_DEL:
						UI_EditTextDelete(&UI, activeID);
						break;

					default:
						if(key>=32&&key<=126)
							UI_EditTextInsertChar(&UI, activeID, tolower(key));
						break;
				}

				return true;
			}

			switch(key)
			{
				// Toggle console
				case KB_GRAVE_ACCENT:
					console.active=!console.active;
					break;

				case KB_SPACE:
					FireParticleEmitter(Vec3_Addv(camera.body.position, Vec3_Muls(camera.forward, camera.body.radius)), camera.forward);
					break;
				case KB_I:
					ResetPhysicsCubes();				break;
				case KB_O:		GenerateWorld();		break;
				case KB_P:		pausePhysics=!pausePhysics; break;
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
				case KB_LCTRL:
				case KB_RCTRL:	isControlPressed=true;	break;
				default:		break;
			}

			break;
		}

		case EVENT_KEYUP:
		{
			switch(key)
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
				case KB_LCTRL:
				case KB_RCTRL:	isControlPressed=false;	break;

				default:		break;
			}

			break;
		}

		case EVENT_MOUSEDOWN:
		{
#ifndef ANDROID
			if(mouseEvent->button&MOUSE_BUTTON_LEFT)
				activeID=UI_TestHit(&UI, mousePosition);
#else
			mousePosition.x=(float)mouseEvent->dx;
			mousePosition.y=(float)mouseEvent->dy;

			if(mouseEvent->button&MOUSE_TOUCH)
			{
				activeID=UI_TestHit(&UI, mousePosition);

				if(activeID!=UINT32_MAX)
					UI_ProcessControl(&UI, activeID, mousePosition);
			}
#endif
			break;
		}

		case EVENT_MOUSEUP:
		{
#ifndef ANDROID
			if(mouseEvent->button&MOUSE_BUTTON_LEFT)
				activeID=UINT32_MAX;
#else
			mousePosition.x=(float)mouseEvent->dx;
			mousePosition.y=(float)mouseEvent->dy;

			if(mouseEvent->button&MOUSE_TOUCH)
				activeID=UINT32_MAX;
#endif
			break;
		}

		case EVENT_MOUSEMOVE:
		{
#ifndef ANDROID
			// Calculate relative movement
			mousePosition=Vec2_Add(mousePosition, (float)mouseEvent->dx, (float)mouseEvent->dy);

			mousePosition.x=clampf(mousePosition.x, 0.0f, (float)config.renderWidth);
			mousePosition.y=clampf(mousePosition.y, 0.0f, (float)config.renderHeight);

			UI_UpdateCursorPosition(&UI, cursorID, mousePosition);

			if(activeID!=UINT32_MAX&&mouseEvent->button&MOUSE_BUTTON_LEFT)
				UI_ProcessControl(&UI, activeID, mousePosition);
			else if(mouseEvent->button&MOUSE_BUTTON_LEFT)
			{
				camera.body.angularVelocity.x-=(float)mouseEvent->dy/200.0f;
				camera.body.angularVelocity.y-=(float)mouseEvent->dx/200.0f;
			}
#else
			oldMousePosition=mousePosition;
			mousePosition.x=(float)mouseEvent->dx;
			mousePosition.y=(float)mouseEvent->dy;

			if(activeID==UINT32_MAX&&mouseEvent->button&MOUSE_TOUCH)
			{
				camera.body.angularVelocity.x-=(float)(mousePosition.y-oldMousePosition.y)/200.0f;
				camera.body.angularVelocity.y-=(float)(mousePosition.x-oldMousePosition.x)/200.0f;
			}
#endif
			break;
		}

		default:
			return false;
	}

	return true;
}
