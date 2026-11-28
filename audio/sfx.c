#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../audio/audio.h"
#include "sfx.h"

#include "../camera/camera.h"
extern Camera_t camera;

extern bool isControlPressed;

#define FOOTSTEP_VARIANT_COUNT	4
#define FOOTSTEP_MAX_SAMPLES	(AUDIO_SAMPLE_RATE/4)

enum
{
	FOOTSTEP_WALK=0,
	FOOTSTEP_RUN,
	FOOTSTEP_KIND_COUNT
};

static int16_t footstepData[FOOTSTEP_KIND_COUNT][FOOTSTEP_VARIANT_COUNT][FOOTSTEP_MAX_SAMPLES];
static Sample_t footstepSamples[FOOTSTEP_KIND_COUNT][FOOTSTEP_VARIANT_COUNT];
static uint32_t nextFootstep[FOOTSTEP_KIND_COUNT];

static WaveParams_t Engine=
{
	.waveType=3,
	.attackTime=0.0f, .sustainTime=0.5f, .sustainPunch=0.0f, .decayTime=0.0f,
	.startFrequency=0.08f, .minFrequency=0.0f,
	.slide=0.0f, .deltaSlide=0.0f, .vibratoDepth=0.0f, .vibratoSpeed=0.0f,
	.changeAmount=0.0f, .changeSpeed=0.0f, .squareWaveDuty=0.0f, .squareWaveDutySweep=0.0f,
	.repeatSpeed=0.0f, .phaserOffset=0.0f, .phaserSweep=0.0f,
	.lpfCutoff=0.08f, .lpfCutoffSweep=0.0f, .lpfResonance=0.25f, .hpfCutoff=0.0f, .hpfCutoffSweep=0.0f
};

static WaveParams_t Laser=
{
	.waveType=0,
	.attackTime=0.0f, .sustainTime=0.5f, .sustainPunch=0.0f, .decayTime=0.0f,
	.startFrequency=0.33f, .minFrequency=0.0f,
	.slide=0.31f, .deltaSlide=0.2f, .vibratoDepth=0.57f, .vibratoSpeed=0.51f,
	.changeAmount=0.0f, .changeSpeed=0.61f, .squareWaveDuty=0.52f, .squareWaveDutySweep=-0.49f,
	.repeatSpeed=0.55f, .phaserOffset=0.24f, .phaserSweep=0.33f,
	.lpfCutoff=0.4f, .lpfCutoffSweep=0.0f, .lpfResonance=0.55f, .hpfCutoff=0.0f, .hpfCutoffSweep=0.0f
};

static WaveParams_t Footstep=
{
	.waveType=2,
	.attackTime=0.0f, .sustainTime=0.02f, .sustainPunch=0.25f, .decayTime=0.24f,
	.startFrequency=0.16f, .minFrequency=0.07f,
	.slide=0.15f, .deltaSlide=0.0f, .vibratoDepth=0.0f, .vibratoSpeed=0.0f,
	.changeAmount=0.0f, .changeSpeed=0.0f, .squareWaveDuty=0.0f, .squareWaveDutySweep=0.0f,
	.repeatSpeed=0.0f, .phaserOffset=0.0f, .phaserSweep=0.0f,
	.lpfCutoff=1.0f, .lpfCutoffSweep=0.0f, .lpfResonance=0.1f, .hpfCutoff=0.03f, .hpfCutoffSweep=0.0f
};

static WaveParams_t FootstepClick=
{
	.waveType=3,
	.attackTime=0.0f, .sustainTime=0.006f, .sustainPunch=0.1f, .decayTime=0.09f,
	.startFrequency=0.65f, .minFrequency=0.2f,
	.slide=0.3f, .deltaSlide=0.0f, .vibratoDepth=0.0f, .vibratoSpeed=0.0f,
	.changeAmount=0.0f, .changeSpeed=0.0f, .squareWaveDuty=0.0f, .squareWaveDutySweep=0.0f,
	.repeatSpeed=0.0f, .phaserOffset=0.0f, .phaserSweep=0.0f,
	.lpfCutoff=0.85f, .lpfCutoffSweep=0.0f, .lpfResonance=0.1f, .hpfCutoff=0.12f, .hpfCutoffSweep=0.0f
};

static WaveParams_t RunFootstep=
{
	.waveType=3,
	.attackTime=0.0f, .sustainTime=0.008f, .sustainPunch=0.15f, .decayTime=0.2f,
	.startFrequency=0.16f, .minFrequency=0.07f,
	.slide=0.0f, .deltaSlide=0.0f, .vibratoDepth=0.0f, .vibratoSpeed=0.0f,
	.changeAmount=0.0f, .changeSpeed=0.0f, .squareWaveDuty=0.0f, .squareWaveDutySweep=0.0f,
	.repeatSpeed=0.0f, .phaserOffset=0.0f, .phaserSweep=0.0f,
	.lpfCutoff=0.3f, .lpfCutoffSweep=0.0f, .lpfResonance=0.1f, .hpfCutoff=0.02f, .hpfCutoffSweep=0.0f
};

static WaveParams_t RunFootstepClick=
{
	.waveType=3,
	.attackTime=0.0f, .sustainTime=0.004f, .sustainPunch=0.15f, .decayTime=0.075f,
	.startFrequency=0.65f, .minFrequency=0.2f,
	.slide=0.0f, .deltaSlide=0.0f, .vibratoDepth=0.0f, .vibratoSpeed=0.0f,
	.changeAmount=0.0f, .changeSpeed=0.0f, .squareWaveDuty=0.0f, .squareWaveDutySweep=0.0f,
	.repeatSpeed=0.0f, .phaserOffset=0.0f, .phaserSweep=0.0f,
	.lpfCutoff=0.8f, .lpfCutoffSweep=0.0f, .lpfResonance=0.1f, .hpfCutoff=0.1f, .hpfCutoffSweep=0.0f
};

static void GenerateFootstepSample(Sample_t *sample, int16_t *data, WaveParams_t *thud, WaveParams_t *click, const float thudGain, const float clickGain)
{
	ResetWaveSample(thud);
	ResetWaveSample(click);

	bool thudActive=true, clickActive=true;
	uint32_t length=0;

	for(;length<FOOTSTEP_MAX_SAMPLES;length++)
	{
		float value=0.0f;

		if(thudActive)
		{
			value+=GenerateWaveSample(thud)*thudGain;
			thudActive=thud->envelopeStage!=0||thud->envelopeTime!=0;
		}

		if(clickActive)
		{
			value+=GenerateWaveSample(click)*clickGain;
			clickActive=click->envelopeStage!=0||click->envelopeTime!=0;
		}

		data[length]=(int16_t)(clampf(value, -1.0f, 1.0f)*INT16_MAX);

		if(!thudActive&&!clickActive)
			break;
	}

	sample->data=data;
	sample->length=length+1;
	sample->channels=1;
}

void SFXStreamData(void *buffer, size_t length)
{
	int16_t *bufferPtr=(int16_t *)buffer;

	for(uint32_t i=0;i<length;i++)
	{
		int16_t sample=0, laserSample=0, engineSample=0;

		// Mix in synth soundfx only when triggered
		if(isControlPressed)
		{
			laserSample=sample+(int16_t)(GenerateWaveSample(&Laser)*INT16_MAX);

			if(laserSample<INT16_MIN)
				laserSample=INT16_MIN;
			else if(laserSample>INT16_MAX)
				laserSample=INT16_MAX;

			sample=laserSample;
		}

		// Thruster hum is for the flying camera, walking shouldn't play it
		if(camera.mode==CAMERA_MODE_FLIGHT&&(Vec3_LengthSq(camera.moveAxis)>0.001f))
		{
			engineSample=sample+(int16_t)(GenerateWaveSample(&Engine)*INT16_MAX);

			if(engineSample<INT16_MIN)
				engineSample=INT16_MIN;
			else if(engineSample>INT16_MAX)
				engineSample=INT16_MAX;

			sample=engineSample;
		}

		if(sample<INT16_MIN)
			sample=INT16_MIN;
		else if(sample>INT16_MAX)
			sample=INT16_MAX;

		bufferPtr[0]=sample;
		bufferPtr[1]=sample;
		bufferPtr+=2;
	}
}

void SFX_Init(void)
{
	ResetWaveSample(&Laser);
	ResetWaveSample(&Engine);

	for(uint32_t i=0;i<FOOTSTEP_VARIANT_COUNT;i++)
	{
		GenerateFootstepSample(&footstepSamples[FOOTSTEP_WALK][i], footstepData[FOOTSTEP_WALK][i], &Footstep, &FootstepClick, 1.5f, 1.0f);
		GenerateFootstepSample(&footstepSamples[FOOTSTEP_RUN][i], footstepData[FOOTSTEP_RUN][i], &RunFootstep, &RunFootstepClick, 1.8f, 0.9f);
	}

	nextFootstep[FOOTSTEP_WALK]=0;
	nextFootstep[FOOTSTEP_RUN]=0;

	Audio_SetStreamCallback(1, SFXStreamData);
	Audio_SetStreamVolume(1, 1.0f);
	Audio_StartStream(1);
}

void SFX_Destroy(void)
{
}

void SFX_PlayFootstep(const vec3 position, const bool running)
{
	const uint32_t kind=running?FOOTSTEP_RUN:FOOTSTEP_WALK;

	Audio_PlaySample(&footstepSamples[kind][nextFootstep[kind]], false, 1.0f, position);
	nextFootstep[kind]=(nextFootstep[kind]+1)%FOOTSTEP_VARIANT_COUNT;
}
