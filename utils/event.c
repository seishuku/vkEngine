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
extern uint32_t leftThumbstickID;
extern uint32_t rightThumbstickID;

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
	Audio_PlaySample(&AssetManager_GetAsset(assets, RandRange(SOUND_PEW1, SOUND_PEW3))->sound, false, 1.0f, position);
}

#define MAX_TOUCHES 4

static uint32_t activeID[MAX_TOUCHES]={ UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX };
static vec2 mousePosition[MAX_TOUCHES]=
{
	{ 0.0f, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, 0.0f },
	{ 0.0f, 0.0f },
};

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

			UI_Control_t *control=UI_FindControlByID(&UI, activeID[0]);

			if(activeID[0]!=UINT32_MAX&&control->type==UI_CONTROL_EDITTEXT)
			{
				switch(key)
				{
					case KB_LEFT:
						UI_EditTextMoveCursor(&UI, activeID[0], -1);
						break;

					case KB_RIGHT:
						UI_EditTextMoveCursor(&UI, activeID[0], 1);
						break;

					case KB_BACKSPACE:
						UI_EditTextBackspace(&UI, activeID[0]);
						break;

					case KB_DEL:
						UI_EditTextDelete(&UI, activeID[0]);
						break;

					default:
						if(key>=32&&key<=126)
							UI_EditTextInsertChar(&UI, activeID[0], tolower(key));
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
				case KB_W:		camera.moveForward=true;		break;
				case KB_S:		camera.moveBackward=true;		break;
				case KB_A:		camera.moveLeft=true;		break;
				case KB_D:		camera.moveRight=true;		break;
				case KB_V:		camera.moveUp=true;		break;
				case KB_C:		camera.moveDown=true;		break;
				case KB_Q:		camera.rollLeft=true;		break;
				case KB_E:		camera.rollRight=true;		break;
				case KB_UP:		camera.pitchUp=true;		break;
				case KB_DOWN:	camera.pitchDown=true;	break;
				case KB_LEFT:	camera.yawLeft=true;	break;
				case KB_RIGHT:	camera.yawRight=true;	break;
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
				case KB_W:		camera.moveForward=false;		break;
				case KB_S:		camera.moveBackward=false;		break;
				case KB_A:		camera.moveLeft=false;		break;
				case KB_D:		camera.moveRight=false;		break;
				case KB_V:		camera.moveUp=false;		break;
				case KB_C:		camera.moveDown=false;		break;
				case KB_Q:		camera.rollLeft=false;		break;
				case KB_E:		camera.rollRight=false;		break;
				case KB_UP:		camera.pitchUp=false;	break;
				case KB_DOWN:	camera.pitchDown=false;	break;
				case KB_LEFT:	camera.yawLeft=false;	break;
				case KB_RIGHT:	camera.yawRight=false;	break;
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
				activeID[0]=UI_TestHit(&UI, mousePosition[0]);
#else
			for(uint32_t i=0;i<MAX_TOUCHES;i++)
			{
				if(mouseEvent->button&(MOUSE_TOUCH1<<i))
				{
					mousePosition[i].x=(float)mouseEvent->dx;
					mousePosition[i].y=(float)mouseEvent->dy;

					activeID[i]=UI_TestHit(&UI, mousePosition[i]);
				}

				if(activeID[i]==leftThumbstickID)
					UI_SetVirtualStickActive(&UI, leftThumbstickID, true);

				if(activeID[i]==rightThumbstickID)
					UI_SetVirtualStickActive(&UI, rightThumbstickID, true);
			}
#endif
			break;
		}

		case EVENT_MOUSEUP:
		{
#ifndef ANDROID
			if(mouseEvent->button&MOUSE_BUTTON_LEFT)
				activeID[0]=UINT32_MAX;
#else
			for(uint32_t i=0;i<MAX_TOUCHES;i++)
			{
				if(activeID[i]==leftThumbstickID)
					UI_SetVirtualStickActive(&UI, leftThumbstickID, false);

				if(activeID[i]==rightThumbstickID)
					UI_SetVirtualStickActive(&UI, rightThumbstickID, false);

				if(mouseEvent->button&(MOUSE_TOUCH1<<i))
				{
					mousePosition[i].x=(float)mouseEvent->dx;
					mousePosition[i].y=(float)mouseEvent->dy;

					activeID[i]=UINT32_MAX;
				}
			}
#endif
			break;
		}

		case EVENT_MOUSEMOVE:
		{
#ifndef ANDROID
			// Calculate relative movement
			mousePosition[0]=Vec2_Add(mousePosition[0], (float)mouseEvent->dx, (float)mouseEvent->dy);

			mousePosition[0].x=clampf(mousePosition[0].x, 0.0f, (float)config.renderWidth);
			mousePosition[0].y=clampf(mousePosition[0].y, 0.0f, (float)config.renderHeight);

			UI_UpdateCursorPosition(&UI, cursorID, mousePosition[0]);

			if(activeID[0]!=UINT32_MAX&&mouseEvent->button&MOUSE_BUTTON_LEFT)
				UI_ProcessControl(&UI, activeID[0], mousePosition[0]);
			else if(mouseEvent->button&MOUSE_BUTTON_LEFT)
			{
				camera.body.angularVelocity.x-=(float)mouseEvent->dy/200.0f;
				camera.body.angularVelocity.y-=(float)mouseEvent->dx/200.0f;
			}
#else
			if(activeID[0]!=UINT32_MAX&&mouseEvent->button&MOUSE_TOUCH1)
			{
				mousePosition[0].x=(float)mouseEvent->dx;
				mousePosition[0].y=(float)mouseEvent->dy;

				UI_ProcessControl(&UI, activeID[0], mousePosition[0]);
			}

			if(activeID[1]!=UINT32_MAX&&mouseEvent->button&MOUSE_TOUCH2)
			{
				mousePosition[1].x=(float)mouseEvent->dx;
				mousePosition[1].y=(float)mouseEvent->dy;

				UI_ProcessControl(&UI, activeID[1], mousePosition[1]);
			}

			if(activeID[2]!=UINT32_MAX&&mouseEvent->button&MOUSE_TOUCH3)
			{
				mousePosition[2].x=(float)mouseEvent->dx;
				mousePosition[2].y=(float)mouseEvent->dy;

				UI_ProcessControl(&UI, activeID[2], mousePosition[2]);
			}

			if(activeID[3]!=UINT32_MAX&&mouseEvent->button&MOUSE_TOUCH4)
			{
				mousePosition[3].x=(float)mouseEvent->dx;
				mousePosition[3].y=(float)mouseEvent->dy;

				UI_ProcessControl(&UI, activeID[3], mousePosition[3]);
			}

#endif
			break;
		}

		default:
			return false;
	}

	return true;
}
