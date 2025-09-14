#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"

#define AUDIO_SAMPLE_RATE 44100
#define MAX_AUDIO_SAMPLES 4096
#define MAX_STREAM_SAMPLES (MAX_AUDIO_SAMPLES*2)
#define MAX_HRIR_SAMPLES 1024
#define MAX_AUDIO_STREAMS 8

typedef struct
{
    int16_t *data;
    uint32_t length;
    uint8_t channels;
} Sample_t;

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM			0x0001
#endif

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT	0x0003
#endif

typedef struct WaveFormat_s
{
	uint16_t formatTag;			// format type (PCM, float, etc)
	uint16_t channels;			// number of channels (mono, stereo, quad, etc)
	uint32_t samplesPerSec;		// sample rate
	uint32_t bytesPerSec;		// for buffer estimation
	uint16_t blockAlign;		// block size of data
	uint16_t bitsPerSample;		// number of bits per sample of mono data
} WaveFormat_t;

typedef struct
{
	// Waveform generation private variables
	int32_t phase, period;

	float fPeriod, fMaxPeriod;

	float fSlide, fDeltaSlide;

	float squareDuty, squareSlide;

	int32_t envelopeStage, envelopeTime, envelopeLength[3];
	float envelopeVolume;

	float fPhase, fDeltaPhase;
	int32_t iPhase;
	float phaserBuffer[1024];
	int32_t iPhaserPhase;

	float noiseBuffer[32];

	float fltPoint, fltPointDerivative, fltWidth;
	float fltWidthDerivative, fltDamping, fltHPPoint;
	float fltHPCutoff, fltHPDamping;

	float vibPhase, vibSpeed, vibAmplitude;

	int32_t repeatTime, repeatLimit;

	int32_t arpeggioTime, arpeggioLimit;
	float arpeggioModulation;

	// Waveform generation user parameters:
	// Wave type (square, sawtooth, sine, noise)
	int32_t waveType;

	// Wave envelope
	float attackTime;
	float sustainTime;
	float sustainPunch;
	float decayTime;

	// Frequency
	float startFrequency;
	float minFrequency;

	// Slide
	float slide;
	float deltaSlide;

	// Vibrato
	float vibratoDepth;
	float vibratoSpeed;

	// Tone change
	float changeAmount;
	float changeSpeed;

	// Square wave
	float squareWaveDuty;
	float squareWaveDutySweep;

	// Repeat
	float repeatSpeed;

	// Phaser
	float phaserOffset;
	float phaserSweep;

	// Filter
	float lpfCutoff;
	float lpfCutoffSweep;
	float lpfResonance;
	float hpfCutoff;
	float hpfCutoffSweep;
} WaveParams_t;

void Audio_SetListenerModelView(matrix modelView);
void Audio_SetListenerPosition(vec3 position);

void Audio_FillBuffer(void *buffer, uint32_t length);
bool Audio_LoadStatic(const char *filename, Sample_t *sample);
uint32_t Audio_PlaySample(Sample_t *sample, const bool looping, const float volume, vec3 position);
void Audio_UpdateXYZPosition(uint32_t slot, vec3 position);
void Audio_StopSample(uint32_t slot);
bool Audio_SetStreamCallback(uint32_t stream, void (*streamCallback)(void *buffer, size_t length));
bool Audio_SetStreamVolume(uint32_t stream, const float volume);
bool Audio_StartStream(uint32_t stream);
bool Audio_StopStream(uint32_t stream);
int Audio_Init(void);
void Audio_Destroy(void);

void *WavRead(const char *filename, WaveFormat_t *format, uint32_t *numSamples);
uint32_t WavWrite(const char *filename, int16_t *samples, uint32_t numSamples, uint32_t sampleRate, uint16_t channels);

void ResetWaveSample(WaveParams_t *params);
float GenerateWaveSample(WaveParams_t *params);

// Backend functions
bool AudioAndroid_Init(void);
void AudioAndroid_Destroy(void);
bool AudioPipeWire_Init(void);
void AudioPipeWire_Destroy(void);
bool AudioWASAPI_Init(void);
void AudioWASAPI_Destroy(void);

//bool AudioPortAudio_Init(void);
//void AudioPortAudio_Destroy(void);

#endif
