#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "../camera/camera.h"
#include "list.h"
#include "../particle/particle.h"
#include "input.h"

// External data from engine.c
void GenerateSkyParams(void);
extern Camera_t Camera;
extern ParticleSystem_t ParticleSystem;
extern uint32_t Emitters[2];
//////////////////////////////

void Event_KeyDown(void *Arg)
{
	uint32_t *Key=(uint32_t *)Arg;

	switch(*Key)
	{
		case KB_SPACE:	ParticleSystem_ResetEmitter(&ParticleSystem, Emitters[0]);	break;
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
		Camera.Yaw-=(float)MouseEvent->dx/800.0f;
		Camera.Pitch-=(float)MouseEvent->dy/800.0f;
	}
}