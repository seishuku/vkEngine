// TODO: Sounds need positional and ID info for tracking through the channels.

#ifdef ANDROID
#include <aaudio/AAudio.h>
#else
#include <portaudio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
#include <memory.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "audio.h"

#include "../font/font.h"
extern Font_t Fnt;

#ifdef ANDROID
AAudioStream *stream=NULL;
#else
PaStream *stream=NULL;
#endif

#define MAX_VOLUME 255
#define MAX_CHANNELS 128

typedef struct
{
	int16_t *data;
	uint32_t pos, len;
	bool looping;
	float vol;
	vec3 *xyz;
	int16_t working[NUM_SAMPLES*2];
} Channel_t;

static vec3 Zero={ 0.0f, 0.0f, 0.0f };

Channel_t channels[MAX_CHANNELS];

// HRTF samples and buffers for interpolated values
extern HRIR_Sphere_t Sphere;
float hrir_l[NUM_SAMPLES], hrir_r[NUM_SAMPLES];
int16_t sample_l[NUM_SAMPLES*2], sample_r[NUM_SAMPLES*2];

extern Camera_t Camera;

vec3 GetChannelPosition(void)
{
	return *channels[0].xyz;
}

static const float interpolateSamples(const float h0, const float h1, const float h2, const float alpha, const float beta, const float gamma)
{
	return alpha*h0+beta*h1+gamma*h2;
}

static vec2 Barycentric(const vec3 p, const vec3 a, const vec3 b, const vec3 c)
{
	const vec3 v0=Vec3_Subv(b, a), v1=Vec3_Subv(c, a), v2=Vec3_Subv(p, a);

	const float d00=Vec3_Dot(v0, v0);
	const float d01=Vec3_Dot(v0, v1);
	const float d11=Vec3_Dot(v1, v1);
	const float d20=Vec3_Dot(v2, v0);
	const float d21=Vec3_Dot(v2, v1);
	const float invDenom=1.0f/(d00*d11-d01*d01);

	return (vec2) { (d11*d20-d01*d21)*invDenom, (d00*d21-d01*d20) *invDenom };
}

static void hrir_interpolate(vec3 xyz)
{
	// Sound distance drop-off
	const float dist_mult=1.0f/500.0f;

	// Calculate relative position of the sound source to the camera
	const vec3 relative_pos=Vec3_Subv(xyz, Camera.Position);
	vec3 position=Vec3(
		Vec3_Dot(relative_pos, Camera.Right),
		Vec3_Dot(relative_pos, Camera.Up),
		Vec3_Dot(relative_pos, Camera.Forward)
	);

	// Normalize the vector, also returns the length
	//     used for calculating distance fall-off later.
	float dist=Vec3_Normalize(&position)*dist_mult;

	// TODO: Fix in the HRIR sphere generation tool:
	position=Matrix3x3MultVec3(position, MatrixRotate(deg2rad(90.0f), 0.0f, 1.0f, 0.0f));

	// Clamp to full volume
	if(dist<0.0f)
		dist=0.0f;

#if 1
	float maxDistanceSq=-1.0f;
	int32_t triangleIndex=-1;

	for(uint32_t i=0;i<Sphere.NumIndex;i+=3)
	{
		// Calculate the normal vector of the current triangle
		// TODO: PRECALCULATE NORMALS
		const HRIR_Vertex_t *v0=&Sphere.Vertices[Sphere.Indices[i+0]];
		const HRIR_Vertex_t *v1=&Sphere.Vertices[Sphere.Indices[i+1]];
		const HRIR_Vertex_t *v2=&Sphere.Vertices[Sphere.Indices[i+2]];
		const vec3 e0=Vec3_Subv(v1->Vertex, v0->Vertex);
		const vec3 e1=Vec3_Subv(v2->Vertex, v0->Vertex);
		vec3 normal=Vec3_Cross(e0, e1);
		Vec3_Normalize(&normal);

		const float distanceSq=Vec3_Dot(position, normal);

		if(distanceSq>maxDistanceSq)
		{
			maxDistanceSq=distanceSq;
			triangleIndex=i;
		}
	}

	const HRIR_Vertex_t *v0=&Sphere.Vertices[Sphere.Indices[triangleIndex+0]];
	const HRIR_Vertex_t *v1=&Sphere.Vertices[Sphere.Indices[triangleIndex+1]];
	const HRIR_Vertex_t *v2=&Sphere.Vertices[Sphere.Indices[triangleIndex+2]];
	const vec2 g=Vec2_Clamp(Barycentric(position, v0->Vertex, v1->Vertex, v2->Vertex), 0.0f, 1.0f);
	const float det=1.0f-g.x-g.y;

	if(g.x>=0.0f&&g.y>=0.0f&&det>=0.0f)
	{
		for(uint32_t j=0;j<Sphere.SampleLength;j++)
		{
			hrir_l[j]=(1.0f-dist)*interpolateSamples(v0->Left[j], v1->Left[j], v2->Left[j], g.x, g.y, det);
			hrir_r[j]=(1.0f-dist)*interpolateSamples(v0->Right[j], v1->Right[j], v2->Right[j], g.x, g.y, det);
		}
	}
#else
	// This is very naive, it just interpolates *all* the HRIR positions and weights them according to whatever is closest to the direction.
	for(uint32_t i=0;i<Sphere.SampleLength;i++)
	{
		float hrirSum_l=0.0f;
		float hrirSum_r=0.0f;
		float weightSum=0.0f;

		// Calculate the weight of the nearest HRIR sphere vertex to the relative sound position
		for(uint32_t j=0;j<Sphere.NumVertex;j++)
		{
			const vec3 RelPos=Vec3_Subv(Sphere.Vertices[j].Vertex, position);
			const float distanceSq=Vec3_Dot(RelPos, RelPos);

			// Only weight based on vertices that are contributing meaningful data
			if(distanceSq<1.0f)
			{
				const float weight=expf(-distanceSq);

				hrirSum_l+=Sphere.Vertices[j].Left[i]*weight;
				hrirSum_r+=Sphere.Vertices[j].Right[i]*weight;

				weightSum+=weight;
			}
		}

		// Blend the HRIR sample according to the calculated weight and apply the distance fall-off
		weightSum=1.0f/weightSum;
		hrir_l[i]=(1.0f-dist)*(hrirSum_l*weightSum);
		hrir_r[i]=(1.0f-dist)*(hrirSum_r*weightSum);
	}
#endif
}

static void Convolve(int16_t *input, int16_t *audio_l, int16_t *audio_r, size_t audio_len, float *kernel_l, float *kernel_r, size_t kernel_len)
{
	for(size_t i=0;i<audio_len+kernel_len-1;i++)
	{
		float sum_l=0.0f;
		float sum_r=0.0f;

		for(size_t j=0;j<kernel_len;j++)
		{
			if(i-j>=0&&i-j<audio_len)
			{
				sum_l+=((float)input[i-j]/INT16_MAX)*kernel_l[j];
				sum_r+=((float)input[i-j]/INT16_MAX)*kernel_r[j];
			}
		}

		audio_l[i]=(int16_t)(sum_l*INT16_MAX);
		audio_r[i]=(int16_t)(sum_r*INT16_MAX);
	}
}

static void MixAudio(int16_t *dst, const int16_t *src_l, const int16_t *src_r, uint32_t len, int volume)
{
	if(volume==0)
		return;

	int16_t src;

	for(size_t i=0;i<len;i++)
	{
		size_t dstIdxL=2*i+0;
		src=(src_l[i]*volume)/128;
		dst[dstIdxL]=min(max(src+dst[dstIdxL], INT16_MIN), INT16_MAX);

		size_t dstIdxR=2*i+1;
		src=(src_r[i]*volume)/128;
		dst[dstIdxR]=min(max(src+dst[dstIdxR], INT16_MIN), INT16_MAX);
	}
}

static void Audio_FillBuffer(void *Buffer, uint32_t Length)
{
	// Get pointer to output buffer.
	int16_t *out=(int16_t *)Buffer;

	// Clear the output buffer, so we don't get annoying repeating samples.
	for(size_t dataIdx=0;dataIdx<Length*2;dataIdx++)
		out[dataIdx]=0;

	for(uint32_t i=0;i<MAX_CHANNELS;i++)
	{
		// Quality of life pointer to current mixing channel.
		Channel_t *channel=&channels[i];

		// If the channel is empty, skip on the to next.
		if(!channel->data)
			continue;

		// Interpolate HRIR samples that are closest to the sound's position
		// TODO: this needs work, it works, but not great
		hrir_interpolate(*channel->xyz);

		// Calculate the remaining amount of data to process.
		size_t remaining_data=channel->len-channel->pos;

		// Remaining data runs off end of primary buffer.
		// Clamp it to buffer size, we'll get the rest later.
		if(remaining_data>=Length)
			remaining_data=Length;

		// Calculate the amount to fill the convolution buffer.
		// The convolve buffer needs to be at least NUM_SAMPLE+HRIR length,
		//   but to stop annoying pops/clicks and other discontinuities, we need to copy ahead,
		//   which is either the full input sample length OR the full buffer+HRIR sample length.
		size_t toFill=(channel->len-channel->pos);

		if(toFill>=(NUM_SAMPLES+Sphere.SampleLength))
			toFill=(NUM_SAMPLES+Sphere.SampleLength);
		else if(toFill>=channel->len)
			toFill=channel->len;

		// Copy the samples.
		for(size_t dataIdx=0;dataIdx<(NUM_SAMPLES*2);dataIdx++)
		{
			if(dataIdx<=toFill)
				channel->working[dataIdx]=channel->data[channel->pos+dataIdx];
			else
			{
				// Zero out the remaining buffer size.
				channel->working[dataIdx]=0;
			}
		}

		// Convolve the samples with the interpolated HRIR sample to produce a stereo sample to mix into the output buffer
		Convolve(channel->working, sample_l, sample_r, remaining_data, hrir_l, hrir_r, Sphere.SampleLength);

		// Mix out the samples into the output buffer
		MixAudio(out, sample_l, sample_r, (uint32_t)remaining_data, 128);

		// Advance the sample position by what we've used, next time around will take another chunk.
		channel->pos+=(uint32_t)remaining_data;

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
}

// Callback functions for when audio driver needs more data.
#ifdef ANDROID
static aaudio_data_callback_result_t Audio_Callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames)
{
	Audio_FillBuffer(audioData, numFrames);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
#else
static int Audio_Callback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	Audio_FillBuffer(outputBuffer, framesPerBuffer);

	return paContinue;
}
#endif

// Add a sound to first open channel.
void Audio_PlaySample(Sample_t *Sample, bool looping, float vol, vec3 *pos)
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

	if(pos)
		channels[index].xyz=pos;
	else
		channels[index].xyz=&Sample->xyz;

	channels[index].vol=min(1.0f, max(0.0f, vol));
}

void Audio_StopSample(Sample_t *Sample)
{
	int32_t index;

	// Search for the sample in the channels list
	for(index=0;index<MAX_CHANNELS;index++)
	{
		if(channels[index].data==Sample->data)
			break;
	}

	// return if it didn't find the sample
	if(index>=MAX_CHANNELS)
		return;

	// Set the position to the end and allow the callback to resolve the removal
	channels[index].pos=channels[index].len-1;
	channels[index].looping=false;
}

int Audio_Init(void)
{
	// Clear out mixing channels
	for(int i=0;i<MAX_CHANNELS;i++)
	{
		channels[i].data=NULL;
		channels[i].pos=0;
		channels[i].len=0;
		channels[i].looping=false;
		channels[i].xyz=&Zero;
	}

	if(!HRIR_Init())
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: HRIR failed to initialize.\n");
		return false;
	}

#ifdef ANDROID
	AAudioStreamBuilder *streamBuilder;

	if(AAudio_createStreamBuilder(&streamBuilder)!=AAUDIO_OK)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Error creating stream builder\n");
		return false;
	}

	AAudioStreamBuilder_setFormat(streamBuilder, AAUDIO_FORMAT_PCM_I16);
	AAudioStreamBuilder_setChannelCount(streamBuilder, 2);
	AAudioStreamBuilder_setSampleRate(streamBuilder, SAMPLE_RATE);
	AAudioStreamBuilder_setPerformanceMode(streamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setDataCallback(streamBuilder, Audio_Callback, NULL);

	// Opens the stream.
	if(AAudioStreamBuilder_openStream(streamBuilder, &stream)!=AAUDIO_OK)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Error opening stream\n");
		return false;
	}

	if(AAudioStream_getSampleRate(stream)!=SAMPLE_RATE)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Sample rate mismatch\n");
		return false;
	}

	// Sets the buffer size. 
	AAudioStream_setBufferSizeInFrames(stream, AAudioStream_getFramesPerBurst(stream)*NUM_SAMPLES);

	// Starts the stream.
	if(AAudioStream_requestStart(stream)!=AAUDIO_OK)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Error starting stream\n");
		return false;
	}

	AAudioStreamBuilder_delete(streamBuilder);
#else
	// Initialize PortAudio
	if(Pa_Initialize()!=paNoError)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: PortAudio failed to initialize.\n");
		return false;
	}

	PaStreamParameters outputParameters;
	// Set up output device parameters
	outputParameters.device=Pa_GetDefaultOutputDevice();

	if(outputParameters.device==paNoDevice)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: No default output device.\n");
		Pa_Terminate();
		return false;
	}

	outputParameters.channelCount=2;
	outputParameters.sampleFormat=paInt16;
	outputParameters.suggestedLatency=Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo=NULL;

	// Open audio stream
	if(Pa_OpenStream(&stream, NULL, &outputParameters, SAMPLE_RATE, NUM_SAMPLES, paNoFlag, Audio_Callback, NULL)!=paNoError)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Unable to open PortAudio stream.\n");
		Pa_Terminate();
		return false;
	}

	// Start audio stream
	if(Pa_StartStream(stream)!=paNoError)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Unable to start PortAudio Stream.\n");
		Pa_Terminate();
		return false;
	}
#endif

	return true;
}

void Audio_Destroy(void)
{
	// Clean up HRIR data
	Zone_Free(Zone, Sphere.Indices);
	Zone_Free(Zone, Sphere.Vertices);

#ifdef ANDROID
	AAudioStream_requestStop(stream);
	AAudioStream_close(stream);
#else
	// Shut down PortAudio
	Pa_AbortStream(stream);
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
#endif
}
