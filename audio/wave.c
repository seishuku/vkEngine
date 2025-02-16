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
enum : uint32_t {
	RIFF_MAGIC='R'|'I'<<8|'F'<<16|'F'<<24,
	WAVE_MAGIC='W'|'A'<<8|'V'<<16|'E'<<24,
	FMT_MAGIC ='f'|'m'<<8|'t'<<16|' '<<24,
	DATA_MAGIC='d'|'a'<<8|'t'<<16|'a'<<24
};

typedef struct RIFFChunk_s
{
	uint32_t magic;
	uint32_t size;
} RIFFChunk_t;

void *WavRead(const char *filename, WaveFormat_t *format, uint32_t *numSamples)
{
	FILE *stream=NULL;
	RIFFChunk_t chunk={ 0 };
	uint32_t waveMagic=0;
	uint32_t dataSize=0;
	uint8_t *buffer=NULL;
	bool foundFormat=false, foundData=false;

	if(filename==NULL||format==NULL||numSamples==NULL)
		return NULL;

	stream=fopen(filename, "rb");
   
	if(stream==NULL)
		return NULL;

	// RIFF header chunk, check chunk magic marker and make sure file size matches the chunk size
	if(fread(&chunk, sizeof(RIFFChunk_t), 1, stream)!=1||chunk.magic!=RIFF_MAGIC)
		goto error;

	// WAVE magic marker ("WAVE") should follow after the RIFF chunk
	if(fread(&waveMagic, sizeof(uint32_t), 1, stream)!=1&&waveMagic!=WAVE_MAGIC)
		goto error;

	while(!(foundFormat&&foundData))
	{
		// Read in a chunk to process
		if(!fread(&chunk, sizeof(RIFFChunk_t), 1, stream))
			goto error;

		switch(chunk.magic)
		{
			case FMT_MAGIC:
			{
				if(fread(format, sizeof(WaveFormat_t), 1, stream)!=1)
					goto error;

				// Only support PCM streams and up to 2 channels
				if(!(format->formatTag==WAVE_FORMAT_PCM||format->formatTag==WAVE_FORMAT_IEEE_FLOAT)||format->channels>2)
					goto error;

				foundFormat=true;
				break;
			}

			case DATA_MAGIC:
			{
				buffer=(uint8_t *)Zone_Malloc(zone, chunk.size);

				if(buffer==NULL)
					goto error;

				// Read in audio data
				if(fread(buffer, 1, chunk.size, stream)!=chunk.size)
					goto error;

				dataSize=chunk.size;
				foundData=true;
				break;
			}

			default:
				fseek(stream, chunk.size, SEEK_CUR);
				break;
		}
	}

	if(foundFormat&&foundData)
	{
		fclose(stream);
		*numSamples=dataSize/(format->bitsPerSample>>3)/format->channels;

		return buffer;
	}

error:
	if(stream)
		fclose(stream);

	if(buffer)
		Zone_Free(zone, buffer);

	return NULL;
}

uint32_t WavWrite(const char *filename, int16_t *samples, uint32_t numSamples, uint32_t sampleRate, uint16_t channels)
{
	uint32_t dataSize=numSamples*channels*sizeof(int16_t);
	const uint16_t bitsPerSample=16;

	FILE *stream=fopen(filename, "wb");

	if(stream==NULL)
		return 0;

	// Write RIFF chunk
	RIFFChunk_t RIFF={ RIFF_MAGIC, dataSize+44-8 };
	fwrite(&RIFF, sizeof(RIFFChunk_t), 1, stream);

	// Write WAVE and Format chunks
	fwrite(&(uint32_t[2]){WAVE_MAGIC, FMT_MAGIC}, sizeof(uint32_t), 2, stream);

	uint32_t fmtSize=sizeof(WaveFormat_t);
	fwrite(&fmtSize, sizeof(uint32_t), 1, stream);
	WaveFormat_t format=
	{
		.formatTag    =WAVE_FORMAT_PCM,
		.channels     =channels,
		.samplesPerSec=sampleRate,
		.bytesPerSec  =channels*sampleRate*bitsPerSample>>3,
		.blockAlign   =channels*bitsPerSample>>3,
		.bitsPerSample=bitsPerSample
	};
	fwrite(&format, sizeof(WaveFormat_t), 1, stream);

	// Write data chunk
	fwrite(&(uint32_t){DATA_MAGIC}, sizeof(uint32_t), 1, stream);
	fwrite(&dataSize, sizeof(uint32_t), 1, stream);
	fwrite(samples, dataSize, 1, stream);

	fclose(stream);

	return dataSize+44-8;
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
