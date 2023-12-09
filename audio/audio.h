#ifndef __AUDIO_H__
#define __AUDIO_H__

#define AUDIO_SAMPLE_RATE 44100
#define MAX_AUDIO_SAMPLES 4096
#define MAX_STREAM_SAMPLES (MAX_AUDIO_SAMPLES*2)
#define MAX_HRIR_SAMPLES 1024

typedef struct
{
    int16_t *Data;
    uint32_t Position, Length;
    uint8_t Channels;
    vec3 xyz;
} Sample_t;

bool Audio_LoadStatic(const char *Filename, Sample_t *Sample);
void Audio_PlaySample(Sample_t *Sample, const bool Looping, const float Volume, vec3 *Position);
void Audio_StopSample(Sample_t *Sample);
void Audio_SetStreamCallback(void (*StreamCallback)(void *Buffer, size_t Length));
void Audio_SetStreamVolume(const float Volume);
void Audio_StartStream(void);
void Audio_StopStream(void);
int Audio_Init(void);
void Audio_Destroy(void);

#endif
