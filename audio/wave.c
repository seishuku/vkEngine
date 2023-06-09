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
void Resample(void *in, int inwidth, int inrate, int inchannel, int inlength, int16_t *out, int outrate)
{
    float stepscale=(float)inrate/SAMPLE_RATE;

    uint32_t outcount=(uint32_t)(inlength/stepscale);

    for(uint32_t i=0, samplefrac=0;i<outcount; i++, samplefrac+=(int32_t)(stepscale*256))
    {
        int32_t samplel=0, sampler=0;
        int32_t srcsample=samplefrac>>8;

        if(inwidth==2)
        {
            if(inchannel==2)
            {
                samplel=((int16_t *)in)[2*srcsample+0];
                sampler=((int16_t *)in)[2*srcsample+1];
            }
            else
                samplel=((int16_t *)in)[srcsample];
        }
        else
        {
            if(inchannel==2)
            {
                samplel=(int16_t)(((int8_t *)in)[2*srcsample+0]-128)<<8;
                sampler=(int16_t)(((int8_t *)in)[2*srcsample+1]-128)<<8;
            }
            else
                samplel=(int16_t)(((int8_t *)in)[srcsample]-128)<<8;
        }

        if(inchannel==2)
        {
            out[2*i+0]=samplel;
            out[2*i+1]=sampler;
        }
        else
            out[i]=samplel;
    }
}

// Load a WAVE sound file, this should search for chunks, not blindly load.
// This also will only accept PCM audio streams and stereo, ideally stereo isn't needed
// because most sound effcts will be panned/spatialized into stereo for "3D" audio,
// but stereo is needed for the HRTF samples.
bool Audio_LoadStatic(char *Filename, Sample_t *Sample)
{
    FILE *stream=NULL;
    uint32_t riff_magic, wave_magic, fmt_magic, data_magic;
    uint16_t format;
    uint16_t channels;
    uint32_t frequency;
    uint16_t sample;
    uint32_t length;

    if((stream=fopen(Filename, "rb"))==NULL)
        return 0;

    // Header
    fread(&riff_magic, sizeof(uint32_t), 1, stream);    // RIFF magic marker ("RIFF")
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // File size

    if(riff_magic!=RIFF_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fread(&wave_magic, sizeof(uint32_t), 1, stream);    // WAVE magic marker ("WAVE")

    if(wave_magic!=WAVE_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    // Wave format block
    fread(&fmt_magic, sizeof(uint32_t), 1, stream);     // Wave format header magic marker ("fmt ")
    if(fmt_magic!=FMT_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // Format header size?

    fread(&format, sizeof(uint16_t), 1, stream);        // wFormatTag
    fread(&channels, sizeof(uint16_t), 1, stream);      // nChannels
    fread(&frequency, sizeof(uint32_t), 1, stream);     // nSamplesPerSec
    fseek(stream, sizeof(uint32_t), SEEK_CUR);          // nAvgBytesPerSec
    fseek(stream, sizeof(uint16_t), SEEK_CUR);          // nBlockAlign
    fread(&sample, sizeof(uint16_t), 1, stream);        // wBitsPerSample

    if(format!=1&&channels>2)
    {
        // Only support PCM streams and stereo
        fclose(stream);
        return 0;
    }

    fread(&data_magic, sizeof(uint32_t), 1, stream);    // Data block magic marker ("data")

    if(data_magic!=DATA_MAGIC)
    {
        fclose(stream);
        return 0;
    }

    fread(&length, sizeof(uint32_t), 1, stream);        // Length of data block

    int16_t *buffer=(int16_t *)Zone_Malloc(Zone, length);

    if(buffer==NULL)
    {
        fclose(stream);
        return 0;
    }

    fread(buffer, 1, length, stream);

    fclose(stream);

    length/=sample>>3;
    length/=channels;

    // Covert to match primary buffer sampling rate
    uint32_t outputSize=(uint32_t)(length/((float)frequency/SAMPLE_RATE));

    int16_t *resampled=(int16_t *)Zone_Malloc(Zone, sizeof(int16_t)*outputSize*channels);

    if(resampled==NULL)
    {
        Zone_Free(Zone, buffer);
        return 0;
    }

    Resample(buffer, sample>>3, frequency, channels, length, resampled, SAMPLE_RATE);

    Zone_Free(Zone, buffer);

    Sample->data=resampled;
    Sample->len=outputSize;
    Sample->pos=0;
    Sample->channels=(uint8_t)channels;
    Sample->xyz=Vec3b(0.0f);

    return 1;
}
