#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "audio.h"
#include "dsp.h"

#define MAX_DSP_EFFECTS 16

#define NUM_COMBS 4

typedef struct
{
    DSP_EffectFunc_t func;
    bool enabled;
} DSP_Effect_t;

static DSP_Effect_t effects[MAX_DSP_EFFECTS];
static size_t numEffects;

bool DSP_Init(void)
{
	numEffects=0;
    memset(effects, 0, sizeof(DSP_Effect_t)*MAX_DSP_EFFECTS);

	return true;
}

int32_t DSP_AddEffect(DSP_EffectFunc_t effect)
{
    if(numEffects>=MAX_DSP_EFFECTS||!effect)
        return -1;

    effects[numEffects].func=effect;
    effects[numEffects].enabled=true;
    numEffects++;

    return numEffects-1;
}

void DSP_SetEnabled(size_t index, bool enabled)
{
    if(index>=numEffects)
        return;

    effects[index].enabled=enabled;
}

void DSP_Process(int16_t *buffer, size_t length)
{
    for(size_t i=0;i<length;i++)
    {
        for(size_t j=0;j<numEffects;j++)
        {
            if(effects[j].enabled&&effects[j].func)
                effects[j].func(&buffer[2*i]);
        }
    }
}

// Example DSP effects
void DSP_LowPass(int16_t *samples)
{
    static float prev[2]={ 0 };
    const float alpha=0.6f;

    for(int ch=0;ch<2;ch++)
    {
        float current=samples[ch];

        prev[ch]=prev[ch]+alpha*(current-prev[ch]);
        samples[ch]=(int16_t)prev[ch];
    }
}

#define ECHO_DELAY_SAMPLES MS_TO_SAMPLES(150)
#define ECHO_DECAY 0.35f

static int16_t echoBuffer[ECHO_DELAY_SAMPLES*2];
static size_t echoIndex=0;

void DSP_Echo(int16_t *samples)
{
    for(int ch=0;ch<2;ch++)
    {
        size_t idx=echoIndex%ECHO_DELAY_SAMPLES;
        size_t echoPosition=2*idx+ch;
        int32_t delayed=echoBuffer[echoPosition];
        int32_t mixed=samples[ch]+(int32_t)(delayed*ECHO_DECAY);

        if(mixed>INT16_MAX)
            mixed=INT16_MAX;
        else if(mixed<INT16_MIN)
            mixed=INT16_MIN;

        echoBuffer[echoPosition]=samples[ch];
        samples[ch]=(int16_t)mixed;
    }

    echoIndex=(echoIndex+1)%ECHO_DELAY_SAMPLES;
}

void DSP_Bitcrusher(int16_t *samples)
{
    const int bits=3; // 4-bit output
    const int shift=16-bits;
    const int mask=~((1<<shift)-1);

    for(size_t i=0;i<2;i++)
    {
        int32_t s=samples[i];

        s=(s+(1<<(shift-1)))&mask;

        if(s>INT16_MAX)
            s=INT16_MAX;
        
        if(s<INT16_MIN)
            s=INT16_MIN;

        samples[i]=(int16_t)s;
    }
}

typedef struct
{
    int delaySamples;
    float buffer[2][MAX_AUDIO_SAMPLES];
    size_t index[2];
    float feedback;
} ReverbComb_t;

static ReverbComb_t combs[NUM_COMBS]=
{
    { .delaySamples=US_TO_SAMPLES(29700), .index=0, .feedback=0.72f },
    { .delaySamples=US_TO_SAMPLES(37100), .index=0, .feedback=0.72f },
    { .delaySamples=US_TO_SAMPLES(41100), .index=0, .feedback=0.72f },
    { .delaySamples=US_TO_SAMPLES(43700), .index=0, .feedback=0.72f },
};

static float CombFilter(ReverbComb_t *c, const size_t channel, const float input)
{
    const size_t index=c->index[channel];
    const float delayed=c->buffer[channel][index];

    c->buffer[channel][index]=input+delayed*c->feedback;
    c->index[channel]=(index+1)%c->delaySamples;

    return delayed;
}

#define REVERB_DRY 0.7f
#define REVERB_WET 0.3f

void DSP_Reverb(int16_t *samples)
{
	static bool init=true;
    const float dryL=samples[0];
    const float dryR=samples[1];
    float wetL=0.0f;
    float wetR=0.0f;

	if(init)
	{
		init=false;

		for(size_t i=0;i<NUM_COMBS;i++)
		{
			memset(combs[i].buffer, 0, sizeof(combs[i].buffer));
			combs[i].index[0]=0;
			combs[i].index[1]=0;
		}
	}

    for(size_t i=0;i<NUM_COMBS;i++)
    {
        wetL+=CombFilter(&combs[i], 0, dryL);
        wetR+=CombFilter(&combs[i], 1, dryR);
    }

    const float mixL=dryL*REVERB_DRY+(wetL/NUM_COMBS)*REVERB_WET;
    const float mixR=dryR*REVERB_DRY+(wetR/NUM_COMBS)*REVERB_WET;

    samples[0]=(int16_t)clampf(mixL, INT16_MIN, INT16_MAX);
    samples[1]=(int16_t)clampf(mixR, INT16_MIN, INT16_MAX);
}

int16_t soft_clip(int16_t sample)
{
    float x=sample/32768.0f;

	if(x>0.5f)
		x=0.5f;

	if(x<-0.5f)
		x=-0.5f;

	x=x-(x*x*x)/3.0f;

	return (int16_t)(x*32768.0f);
}

void DSP_Overdrive(int16_t *samples)
{
    for(size_t i=0;i<2;i++)
    {
        int32_t s=samples[i];
        samples[i]=(int16_t)soft_clip(s);
    }
}
