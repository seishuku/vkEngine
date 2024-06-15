// This handles Microsoft wave file loading and SFX waveform generation
// Wave files supported are 8, 16, 24, 32bit PCM, and 32 and 64bit float.

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "audio.h"

// Chunk magic markers
#define RIFF_MAGIC ('R'|('I'<<8)|('F'<<16)|('F'<<24))
#define WAVE_MAGIC ('W'|('A'<<8)|('V'<<16)|('E'<<24))
#define FMT_MAGIC  ('f'|('m'<<8)|('t'<<16)|(' '<<24))
#define DATA_MAGIC ('d'|('a'<<8)|('t'<<16)|('a'<<24))

typedef struct RIFFChunk_s
{
	uint32_t magic;
	uint32_t size;
} RIFFChunk_t;

#define WAVE_FORMAT_PCM			0x0001
#define WAVE_FORMAT_IEEE_FLOAT	0x0003

typedef struct WaveFormat_s
{
	uint16_t formatTag;			// format type (PCM, float, etc)
	uint16_t channels;			// number of channels (mono, stereo, quad, etc)
	uint32_t samplesPerSec;		// sample rate
	uint32_t bytesPerSec;		// for buffer estimation
	uint16_t blockAlign;		// block size of data
	uint16_t bitsPerSample;		// number of bits per sample of mono data
} WaveFormat_t;

// Simple resample and conversion function to the audio engine's common format (44.1KHz/16bit).
// Based off of what id Software used in Quake, seems to work well enough.
static void ConvertAndResample(const void *in, const uint32_t inLength, const WaveFormat_t waveFormat, int16_t *out)
{
	if(in==NULL||out==NULL)
		return;

	const float stepScale=(float)waveFormat.samplesPerSec/AUDIO_SAMPLE_RATE;
	const uint32_t outCount=(uint32_t)(inLength/stepScale);

	for(uint32_t i=0, sampleFrac=0;i<outCount;i++, sampleFrac+=(int32_t)(stepScale*256))
	{
		const int32_t srcSample=sampleFrac>>8;
		int32_t sampleL=0, sampleR=0;

		// Input conversion/resample
		if(waveFormat.formatTag==WAVE_FORMAT_PCM)
		{
			if(waveFormat.bitsPerSample==32) // 32bit signed integer PCM
			{
				if(waveFormat.channels==2)
				{
					sampleL=((int32_t *)in)[2*srcSample+0]>>16;
					sampleR=((int32_t *)in)[2*srcSample+1]>>16;
				}
				else if(waveFormat.channels==1)
					sampleL=((int32_t *)in)[srcSample]>>16;
			}
			else if(waveFormat.bitsPerSample==24) // 24bit signed integer PCM
			{
				if(waveFormat.channels==2)
				{
					sampleL=(((uint8_t *)in)[i*6+0]|(((uint8_t *)in)[i*6+1]<<8)|(((uint8_t *)in)[i*6+2]<<16))>>8;
					sampleR=(((uint8_t *)in)[i*6+3]|(((uint8_t *)in)[i*6+4]<<8)|(((uint8_t *)in)[i*6+5]<<16))>>8;
				}
				else if(waveFormat.channels==1)
					sampleL=(((uint8_t *)in)[i*3+0]|(((uint8_t *)in)[i*3+1]<<8)|(((uint8_t *)in)[i*3+2]<<16))>>8;
			}
			else if(waveFormat.bitsPerSample==16) // 16bit signed integer PCM
			{
				if(waveFormat.channels==2)
				{
					sampleL=((int16_t *)in)[2*srcSample+0];
					sampleR=((int16_t *)in)[2*srcSample+1];
				}
				else if(waveFormat.channels==1)
					sampleL=((int16_t *)in)[srcSample];
			}
			else if(waveFormat.bitsPerSample==8) // 8bit unsigned integer PCM
			{
				if(waveFormat.channels==2)
				{
					sampleL=(((int8_t *)in)[2*srcSample+0]-128)<<8;
					sampleR=(((int8_t *)in)[2*srcSample+1]-128)<<8;
				}
				else if(waveFormat.channels==1)
					sampleL=(((int8_t *)in)[srcSample]-128)<<8;
			}
		}
		else if(waveFormat.formatTag==WAVE_FORMAT_IEEE_FLOAT)
		{
			if(waveFormat.bitsPerSample==64) // 64bit float
			{
				if(waveFormat.channels==2)
				{
					sampleL=(((double *)in)[2*srcSample+0]*INT16_MAX);
					sampleR=(((double *)in)[2*srcSample+1]*INT16_MAX);
				}
				else
					sampleL=(((double *)in)[srcSample]*INT16_MAX);
			}
			else if(waveFormat.bitsPerSample==32) // 32bit float
			{
				if(waveFormat.channels==2)
				{
					sampleL=(((float *)in)[2*srcSample+0]*INT16_MAX);
					sampleR=(((float *)in)[2*srcSample+1]*INT16_MAX);
				}
				else
					sampleL=(((float *)in)[srcSample]*INT16_MAX);
			}
		}

		// Output (16bit signed int)
		if(waveFormat.channels==2)
		{
			out[2*i+0]=(int16_t)sampleL;
			out[2*i+1]=(int16_t)sampleR;
		}
		else if(waveFormat.channels==1)
			out[i]=(int16_t)sampleL;
	}
}

// Load a WAVE sound file.
bool Audio_LoadStatic(const char *filename, Sample_t *sample)
{
	FILE *stream=NULL;
	RIFFChunk_t riff={ 0 }, chunk={ 0 };
	uint32_t waveMagic=0;
	WaveFormat_t waveFormat={ 0 };
	int32_t fileSize=0;
	uint8_t *buffer=NULL;

	stream=fopen(filename, "rb");
   
	if(stream==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to open file %s.\n", filename);
		return false;
	}

	if(fseek(stream, 0, SEEK_END))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to seek to end of file %s.\n", filename);
		goto error;
	}

	fileSize=ftell(stream);

	if(fileSize==-1)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to tell position of file %s.\n", filename);
		goto error;
	}

	if(fseek(stream, 0, SEEK_SET))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to seek beginning of file %s.\n", filename);
		goto error;
	}

	// RIFF header chunk, check chunk magic marker and make sure file size matches the chunk size
	if(fread(&riff, sizeof(RIFFChunk_t), 1, stream)!=1||riff.magic!=RIFF_MAGIC)
	{
		DBGPRINTF(DEBUG_ERROR, "Invalid RIFF chunk in file %s.\n", filename);
		goto error;
	}

	// WAVE magic marker ("WAVE") should follow after the RIFF chunk
	if(fread(&waveMagic, sizeof(uint32_t), 1, stream)!=1&&waveMagic!=WAVE_MAGIC)
	{
		DBGPRINTF(DEBUG_ERROR, "Invalid wave magic in file %s.\n", filename);
		goto error;
	}

	while(!feof(stream))
	{
		// Read in a chunk to process
		if(!fread(&chunk, sizeof(RIFFChunk_t), 1, stream))
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to read chunk in file %s.\n", filename);
			goto error;
		}

		switch(chunk.magic)
		{
			case FMT_MAGIC:
			{
				if(fread(&waveFormat, sizeof(WaveFormat_t), 1, stream)!=1)
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to read wave format chunk in file %s.\n", filename);
					goto error;
				}

				// Only support PCM streams and up to 2 channels
				if(!(waveFormat.formatTag==WAVE_FORMAT_PCM||waveFormat.formatTag==WAVE_FORMAT_IEEE_FLOAT)||waveFormat.channels>2)
				{
					DBGPRINTF(DEBUG_ERROR, "Unsupported wave format in file %s.\n", filename);
					goto error;
				}

				break;
			}

			case DATA_MAGIC:
			{
				buffer=(uint8_t *)Zone_Malloc(zone, chunk.size);

				if(buffer==NULL)
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for file %s.\n", filename);
					goto error;
				}

				// Read in audio data
				if(fread(buffer, 1, chunk.size, stream)!=chunk.size)
				{
					DBGPRINTF(DEBUG_ERROR, "Unable to read audio data in file %s.\n", filename);
					goto error;
				}

				fclose(stream);
				stream=NULL;

				// Convert data byte size to number of samples
				const uint32_t numSamples=chunk.size/(waveFormat.bitsPerSample>>3)/waveFormat.channels;

				// Covert to match primary buffer sampling rate
				const uint32_t outputSize=(uint32_t)(numSamples/((float)waveFormat.samplesPerSec/AUDIO_SAMPLE_RATE));

				if(outputSize!=numSamples)
				{
#ifdef _DEBUG
					DBGPRINTF(DEBUG_INFO, "Converting %s wave format from %dHz/%dbit to %d/16bit.\n", filename, waveFormat.samplesPerSec, waveFormat.bitsPerSample, AUDIO_SAMPLE_RATE);
#endif
					int16_t *resampledAndConverted=(int16_t *)Zone_Malloc(zone, sizeof(int16_t)*outputSize*waveFormat.channels);

					if(resampledAndConverted==NULL)
					{
						DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for conversion buffer for file %s.\n", filename);
						goto error;
					}

					// Resample PCM data to our common sample rate
					ConvertAndResample(buffer, numSamples, waveFormat, resampledAndConverted);

					Zone_Free(zone, buffer);

					sample->data=resampledAndConverted;
				}
				else
					sample->data=(int16_t *)buffer;

				sample->length=outputSize;
				sample->channels=(uint8_t)waveFormat.channels;

				// Done, return out
				return true;
			}

			default:
				fseek(stream, chunk.size, SEEK_CUR);
				break;
		}
	}

error:
	if(stream)
		fclose(stream);

	if(buffer)
		Zone_Free(zone, buffer);

	return false;
}

static const float speedRatio=100.0f;

void ResetWaveSample(WaveParams_t *params)
{
	// Minimum frequency can't be higher than start frequency
	params->minFrequency=fminf(params->startFrequency, params->minFrequency);

	// Slide can't be less than delta slide
	params->slide=fmaxf(params->deltaSlide, params->slide);

	params->fPeriod=speedRatio/(params->startFrequency*params->startFrequency+0.001f);
	params->fMaxPeriod=speedRatio/(params->minFrequency*params->minFrequency+0.001f);
	params->phase=0;
	params->period=(int32_t)params->fPeriod;

	params->fSlide=1.0f-(params->slide*params->slide*params->slide)*0.01f;
	params->fDeltaSlide=-(params->deltaSlide*params->deltaSlide*params->deltaSlide)*0.000001f;

	params->squareDuty=0.5f-params->squareWaveDuty*0.5f;
	params->squareSlide=-params->squareWaveDutySweep*0.00005f;

	params->arpeggioTime=0;
	params->arpeggioLimit=(int32_t)(((1.0f-params->changeSpeed)*(1.0f-params->changeSpeed))*20000.0f+32.0f);

	if(params->changeSpeed>1.0f)
		params->arpeggioLimit=0;

	if(params->changeAmount>=0.0f)
		params->arpeggioModulation=1.0f-(params->changeAmount*params->changeAmount)*0.9f;
	else
		params->arpeggioModulation=1.0f+(params->changeAmount*params->changeAmount)*10.0f;

	// Reset filter parameters
	params->fltPoint=0.0f;
	params->fltPointDerivative=0.0f;
	params->fltWidth=(params->lpfCutoff*params->lpfCutoff*params->lpfCutoff)*0.1f;
	params->fltWidthDerivative=1.0f+params->lpfCutoffSweep*0.0001f;
	params->fltDamping=fminf(0.8f, 5.0f/(1.0f+(params->lpfResonance*params->lpfResonance)*20.0f)*(0.01f+params->fltWidth));
	params->fltHPPoint=0.0f;

	params->fltHPCutoff=(params->hpfCutoff*params->hpfCutoff)*0.1f;
	params->fltHPDamping=1.0f+params->hpfCutoffSweep*0.0003f;

	// Reset vibrato
	params->vibPhase=0.0f;
	params->vibSpeed=(params->vibratoSpeed*params->vibratoSpeed)*0.01f;
	params->vibAmplitude=params->vibratoDepth*0.5f;

	// Reset envelope
	params->envelopeStage=0;
	params->envelopeTime=0;
	params->envelopeLength[0]=(int32_t)fmaxf(1.0f, params->attackTime*params->attackTime*100000.0f);
	params->envelopeLength[1]=(int32_t)fmaxf(1.0f, params->sustainTime*params->sustainTime*100000.0f);
	params->envelopeLength[2]=(int32_t)fmaxf(1.0f, params->decayTime*params->decayTime*100000.0f);
	params->envelopeVolume=0.0f;

	params->fPhase=(params->phaserOffset*params->phaserOffset)*1020.0f;

	if(params->phaserOffset<0.0f)
		params->fPhase=-params->fPhase;

	params->fDeltaPhase=(params->phaserSweep*params->phaserSweep)*1.0f;

	if(params->phaserSweep<0.0f)
		params->fDeltaPhase=-params->fDeltaPhase;

	params->iPhase=abs((int32_t)params->fPhase);
	memset(params->phaserBuffer, 0, sizeof(float)*1024);
	params->iPhaserPhase=0;

	for(uint32_t i=0;i<32;i++)
		params->noiseBuffer[i]=RandFloatRange(-1.0f, 1.0f);

	params->repeatTime=0;

	if(params->repeatSpeed>0.0f)
		params->repeatLimit=(int32_t)((1.0f-params->repeatSpeed)*(1.0f-params->repeatSpeed)*20000.0f+32.0f);
	else
		params->repeatLimit=0;
}

float GenerateWaveSample(WaveParams_t *params)
{
	params->repeatTime++;

	if((params->repeatLimit!=0)&&(params->repeatTime>=params->repeatLimit))
	{
		// Reset sample parameters (only some of them)
		params->repeatTime=0;

		params->fPeriod=speedRatio/(params->startFrequency*params->startFrequency+0.001f);
		params->fMaxPeriod=speedRatio/(params->minFrequency*params->minFrequency+0.001f);
		params->period=(int32_t)params->fPeriod;

		params->fSlide=1.0f-(params->slide*params->slide*params->slide)*0.01f;
		params->fDeltaSlide=-(params->deltaSlide*params->deltaSlide*params->deltaSlide)*0.000001f;

		params->squareDuty=0.5f-params->squareWaveDuty*0.5f;
		params->squareSlide=-params->squareWaveDutySweep*0.00005f;

		params->arpeggioTime=0;
		params->arpeggioLimit=(int32_t)(((1.0f-params->changeSpeed)*(1.0f-params->changeSpeed))*20000.0f+32.0f);

		if(params->changeSpeed>1.0f)
			params->arpeggioLimit=0;

		if(params->changeAmount>=0.0f)
			params->arpeggioModulation=1.0f-(params->changeAmount*params->changeAmount)*0.9f;
		else
			params->arpeggioModulation=1.0f+(params->changeAmount*params->changeAmount)*10.0f;
	}

	// Frequency envelopes/arpeggios
	params->arpeggioTime++;

	if((params->arpeggioLimit!=0)&&(params->arpeggioTime>=params->arpeggioLimit))
	{
		params->arpeggioLimit=0;
		params->fPeriod*=params->arpeggioModulation;
	}

	params->fSlide+=params->fDeltaSlide;
	params->fPeriod=fminf(params->fMaxPeriod, params->fPeriod*params->fSlide);

	if(params->vibAmplitude>0.0f)
	{
		params->vibPhase+=params->vibSpeed;
		params->period=max(8, (int)(params->fPeriod*(1.0+sinf(params->vibPhase)*params->vibAmplitude)));
	}
	else
		params->period=max(8, (int)params->fPeriod);

	params->squareDuty+=params->squareSlide;
	params->squareDuty=clampf(params->squareDuty, 0.0f, 0.5f);

	// Volume envelope
	if(++params->envelopeTime>params->envelopeLength[params->envelopeStage])
	{
		params->envelopeTime=0;
		params->envelopeStage++;
	}

	switch(params->envelopeStage)
	{
		case 0:
			params->envelopeVolume=(float)params->envelopeTime/params->envelopeLength[0];
			break;

		case 1:
			params->envelopeVolume=2.0f-(float)params->envelopeTime/params->envelopeLength[1]*2.0f*params->sustainPunch;
			break;

		case 2:
			params->envelopeVolume=1.0f-(float)params->envelopeTime/params->envelopeLength[2];
			break;

		// Reset envelope stage, this would normally end waveform generation
		case 3:
		default:
			params->envelopeStage=0;
			params->envelopeTime=0;
			params->envelopeLength[0]=(int32_t)fmaxf(1.0f, params->attackTime*params->attackTime*100000.0f);
			params->envelopeLength[1]=(int32_t)fmaxf(1.0f, params->sustainTime*params->sustainTime*100000.0f);
			params->envelopeLength[2]=(int32_t)fmaxf(1.0f, params->decayTime*params->decayTime*100000.0f);
			params->envelopeVolume=0.0f;
			break;
	}

	// Phaser step
	params->fPhase+=params->fDeltaPhase;
	params->iPhase=min(1023, abs((int32_t)params->fPhase));

	float superSample=0.0f;

	// 8x supersampling
	for(int32_t i=0;i<8;i++)
	{
		float sample=0.0f;

		if(++params->phase>=params->period)
		{
			params->phase%=params->period;

			if(params->waveType==3)
			{
				for(uint32_t i=0;i<32;i++)
					params->noiseBuffer[i]=RandFloatRange(-1.0f, 1.0f);
			}
		}

		// Base waveform
		float phaseFraction=(float)params->phase/params->period;

		switch(params->waveType)
		{
			case 0: // Square wave
				sample=phaseFraction<params->squareDuty?0.5f:-0.5f;
				break;

			case 1: // Sawtooth wave
				sample=1.0f-phaseFraction*2.0f;
				break;

			case 2: // Sine wave
				sample=sinf(phaseFraction*2.0f*PI);
				break;

			case 3: // Noise wave
				sample=params->noiseBuffer[params->phase*32/params->period];
				break;

			default:
				break;
		}

		// LP filter
		float prevPoint=params->fltPoint;

		params->fltWidth=clampf(params->fltWidth*params->fltWidthDerivative, 0.0f, 0.1f);

		if(params->lpfCutoff<=1.0f)
			params->fltPointDerivative+=((sample-params->fltPoint)*params->fltWidth)-(params->fltPointDerivative*params->fltDamping);
		else
		{
			params->fltPoint=sample;
			params->fltPointDerivative=0.0f;
		}

		params->fltPoint+=params->fltPointDerivative;

		// HP filter
		if(params->fltHPDamping>0.0f||params->fltHPDamping<0.0f)
			params->fltHPCutoff=clampf(params->fltHPCutoff*params->fltHPDamping, 0.00001f, 0.1f);

		params->fltHPPoint+=params->fltPoint-prevPoint-(params->fltHPPoint*params->fltHPCutoff);
		sample=params->fltHPPoint;

		// Phaser
		params->phaserBuffer[params->iPhaserPhase&1023]=sample;
		sample+=params->phaserBuffer[(params->iPhaserPhase-params->iPhase+1024)&1023];
		params->iPhaserPhase=(params->iPhaserPhase+1)&1023;

		// Final accumulation and envelope application
		superSample+=sample*params->envelopeVolume;
	}

	superSample/=8.0f;
	superSample*=0.06f;

	return superSample;
}
