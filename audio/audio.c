// TODO: Sounds need positional and ID info for tracking through the channels.

#include <portaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <memory.h>
#include "../system/system.h"
#include "../math/math.h"
#include "audio.h"

PaStream *stream=NULL;

#define MAX_VOLUME 255
#define MAX_CHANNELS 128

typedef struct
{
    int16_t *data;
    uint32_t pos, len;
    bool looping;
    vec3 *xyz;
} Channel_t;

vec3 Zero={ 0.0f, 0.0f, 0.0f };

Channel_t channels[MAX_CHANNELS];

// Position spatializion/stereo separation.
// Largely base off of what idSoftware used in the Quake engined games.
vec3 listener_origin={ 0.0f, 0.0f, 0.0f };
vec3 listener_right={ 1.0f, 0.0f, 0.0f };

void Audio_SetListenerOrigin(vec3 pos, vec3 right)
{
    Vec3_Setv(&listener_origin, pos);
    Vec3_Setv(&listener_right, right);
}

void Spatialize(vec3 origin, uint8_t *left_vol, uint8_t *right_vol)
{
    vec3 source_vec;

    const float dist_mult=1.0f/500.0f;

    // Distance from listener to sound source
    Vec3_Setv(&source_vec, origin);
    Vec3_Subv(&source_vec, listener_origin);

    float dist=(Vec3_Normalize(&source_vec)-MAX_VOLUME)*dist_mult;

    // Clamp to full volume
    if(dist<0.0f)
        dist=0.0f;

    // Calc angle from listener to sound source
    const float dot=Vec3_Dot(listener_right, source_vec);

    // Factor scale for left and right channels
    const float lscale=0.5f*(1.0f-dot);
    const float rscale=0.5f*(1.0f+dot);

    // Distance attenuation
    *left_vol=(uint8_t)(max((1.0f-dist)*lscale, 0.0f)*MAX_VOLUME);
    *right_vol=(uint8_t)(max((1.0f-dist)*rscale, 0.0f)*MAX_VOLUME);
}

// Mix samples
// result=(a+b)-(a*b)
// https://atastypixel.com/how-to-mix-audio-samples-properly-on-ios/
int16_t MixSamples(int16_t a, int16_t b, uint8_t volume)
{
    b=(b*volume)/MAX_VOLUME;

    if(a<0&&b<0)
        return (a+b)-((a*b)/INT16_MIN);
    else if(a>0&&b>0)
        return (a+b)-((a*b)/INT16_MAX);

    // Why isn't this clipping now, it was before?
    return a+b;
}

// Callback function for when PortAudio needs more data.
int paCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
    // Get pointer to output buffer.
    int16_t *out=(int16_t *)outputBuffer;

    // Clear the output buffer, so we don't get annoying repeating samples.
    memset(out, 0, framesPerBuffer*sizeof(int16_t)*2);

    for(int32_t i=0;i<MAX_CHANNELS;i++)
    {
        // Quality of life pointer to current mixing channel.
        Channel_t *channel=&channels[i];

        // If the channel is empty, skip on the to next.
        if(!channel->data)
            continue;

        // Calculate the remaining amount of data to process.
        uint32_t remaining_data=(channel->len-channel->pos);

        // Remaining data runs off end of primary buffer.
        // Clamp it to buffer size, we'll get the rest later.
        if(remaining_data>framesPerBuffer)
            remaining_data=framesPerBuffer;

        // Transfer/process what we can.
        for(uint32_t j=0;j<remaining_data;j++)
        {
            int16_t input_sample=channel->data[channel->pos+j];
            int16_t output_sampleL=*out+0;
            int16_t output_sampleR=*out+1;
            uint8_t left=0, right=0;

            Spatialize(*channel->xyz, &left, &right);

            *out++=MixSamples(output_sampleL, input_sample, left);
            *out++=MixSamples(output_sampleR, input_sample, right);
        }

        // Advance the sample position by what we've used, next time around will take another chunk.
        channel->pos+=remaining_data;

        // Reset output buffer pointer for next channel to process.
        out=(int16_t *)outputBuffer;

        // If loop flag was set, reset position to 0 if it's at the end.
        if(channel->pos==channel->len)
        {
            if(channel->looping)
                channel->pos=0;
            else
            {
                // DO STUFF TO REMOVE
                channel->data=NULL;
                channel->pos=0;
                channel->len=0;
                channel->looping=false;
                channel->xyz=&Zero;
            }
        }
    }

    return paContinue;
}

// Add a sound to first open channel.
void Audio_PlaySample(Sample_t *Sample, bool looping)
{
    int32_t index;

    // Look for an empty sound channel slot.
    for(index=0;index<MAX_CHANNELS;index++)
    {
        // If it's either done playing or is still the initial zero.
        if(channels[index].pos==channels[index].len)
            break;
    }

    // return if there aren't any channels available.
    if(index>=MAX_CHANNELS)
        return;

    // otherwise set the channel's data pointer to this sample's pointer
    // and set the length, reset play position, and loop flag.
    channels[index].data=Sample->data;
    channels[index].len=Sample->len;
    channels[index].pos=0;
    channels[index].looping=looping;
    channels[index].xyz=&Sample->xyz;
}

int Audio_Init(void)
{
    PaStreamParameters outputParameters;

    // Clear out mixing channels
    for(int i=0;i<MAX_CHANNELS;i++)
    {
        channels[i].data=NULL;
        channels[i].pos=0;
        channels[i].len=0;
        channels[i].looping=false;
        channels[i].xyz=&Zero;
    }

    // Initialize PortAudio
    if(Pa_Initialize()!=paNoError)
    {
        DBGPRINTF("Audio: PortAudio failed to initialize.\n");
        return false;
    }

    // Set up output device parameters
    outputParameters.device=Pa_GetDefaultOutputDevice();

    if(outputParameters.device==paNoDevice)
    {
        DBGPRINTF("Audio: No default output device.\n");
        Pa_Terminate();
        return false;
    }

    outputParameters.channelCount=2;
    outputParameters.sampleFormat=paInt16;
    outputParameters.suggestedLatency=Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo=NULL;

    // Open audio stream
    if(Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, NUM_SAMPLES, paNoFlag, paCallback, NULL)!=paNoError)
    {
        DBGPRINTF("Audio: Unable to open PortAudio stream.\n");
        Pa_Terminate();
        return false;
    }

    // Start audio stream
    if(Pa_StartStream(stream)!=paNoError)
    {
        DBGPRINTF("Audio: Unable to start PortAudio Stream.\n");
        Pa_Terminate();
        return false;
    }

    return true;
}

void Audio_Destroy(void)
{
    // Shut down PortAudio
    Pa_AbortStream(stream);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
}
