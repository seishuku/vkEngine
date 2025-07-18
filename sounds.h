#ifndef __SOUNDS_H__
#define __SOUNDS_H__

#include "audio/audio.h"

typedef enum
{
	SOUND_PEW1=0,
	SOUND_PEW2,
	SOUND_PEW3,
	SOUND_STONE1,
	SOUND_STONE2,
	SOUND_STONE3,
	SOUND_CRASH,
	SOUND_EXPLODE1,
	SOUND_EXPLODE2,
	SOUND_EXPLODE3,
	NUM_SOUNDS
} SoundIDs;

typedef struct
{
	const char *filename;
	Sample_t sample;
} Sounds_t;

extern Sounds_t sounds[NUM_SOUNDS];

#endif
