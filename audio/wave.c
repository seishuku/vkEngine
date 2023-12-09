#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "../system/system.h"
#include "../math/math.h"
#include "audio.h"

#define RIFF_MAGIC ('R'|('I'<<8)|('F'<<16)|('F'<<24))
#define WAVE_MAGIC ('W'|('A'<<8)|('V'<<16)|('E'<<24))
#define FMT_MAGIC ('f'|('m'<<8)|('t'<<16)|(' '<<24))
#define DATA_MAGIC ('d'|('a'<<8)|('t'<<16)|('a'<<24))

// Simple resampling function, based off of what id Software used in Quake, seems to work well enough.
static void Resample(const void *in, const int inWidth, const int inRate, const int inChannel, const int inLength, int16_t *out, const int outRate)
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
bool Audio_LoadStatic(const char *Filename, Sample_t *Sample)
{
    FILE *Stream=NULL;
    uint32_t riffMagic, waveMagic, fmtMagic, dataMagic;
    uint16_t Format;
    uint16_t Channels;
    uint32_t samplesPerSec;
    uint16_t bitPerSample;
    uint32_t Length;

    if((Stream=fopen(Filename, "rb"))==NULL)
        return 0;

    // Header
    fread(&riffMagic, sizeof(uint32_t), 1, Stream);    // RIFF magic marker ("RIFF")
    fseek(Stream, sizeof(uint32_t), SEEK_CUR);          // File size

    if(riffMagic!=RIFF_MAGIC)
    {
        fclose(Stream);
        return 0;
    }

    // WAVE magic marker ("WAVE")
    fread(&waveMagic, sizeof(uint32_t), 1, Stream);

    if(waveMagic!=WAVE_MAGIC)
    {
        fclose(Stream);
        return 0;
    }

    // Wave format header magic marker ("fmt ")
    fread(&fmtMagic, sizeof(uint32_t), 1, Stream);

    if(fmtMagic!=FMT_MAGIC)
    {
        fclose(Stream);
        return 0;
    }

    fseek(Stream, sizeof(uint32_t), SEEK_CUR);          // Format header size?

    fread(&Format, sizeof(uint16_t), 1, Stream);        // wFormatTag
    fread(&Channels, sizeof(uint16_t), 1, Stream);      // nChannels
    fread(&samplesPerSec, sizeof(uint32_t), 1, Stream);     // nSamplesPerSec
    fseek(Stream, sizeof(uint32_t), SEEK_CUR);          // nAvgBytesPerSec
    fseek(Stream, sizeof(uint16_t), SEEK_CUR);          // nBlockAlign
    fread(&bitPerSample, sizeof(uint16_t), 1, Stream);  // wBitsPerSample

    // Only support PCM streams and stereo
    if(Format!=1&&Channels>2)
    {
        fclose(Stream);
        return 0;
    }

    // Data block magic marker ("data")
    fread(&dataMagic, sizeof(uint32_t), 1, Stream);

    if(dataMagic!=DATA_MAGIC)
    {
        fclose(Stream);
        return 0;
    }

    // Length of data block
    fread(&Length, sizeof(uint32_t), 1, Stream);

    int16_t *Buffer=(int16_t *)Zone_Malloc(Zone, Length);

    if(Buffer==NULL)
    {
        fclose(Stream);
        return 0;
    }

    fread(Buffer, 1, Length, Stream);

    fclose(Stream);

    Length/=bitPerSample>>3;
    Length/=Channels;

    // Covert to match primary buffer sampling rate
    const uint32_t outputSize=(uint32_t)(Length/((float)samplesPerSec/AUDIO_SAMPLE_RATE));

    int16_t *Resampled=(int16_t *)Zone_Malloc(Zone, sizeof(int16_t)*outputSize*Channels);

    if(Resampled==NULL)
    {
        Zone_Free(Zone, Buffer);
        return 0;
    }

    Resample(Buffer, bitPerSample>>3, samplesPerSec, Channels, Length, Resampled, AUDIO_SAMPLE_RATE);

    Zone_Free(Zone, Buffer);

    Sample->Data=Resampled;
    Sample->Length=outputSize;
    Sample->Position=0;
    Sample->Channels=(uint8_t)Channels;
    Sample->xyz=Vec3b(0.0f);

    return 1;
}
