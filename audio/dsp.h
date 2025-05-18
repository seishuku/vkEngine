#ifndef __DSP_H__
#define __DSP_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "audio.h"

#ifndef MS_TO_SAMPLES
#define MS_TO_SAMPLES(x) ((x)*AUDIO_SAMPLE_RATE/1000)
#endif

#ifndef US_TO_SAMPLES
#define US_TO_SAMPLES(x) ((x)*AUDIO_SAMPLE_RATE/1000000)
#endif

typedef void (*DSP_EffectFunc_t)(int16_t *samples);

bool DSP_Init(void);
int32_t DSP_AddEffect(DSP_EffectFunc_t effect);
void DSP_SetEnabled(size_t index, bool enabled);
void DSP_Process(int16_t *buffer, size_t length);

void DSP_LowPass(int16_t *samples);
void DSP_Echo(int16_t *samples);
void DSP_Bitcrusher(int16_t *samples);
void DSP_Reverb(int16_t *samples);
void DSP_Overdrive(int16_t *samples);

#endif
