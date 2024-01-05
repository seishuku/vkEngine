#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "audio.h"

#define RIFF_MAGIC ('R'|('I'<<8)|('F'<<16)|('F'<<24))
#define WAVE_MAGIC ('W'|('A'<<8)|('V'<<16)|('E'<<24))
#define FMT_MAGIC ('f'|('m'<<8)|('t'<<16)|(' '<<24))
#define DATA_MAGIC ('d'|('a'<<8)|('t'<<16)|('a'<<24))

// Simple resampling function, based off of what id Software used in Quake, seems to work well enough.
static void Resample(const void *in, const int inWidth, const int inRate, const int inChannel, const int inLength, int16_t *out)
{
    const float stepScale=(float)inRate/AUDIO_SAMPLE_RATE;
    const uint32_t outCount=(uint32_t)(inLength/stepScale);

    for(uint32_t i=0, sampleFrac=0;i<outCount;i++, sampleFrac+=(int32_t)(stepScale*256))
    {
        const int32_t srcSample=sampleFrac>>8;
        int32_t sampleL=0, sampleR=0;

        if(inWidth==2)
        {
            if(inChannel==2)
            {
                sampleL=((int16_t *)in)[2*srcSample+0];
                sampleR=((int16_t *)in)[2*srcSample+1];
            }
            else
                sampleL=((int16_t *)in)[srcSample];
        }
        else
        {
            if(inChannel==2)
            {
                sampleL=(int16_t)(((int8_t *)in)[2*srcSample+0]-128)<<8;
                sampleR=(int16_t)(((int8_t *)in)[2*srcSample+1]-128)<<8;
            }
            else
                sampleL=(int16_t)(((int8_t *)in)[srcSample]-128)<<8;
        }

        if(inChannel==2)
        {
            out[2*i+0]=sampleL;
            out[2*i+1]=sampleR;
        }
        else
            out[i]=sampleL;
    }
}

// Load a WAVE sound file, this should search for chunks, not blindly load.
// This also will only accept PCM audio streams and stereo, ideally stereo isn't needed
// because most sound effcts will be panned/spatialized into stereo for "3D" audio,
// but stereo is needed for the HRTF samples.
bool Audio_LoadStatic(const char *filename, Sample_t *sample)
{
    FILE *stream=NULL;
    uint32_t riffMagic, waveMagic, fmtMagic, dataMagic;
    uint16_t format;
    uint16_t channels;
    uint32_t samplesPerSec;
    uint16_t bitPerSample;
    uint32_t length;

    if((stream=fopen(filename, "rb"))==NULL)
        return 0;

    // Header
    fread(&riffMagic, sizeof(uint32_t), 1, stream);    // RIFF magic marker ("RIFF")
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // File size

    if(riffMagic!=RIFF_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    // WAVE magic marker ("WAVE")
    fread(&waveMagic, sizeof(uint32_t), 1, stream);

    if(waveMagic!=WAVE_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    // Wave format header magic marker ("fmt ")
    fread(&fmtMagic, sizeof(uint32_t), 1, stream);

    if(fmtMagic!=FMT_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // Format header size?

    fread(&format, sizeof(uint16_t), 1, stream);        // wFormatTag
    fread(&channels, sizeof(uint16_t), 1, stream);      // nChannels
    fread(&samplesPerSec, sizeof(uint32_t), 1, stream);     // nSamplesPerSec
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // nAvgBytesPerSec
    fseek(stream, sizeof(uint16_t), SEEK_CUR);          // nBlockAlign
    fread(&bitPerSample, sizeof(uint16_t), 1, stream);  // wBitsPerSample

    // Only support PCM streams and stereo
    if(format!=1&&channels>2)
    {
        fclose(stream);
        return 0;
    }

    // Data block magic marker ("data")
    fread(&dataMagic, sizeof(uint32_t), 1, stream);

    if(dataMagic!=DATA_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    // Length of data block
    fread(&length, sizeof(uint32_t), 1, stream);

    int16_t *buffer=(int16_t *)Zone_Malloc(zone, length);

    if(buffer==NULL)
    {
        fclose(stream);
        return 0;
    }

    fread(buffer, 1, length, stream);

    fclose(stream);

    length/=bitPerSample>>3;
    length/=channels;

    // Covert to match primary buffer sampling rate
    const uint32_t outputSize=(uint32_t)(length/((float)samplesPerSec/AUDIO_SAMPLE_RATE));

    int16_t *resampled=(int16_t *)Zone_Malloc(zone, sizeof(int16_t)*outputSize*channels);

    if(resampled==NULL)
    {
        Zone_Free(zone, buffer);
        return 0;
    }

    Resample(buffer, bitPerSample>>3, samplesPerSec, channels, length, resampled);

    Zone_Free(zone, buffer);

    sample->data=resampled;
    sample->length=outputSize;
    sample->position=0;
    sample->channels=(uint8_t)channels;
    sample->xyz=Vec3b(0.0f);

    return 1;
}

static const float speedRatio=100.0f;

void ResetWaveSample(WaveParams_t *params)
{
	// Minimum frequency can't be higher than start frequency
	params->minFrequency=min(params->startFrequency, params->minFrequency);

	// Slide can't be less than delta slide
	params->slide=max(params->deltaSlide, params->slide);

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
	params->fltDamping=min(0.8f, 5.0f/(1.0f+(params->lpfResonance*params->lpfResonance)*20.0f)*(0.01f+params->fltWidth));
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
	params->envelopeLength[0]=(int32_t)max(1, params->attackTime*params->attackTime*100000.0f);
	params->envelopeLength[1]=(int32_t)max(1, params->sustainTime*params->sustainTime*100000.0f);
	params->envelopeLength[2]=(int32_t)max(1, params->decayTime*params->decayTime*100000.0f);
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
	params->fPeriod=min(params->fMaxPeriod, params->fPeriod*params->fSlide);

	if(params->vibAmplitude>0.0f)
	{
		params->vibPhase+=params->vibSpeed;
		params->period=max(8, (int)(params->fPeriod*(1.0+sinf(params->vibPhase)*params->vibAmplitude)));
	}
	else
		params->period=max(8, (int)params->fPeriod);

	params->squareDuty+=params->squareSlide;
	params->squareDuty=clamp(params->squareDuty, 0.0f, 0.5f);

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
			params->envelopeLength[0]=(int32_t)max(1, params->attackTime*params->attackTime*100000.0f);
			params->envelopeLength[1]=(int32_t)max(1, params->sustainTime*params->sustainTime*100000.0f);
			params->envelopeLength[2]=(int32_t)max(1, params->decayTime*params->decayTime*100000.0f);
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

		params->fltWidth=clamp(params->fltWidth*params->fltWidthDerivative, 0.0f, 0.1f);

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
			params->fltHPCutoff=clamp(params->fltHPCutoff*params->fltHPDamping, 0.00001f, 0.1f);

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
