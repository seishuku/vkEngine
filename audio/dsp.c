#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "audio.h"
#include "dsp.h"

#define MAX_DSP_EFFECTS 16

typedef struct
{
    DSP_EffectFunc_t func;
    bool enabled;
} DSP_Effect_t;

static DSP_Effect_t effects[MAX_DSP_EFFECTS];
static size_t numEffects;

bool DSP_Init(void)
{
    memset(effects, 0, sizeof(DSP_Effect_t)*MAX_DSP_EFFECTS);
    return true;
}

int32_t DSP_AddEffect(DSP_EffectFunc_t effect)
{
    if(numEffects>=MAX_DSP_EFFECTS&&!effect)
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

#define ECHO_DELAY_SAMPLES MS_TO_SAMPLES(50)
#define ECHO_DECAY 0.2f

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
    int16_t buffer[MAX_AUDIO_SAMPLES];
    size_t index;
    float feedback;
} ReverbComb_t;

void CombFilter(ReverbComb_t *c, int16_t *sample)
{
    int16_t delayed=c->buffer[c->index];
    int32_t out=delayed+(*sample);

    if(out>INT16_MAX)
        out=INT16_MAX;
    if(out<INT16_MIN)
        out=INT16_MIN;

    c->buffer[c->index]=(int16_t)(*sample+delayed*c->feedback);
    c->index=(c->index+1)%c->delaySamples;

    *sample=(int16_t)out;
}

#define NUM_COMBS 4
ReverbComb_t combs[NUM_COMBS]=
{
    { .delaySamples=US_TO_SAMPLES(29700), .index=0 },
    { .delaySamples=US_TO_SAMPLES(37100), .index=0 },
    { .delaySamples=US_TO_SAMPLES(41100), .index=0 },
    { .delaySamples=US_TO_SAMPLES(43700), .index=0 },
};

void DSP_Reverb(int16_t *samples)
{
    int32_t mixL=samples[0];
    int32_t mixR=samples[1];

    for(int i=0;i<NUM_COMBS;i++)
    {
        int16_t inputL=samples[0];
        int16_t inputR=samples[1];

        CombFilter(&combs[i], &inputL);
        CombFilter(&combs[i], &inputR);

        mixL+=inputL;
        mixR+=inputR;
    }

    // Clamp result
    if(mixL>INT16_MAX)
        mixL=INT16_MAX;
    if(mixL<INT16_MIN)
        mixL=INT16_MIN;
    if(mixR>INT16_MAX)
        mixR=INT16_MAX;
    if(mixR<INT16_MIN)
        mixR=INT16_MIN;

    samples[0]=(int16_t)mixL;
    samples[1]=(int16_t)mixR;
}

int16_t soft_clip(int16_t sample) {
    float x = sample / 32768.0f;
    if (x > 0.5f) x = 0.5f;
    if (x < -0.5f) x = -0.5f;
    x = x - (x * x * x) / 3.0f;
    return (int16_t)(x * 32768.0f);
}

void DSP_Overdrive(int16_t *samples)
{
    for(size_t i=0;i<2;i++)
    {
        int32_t s=samples[i];
        soft_clip(s);
        samples[i]=(int16_t)s;
    }
}
