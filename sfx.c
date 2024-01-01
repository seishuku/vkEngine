#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "system/system.h"
#include "math/math.h"
#include "audio/audio.h"
#include "sfx.h"

#include "camera/camera.h"
extern Camera_t camera;

static WaveParams_t Engine=
{
	.waveType=3,

	.attackTime=0.0f,
	.sustainTime=0.5f,
	.sustainPunch=0.0f,
	.decayTime=0.0f,

	.startFrequency=0.08f,
	.minFrequency=0.0f,

	.slide=0.0f,
	.deltaSlide=0.0f,
	.vibratoDepth=0.0f,
	.vibratoSpeed=0.0f,

	.changeAmount=0.0f,
	.changeSpeed=0.0f,
	.squareWaveDuty=0.0f,
	.squareWaveDutySweep=0.0f,

	.repeatSpeed=0.0f,
	.phaserOffset=0.0f,
	.phaserSweep=0.0f,

	.lpfCutoff=0.08f,
	.lpfCutoffSweep=0.0f,
	.lpfResonance=0.25f,
	.hpfCutoff=0.0f,
	.hpfCutoffSweep=0.0f
};

static WaveParams_t Laser=
{
	.waveType=0,

	.attackTime=0.0f,
	.sustainTime=0.5f,
	.sustainPunch=0.0f,
	.decayTime=0.0f,

	.startFrequency=0.33f,
	.minFrequency=0.0f,

	.slide=0.31f,
	.deltaSlide=0.2f,
	.vibratoDepth=0.57f,
	.vibratoSpeed=0.51f,

	.changeAmount=0.0f,
	.changeSpeed=0.61f,
	.squareWaveDuty=0.52f,
	.squareWaveDutySweep=-0.49f,

	.repeatSpeed=0.55f,
	.phaserOffset=0.24f,
	.phaserSweep=0.33f,

	.lpfCutoff=0.4f,
	.lpfCutoffSweep=0.0f,
	.lpfResonance=0.55f,
	.hpfCutoff=0.0f,
	.hpfCutoffSweep=0.0f
};

void SFXStreamData(void *buffer, size_t length)
{
	int16_t *bufferPtr=(int16_t *)buffer;

	for(uint32_t i=0;i<length;i++)
	{
		int16_t sample=0;

		// Mix in synth soundfx only when triggered
		if(camera.shift)
		{
			int16_t laserSample=(int16_t)(GenerateWaveSample(&Laser)*INT16_MAX);
			sample+=min(max(sample+laserSample, INT16_MIN), INT16_MAX);
		}

		if(camera.key_w||camera.key_s||camera.key_a||camera.key_d||camera.key_v||camera.key_c)
		{
			int16_t engineSample=(int16_t)(GenerateWaveSample(&Engine)*INT16_MAX);
			sample=min(max(sample+engineSample, INT16_MIN), INT16_MAX);
		}

		sample=min(max(sample, INT16_MIN), INT16_MAX);

		bufferPtr[0]=sample;
		bufferPtr[1]=sample;
		bufferPtr+=2;
	}
}

void SFX_Init(void)
{
	ResetWaveSample(&Laser);
	ResetWaveSample(&Engine);

	Audio_SetStreamCallback(1, SFXStreamData);
	Audio_SetStreamVolume(1, 1.0f);
	Audio_StartStream(1);
}

void SFX_Destroy(void)
{
}
