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

#ifdef ANDROID
static AAudioStream *audioStream=NULL;
#else
static PaStream *audioStream=NULL;
#endif

// HRIR sphere model and audio samples
typedef struct
{
	vec3 vertex, normal;
	float left[MAX_HRIR_SAMPLES], right[MAX_HRIR_SAMPLES];
} HRIR_Vertex_t;

typedef struct
{
	uint32_t magic;
	uint32_t sampleRate;
	uint32_t sampleLength;
	uint32_t numVertex;
	uint32_t numIndex;
	uint32_t *indices;
	HRIR_Vertex_t *vertices;
} HRIR_Sphere_t;

static HRIR_Sphere_t sphere;

#define MAX_VOLUME 127

#define MAX_CHANNELS 32

typedef struct
{
	int16_t *data;
	uint32_t position, length;
	bool looping;
	float volume;
	vec3 *xyz;
	int16_t working[MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES];
} Channel_t;

static Channel_t channels[MAX_CHANNELS];

// HRIR interpolation buffers
static int16_t HRIRSamples[2*MAX_HRIR_SAMPLES];

// Audio buffer after HRTF convolve
static int16_t audioBuffer[2*(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES)];

// Streaming audio

typedef struct
{
	uint32_t position;
	int16_t buffer[MAX_STREAM_SAMPLES*2];

	struct
	{
		bool playing;
		float volume;
		void (*streamCallback)(void *buffer, size_t length);
	} stream[MAX_AUDIO_STREAMS];
} AudioStream_t;

static AudioStream_t streamBuffer;

extern Camera_t camera;

static vec2 CalculateBarycentric(const vec3 p, const vec3 a, const vec3 b, const vec3 c)
{
	const vec3 v0=Vec3_Subv(b, a), v1=Vec3_Subv(c, a), v2=Vec3_Subv(p, a);

	const float d00=Vec3_Dot(v0, v0);
	const float d01=Vec3_Dot(v0, v1);
	const float d11=Vec3_Dot(v1, v1);
	const float d20=Vec3_Dot(v2, v0);
	const float d21=Vec3_Dot(v2, v1);
	const float invDenom=1.0f/(d00*d11-d01*d01);

	return (vec2) { (d11*d20-d01*d21)*invDenom, (d00*d21-d01*d20)*invDenom };
}

// HRIR sample interpolation, takes world-space position as input.
// HRIR samples are taken as float, but interpolated output is int16.
static void HRIRInterpolate(vec3 xyz)
{
	// Sound distance drop-off constant, this is the radius of the hearable range
	const float invRadius=1.0f/1000.0f;

	// Calculate relative position of the sound source to the camera
	const vec3 relPosition=Vec3_Subv(xyz, camera.position);
	vec3 position=Vec3(
		Vec3_Dot(relPosition, camera.right),
		Vec3_Dot(relPosition, camera.up),
		Vec3_Dot(relPosition, camera.forward)
	);

	// Calculate distance fall-off
	float falloffDist=max(0.0f, 1.0f-Vec3_Length(Vec3_Muls(position, invRadius)));

	// Normalize also returns the length of the vector...
	Vec3_Normalize(&position);

	// Find closest triangle to the sound direction
	float maxDistanceSq=-1.0f;
	int32_t triangleIndex=-1;

	for(uint32_t i=0;i<sphere.numIndex;i+=3)
	{
		const float distanceSq=Vec3_Dot(position, sphere.vertices[sphere.indices[i+0]].normal);

		if(distanceSq>maxDistanceSq)
		{
			maxDistanceSq=distanceSq;
			triangleIndex=i;
		}
	}

	// Calculate the barycentric coordinates and use them to interpolate the HRIR samples.
	const HRIR_Vertex_t *v0=&sphere.vertices[sphere.indices[triangleIndex+0]];
	const HRIR_Vertex_t *v1=&sphere.vertices[sphere.indices[triangleIndex+1]];
	const HRIR_Vertex_t *v2=&sphere.vertices[sphere.indices[triangleIndex+2]];
	const vec2 g=Vec2_Clamp(CalculateBarycentric(position, v0->vertex, v1->vertex, v2->vertex), 0.0f, 1.0f);
	const vec3 coords=Vec3(g.x, g.y, 1.0f-g.x-g.y);

	if(coords.x>=0.0f&&coords.y>=0.0f&&coords.z>=0.0f)
	{
		for(uint32_t j=0;j<sphere.sampleLength;j++)
		{
			const vec3 left=Vec3(v0->left[j], v1->left[j], v2->left[j]);
			const vec3 right=Vec3(v0->right[j], v1->right[j], v2->right[j]);

			HRIRSamples[2*j+0]=(int16_t)(falloffDist*Vec3_Dot(left, coords)*INT16_MAX);
			HRIRSamples[2*j+1]=(int16_t)(falloffDist*Vec3_Dot(right, coords)*INT16_MAX);
		}
	}
}

// Integer audio convolution, this is a current chokepoint in the audio system at 25% CPU usage in the profiler.
// I don't think I can optimize this any more without going to SIMD or multithreading.
static void Convolve(const int16_t *input, int16_t *output, const size_t length, const int16_t *kernel, const size_t kernelLength)
{
	int16_t *outputPtr=output;
	int32_t sum[2];

	for(size_t i=0;i<length+kernelLength-1;i++)
	{
		const int16_t *inputPtr=&input[i+kernelLength];
		const int16_t *kernelPtr=kernel;

		sum[0]=1<<14;
		sum[1]=1<<14;

		for(size_t j=0;j<kernelLength;j++)
		{
			sum[0]+=(int32_t)(*kernelPtr++)*(int32_t)(*inputPtr);
			sum[1]+=(int32_t)(*kernelPtr++)*(int32_t)(*inputPtr);
			inputPtr--;
		}

		*outputPtr++=(int16_t)(clamp(sum[0], -0x40000000, 0x3fffffff)>>15);
		*outputPtr++=(int16_t)(clamp(sum[1], -0x40000000, 0x3fffffff)>>15);
	}
}

// Additively mix source into destination
static void MixAudio(int16_t *dst, const int16_t *src, const size_t length, const int8_t volume)
{
	if(volume==0)
		return;

	for(size_t i=0;i<length*2;i+=2)
	{
		dst[i+0]=clamp(((src[i+0]*volume)/MAX_VOLUME)+dst[i+0], INT16_MIN, INT16_MAX);
		dst[i+1]=clamp(((src[i+1]*volume)/MAX_VOLUME)+dst[i+1], INT16_MIN, INT16_MAX);
	}
}

#include "../font/font.h"
extern Font_t Fnt;

static void Audio_FillBuffer(void *buffer, uint32_t length)
{
	// Get pointer to output buffer.
	int16_t *out=(int16_t *)buffer;

	// Clear the output buffer, so we don't get annoying repeating samples.
	for(size_t dataIdx=0;dataIdx<length*2;dataIdx++)
		out[dataIdx]=0;

	for(uint32_t i=0;i<MAX_CHANNELS;i++)
	{
		// Quality of life pointer to current mixing channel.
		Channel_t *channel=&channels[i];

		// If the channel is empty, skip on the to next.
		if(!channel->data)
			continue;

		// If there's a position set, use it.
		vec3 position=Vec3b(0.0f);
		if(channel->xyz)
			position=*channel->xyz;

		// Interpolate HRIR samples that are closest to the sound's position
		HRIRInterpolate(position);

		// Calculate the remaining amount of data to process.
		size_t remainingData=channel->length-channel->position;

		// Remaining data runs off end of primary buffer.
		// Clamp it to buffer size, we'll get the rest later.
		if(remainingData>=length)
			remainingData=length;

		// Calculate the amount to fill the convolution buffer.
		// The convolve buffer needs to be at least NUM_SAMPLE+HRIR length,
		//   but to stop annoying pops/clicks and other discontinuities, we need to copy ahead,
		//   which is either the full input sample length OR the full buffer+HRIR sample length.
		size_t toFill=(channel->length-channel->position);

		if(toFill>=(MAX_AUDIO_SAMPLES+sphere.sampleLength))
			toFill=(MAX_AUDIO_SAMPLES+sphere.sampleLength);
		else if(toFill>=channel->length)
			toFill=channel->length;

		// Copy the samples.
		for(size_t dataIdx=0;dataIdx<(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES);dataIdx++)
		{
			if(dataIdx<=toFill)
				channel->working[dataIdx]=channel->data[channel->position+dataIdx];
			else
			{
				// Zero out the remaining buffer size.
				channel->working[dataIdx]=0;
			}
		}

		// Convolve the samples with the interpolated HRIR sample to produce a stereo sample to mix into the output buffer
		Convolve(channel->working, audioBuffer, remainingData, HRIRSamples, sphere.sampleLength);

		// Mix out the samples into the output buffer
		MixAudio(out, audioBuffer, (uint32_t)remainingData, (int8_t)(channel->volume*MAX_VOLUME));

		// Advance the sample position by what we've used, next time around will take another chunk.
		channel->position+=(uint32_t)remainingData;

		// Reached end of audio sample, either remove from channels list, or
		//     if loop flag was set, reset position to 0.
		if(channel->position>=channel->length)
		{
			if(channel->looping)
				channel->position=0;
			else
			{
				// Remove from list
				memset(channel, 0, sizeof(Channel_t));
			}
		}
	}

	size_t remainingData=min(MAX_STREAM_SAMPLES-streamBuffer.position, length);

	for(uint32_t i=0;i<MAX_AUDIO_STREAMS;i++)
	{
		if(streamBuffer.stream[i].playing)
		{
			// If there's an assigned callback, call it to load more audio data
			if(streamBuffer.stream[i].streamCallback)
			{
				streamBuffer.stream[i].streamCallback(&streamBuffer.buffer[streamBuffer.position], remainingData);
				MixAudio(out, &streamBuffer.buffer[streamBuffer.position], remainingData, (int8_t)(streamBuffer.stream[i].volume*MAX_VOLUME));
			}
		}
	}

	streamBuffer.position+=(uint32_t)remainingData;

	if(streamBuffer.position>=MAX_STREAM_SAMPLES)
		streamBuffer.position=0;
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
void Audio_PlaySample(Sample_t *sample, const bool looping, const float volume, vec3 *position)
{
	int32_t index;

	// Look for an empty sound channel slot.
	for(index=0;index<MAX_CHANNELS;index++)
	{
		// If it's either done playing or is still the initial zero.
		if(channels[index].position==channels[index].length)
			break;
	}

	// return if there aren't any channels available.
	if(index>=MAX_CHANNELS)
		return;

	// otherwise set the channel's data pointer to this sample's pointer
	// and set the length, reset play position, and loop flag.
	channels[index].data=sample->data;
	channels[index].length=sample->length;
	channels[index].position=0;
	channels[index].looping=looping;

	if(position)
		channels[index].xyz=position;
	else
		channels[index].xyz=&sample->xyz;

	channels[index].volume=min(1.0f, max(0.0f, volume));
}

void Audio_StopSample(Sample_t *sample)
{
	int32_t index;

	// Search for the sample in the channels list
	for(index=0;index<MAX_CHANNELS;index++)
	{
		if(channels[index].data==sample->data)
			break;
	}

	// return if it didn't find the sample
	if(index>=MAX_CHANNELS)
		return;

	// Set the position to the end and allow the callback to resolve the removal
	channels[index].position=channels[index].length-1;
	channels[index].looping=false;
}

bool Audio_SetStreamCallback(uint32_t stream, void (*streamCallback)(void *buffer, size_t length))
{
	if(stream>=MAX_AUDIO_STREAMS)
		return false;

	streamBuffer.stream[stream].streamCallback=streamCallback;

	return true;
}

bool Audio_SetStreamVolume(uint32_t stream, const float volume)
{
	if(stream>=MAX_AUDIO_STREAMS)
		return false;

	streamBuffer.stream[stream].volume=min(1.0f, max(0.0f, volume));

	return true;
}

bool Audio_StartStream(uint32_t stream)
{
	if(stream>=MAX_AUDIO_STREAMS)
		return false;

	streamBuffer.stream[stream].playing=true;

	return true;
}

bool Audio_StopStream(uint32_t stream)
{
	if(stream>=MAX_AUDIO_STREAMS)
		return false;

	streamBuffer.stream[stream].playing=false;

	return true;
}

static bool HRIR_Init(void)
{
	FILE *stream=NULL;

	stream=fopen("assets/hrir_full.bin", "rb");

	if(!stream)
		return false;

	fread(&sphere, sizeof(uint32_t), 5, stream);

	if(sphere.magic!=('H'|('R'<<8)|('I'<<16)|('R'<<24)))
		return false;

	if(sphere.sampleLength>MAX_HRIR_SAMPLES)
		return false;

	sphere.indices=(uint32_t *)Zone_Malloc(zone, sizeof(uint32_t)*sphere.numIndex);

	if(sphere.indices==NULL)
		return false;

	fread(sphere.indices, sizeof(uint32_t), sphere.numIndex, stream);

	sphere.vertices=(HRIR_Vertex_t *)Zone_Malloc(zone, sizeof(HRIR_Vertex_t)*sphere.numVertex);

	if(sphere.vertices==NULL)
		return false;

	memset(sphere.vertices, 0, sizeof(HRIR_Vertex_t)*sphere.numVertex);

	for(uint32_t i=0;i<sphere.numVertex;i++)
	{
		fread(&sphere.vertices[i].vertex, sizeof(vec3), 1, stream);

		fread(sphere.vertices[i].left, sizeof(float), sphere.sampleLength, stream);
		fread(sphere.vertices[i].right, sizeof(float), sphere.sampleLength, stream);
	}

	fclose(stream);

	// Pre-calculate the normal vector of the HRIR sphere triangles
	for(uint32_t i=0;i<sphere.numIndex;i+=3)
	{
		HRIR_Vertex_t *v0=&sphere.vertices[sphere.indices[i+0]];
		HRIR_Vertex_t *v1=&sphere.vertices[sphere.indices[i+1]];
		HRIR_Vertex_t *v2=&sphere.vertices[sphere.indices[i+2]];
		vec3 normal=Vec3_Cross(Vec3_Subv(v1->vertex, v0->vertex), Vec3_Subv(v2->vertex, v0->vertex));
		Vec3_Normalize(&normal);

		v0->normal=Vec3_Addv(v0->normal, normal);
		v1->normal=Vec3_Addv(v1->normal, normal);
		v2->normal=Vec3_Addv(v2->normal, normal);
	}

	return true;
}

int Audio_Init(void)
{
	// Clear out mixing channels
	memset(channels, 0, sizeof(Channel_t)*MAX_CHANNELS);

	// Clear out stream buffer
	memset(&streamBuffer, 0, sizeof(AudioStream_t));

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
	AAudioStreamBuilder_setSampleRate(streamBuilder, AUDIO_SAMPLE_RATE);
	AAudioStreamBuilder_setPerformanceMode(streamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setDataCallback(streamBuilder, Audio_Callback, NULL);

	// Opens the stream.
	if(AAudioStreamBuilder_openStream(streamBuilder, &audioStream)!=AAUDIO_OK)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Error opening stream\n");
		return false;
	}

	if(AAudioStream_getSampleRate(audioStream)!=AUDIO_SAMPLE_RATE)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Sample rate mismatch\n");
		return false;
	}

	// Sets the buffer size. 
	AAudioStream_setBufferSizeInFrames(audioStream, AAudioStream_getFramesPerBurst(audioStream)*MAX_AUDIO_SAMPLES);

	// Starts the stream.
	if(AAudioStream_requestStart(audioStream)!=AAUDIO_OK)
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

	PaStreamParameters outputParameters={ 0 };
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
	if(Pa_OpenStream(&audioStream, NULL, &outputParameters, AUDIO_SAMPLE_RATE, MAX_AUDIO_SAMPLES, paNoFlag, Audio_Callback, NULL)!=paNoError)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Unable to open PortAudio stream.\n");
		Pa_Terminate();
		return false;
	}

	// Start audio stream
	if(Pa_StartStream(audioStream)!=paNoError)
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
	Zone_Free(zone, sphere.indices);
	Zone_Free(zone, sphere.vertices);

#ifdef ANDROID
	// Shut down Android Audio
	AAudioStream_requestStop(audioStream);
	AAudioStream_close(audioStream);
#else
	// Shut down PortAudio
	Pa_AbortStream(audioStream);
	Pa_StopStream(audioStream);
	Pa_CloseStream(audioStream);
	Pa_Terminate();
#endif
}
