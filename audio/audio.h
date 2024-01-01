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

	float fltp, fltdp, fltw;
	float fltwd, fltdmp, fltphp;
	float flthp, flthpd;

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

bool Audio_LoadStatic(const char *filename, Sample_t *sample);
void Audio_PlaySample(Sample_t *sample, const bool looping, const float volume, vec3 *position);
void Audio_StopSample(Sample_t *sample);
void Audio_SetStreamCallback(void (*streamCallback)(void *buffer, size_t length));
void Audio_SetStreamVolume(const float volume);
void Audio_StartStream(void);
void Audio_StopStream(void);
int Audio_Init(void);
void Audio_Destroy(void);

void ResetWaveSample(WaveParams_t *params);
float GenerateWaveSample(WaveParams_t *params);

#endif
