#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"

#define SAMPLE_RATE 44100
#define NUM_SAMPLES 4096

typedef struct
{
    int16_t *data;
    uint32_t pos, len;
    uint8_t channels;
    vec3 xyz;
} Sample_t;

bool Audio_LoadStatic(char *Filename, Sample_t *Sample);
void Audio_SetListenerOrigin(vec3 pos, vec3 right);
void Audio_PlaySample(Sample_t *Sample, bool looping);
int Audio_Init(void);
void Audio_Destroy(void);

#endif
