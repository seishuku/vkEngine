#ifndef __SOUNDS_H__
#define __SOUNDS_H__

#include "audio/audio.h"

enum
{
	SOUND_PEW1,
	SOUND_PEW2,
	SOUND_PEW3,
	NUM_SOUNDS
};

extern Sample_t Sounds[NUM_SOUNDS];

#endif
