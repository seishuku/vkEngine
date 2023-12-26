#ifndef __AUDIO_H__
#define __AUDIO_H__

#define AUDIO_SAMPLE_RATE 44100
#define MAX_AUDIO_SAMPLES 4096
#define MAX_STREAM_SAMPLES (MAX_AUDIO_SAMPLES*2)
#define MAX_HRIR_SAMPLES 1024

typedef struct
{
    int16_t *data;
    uint32_t position, length;
    uint8_t channels;
    vec3 xyz;
} Sample_t;

bool Audio_LoadStatic(const char *filename, Sample_t *sample);
void Audio_PlaySample(Sample_t *sample, const bool looping, const float volume, vec3 *position);
void Audio_StopSample(Sample_t *sample);
void Audio_SetStreamCallback(void (*streamCallback)(void *buffer, size_t length));
void Audio_SetStreamVolume(const float volume);
void Audio_StartStream(void);
void Audio_StopStream(void);
int Audio_Init(void);
void Audio_Destroy(void);

#endif
