#ifndef __AUDIO_H__
#define __AUDIO_H__

#define SAMPLE_RATE 44100
#define NUM_SAMPLES 1024
#define MAX_HRIR_SAMPLES 1024

typedef struct
{
    int16_t *Data;
    uint32_t Position, Length;
    uint8_t Channels;
    vec3 xyz;
} Sample_t;

bool Audio_LoadStatic(char *Filename, Sample_t *Sample);
void Audio_PlaySample(Sample_t *Sample, bool Looping, float Volume, vec3 *Position);
void Audio_StopSample(Sample_t *Sample);
int Audio_Init(void);
void Audio_Destroy(void);

#endif
