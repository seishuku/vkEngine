#ifndef __AUDIO_H__
#define __AUDIO_H__

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
void Audio_PlaySample(Sample_t *Sample, bool looping, float vol, vec3 *pos);
void Audio_StopSample(Sample_t *Sample);
int Audio_Init(void);
void Audio_Destroy(void);

typedef struct
{
	vec3 Vertex;
	float Left[NUM_SAMPLES], Right[NUM_SAMPLES];
} HRIR_Vertex_t;

typedef struct
{
	uint32_t Magic;
	uint32_t SampleRate;
	uint32_t SampleLength;
	uint32_t NumVertex;
	uint32_t NumIndex;
	uint32_t *Indices;
	HRIR_Vertex_t *Vertices;
} HRIR_Sphere_t;

#define HRIR_MAGIC ('H'|('R'<<8)|('I'<<16)|('R'<<24))

bool HRIR_Init(void);

#endif
