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
static AAudioStream *AudioStream=NULL;
#else
static PaStream *AudioStream=NULL;
#endif

// HRIR sphere model and audio samples
typedef struct
{
	vec3 Vertex, Normal;
	float Left[MAX_HRIR_SAMPLES], Right[MAX_HRIR_SAMPLES];
} HRIR_Vertex_t;

typedef struct
{
	uint32_t Magic;
	uint32_t SampleRate;
	uint32_t SampleLength;
	uint32_t NumVertex;
	uint32_t NumIndex;
	uint32_t *Indices;
	HRIR_Vertex_t *Vertices;
} HRIR_Sphere_t;

static HRIR_Sphere_t Sphere;

#define MAX_VOLUME 128

#define MAX_CHANNELS 128

typedef struct
{
	int16_t *Data;
	uint32_t Position, Length;
	bool Looping;
	float Volume;
	vec3 *xyz;
	int16_t Working[MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES];
} Channel_t;

static Channel_t Channels[MAX_CHANNELS];

// HRIR interpolation buffers
static int16_t HRIRSamples[2*MAX_HRIR_SAMPLES];

// Audio buffer after HRTF convolve
static int16_t audioBuffer[2*(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES)];

// Streaming audio

typedef struct
{
	bool Playing;
	float Volume;
	uint32_t Position;
	int16_t Buffer[MAX_STREAM_SAMPLES*2];
	void (*StreamCallback)(void *Buffer, size_t Length);
} AudioStream_t;

static AudioStream_t streamBuffer;

extern Camera_t Camera;

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
	const vec3 relPosition=Vec3_Subv(xyz, Camera.Position);
	vec3 Position=Vec3(
		Vec3_Dot(relPosition, Camera.Right),
		Vec3_Dot(relPosition, Camera.Up),
		Vec3_Dot(relPosition, Camera.Forward)
	);

	// Calculate distance fall-off
	float falloffDist=max(0.0f, 1.0f-Vec3_Length(Vec3_Muls(Position, invRadius)));

	// Normalize also returns the length of the vector...
	Vec3_Normalize(&Position);

	// Find closest triangle to the sound direction
	float maxDistanceSq=-1.0f;
	int32_t triangleIndex=-1;

	for(uint32_t i=0;i<Sphere.NumIndex;i+=3)
	{
		const float distanceSq=Vec3_Dot(Position, Sphere.Vertices[Sphere.Indices[i+0]].Normal);

		if(distanceSq>maxDistanceSq)
		{
			maxDistanceSq=distanceSq;
			triangleIndex=i;
		}
	}

	// Calculate the barycentric coordinates and use them to interpolate the HRIR samples.
	const HRIR_Vertex_t *v0=&Sphere.Vertices[Sphere.Indices[triangleIndex+0]];
	const HRIR_Vertex_t *v1=&Sphere.Vertices[Sphere.Indices[triangleIndex+1]];
	const HRIR_Vertex_t *v2=&Sphere.Vertices[Sphere.Indices[triangleIndex+2]];
	const vec2 g=Vec2_Clamp(CalculateBarycentric(Position, v0->Vertex, v1->Vertex, v2->Vertex), 0.0f, 1.0f);
	const vec3 Coords=Vec3(g.x, g.y, 1.0f-g.x-g.y);

	if(Coords.x>=0.0f&&Coords.y>=0.0f&&Coords.z>=0.0f)
	{
		for(uint32_t j=0;j<Sphere.SampleLength;j++)
		{
			const vec3 Left=Vec3(v0->Left[j], v1->Left[j], v2->Left[j]);
			const vec3 Right=Vec3(v0->Right[j], v1->Right[j], v2->Right[j]);

			HRIRSamples[2*j+0]=(int16_t)(falloffDist*Vec3_Dot(Left, Coords)*INT16_MAX);
			HRIRSamples[2*j+1]=(int16_t)(falloffDist*Vec3_Dot(Right, Coords)*INT16_MAX);
		}
	}
}

// Integer audio convolution, this is a current chokepoint in the audio system at 25% CPU usage in the profiler.
// I don't think I can optimize this any more without going to SIMD or multithreading.
static void Convolve(const int16_t *Input, int16_t *Output, const size_t Length, const int16_t *Kernel, const size_t kernelLength)
{
	int16_t *outputPtr=Output;
	int32_t sum[2];

	for(size_t i=0;i<Length+kernelLength-1;i++)
	{
		const int16_t *inputPtr=&Input[i+kernelLength];
		const int16_t *kernelPtr=Kernel;

		sum[0]=1<<14;
		sum[1]=1<<14;

		for(size_t j=0;j<kernelLength;j++)
		{
			sum[0]+=(int32_t)(*kernelPtr++)*(int32_t)(*inputPtr);
			sum[1]+=(int32_t)(*kernelPtr++)*(int32_t)(*inputPtr);
			inputPtr--;
		}

		*outputPtr++=(int16_t)(min(0x3fffffff, max(-0x40000000, sum[0]))>>15);
		*outputPtr++=(int16_t)(min(0x3fffffff, max(-0x40000000, sum[1]))>>15);
	}
}

// Additively mix source into destination
static void MixAudio(int16_t *Dst, const int16_t *Src, const size_t Length, const int8_t Volume)
{
	if(Volume==0)
		return;

	for(size_t i=0;i<Length*2;i+=2)
	{
		Dst[i+0]=min(max(((Src[i+0]*Volume)/MAX_VOLUME)+Dst[i+0], INT16_MIN), INT16_MAX);
		Dst[i+1]=min(max(((Src[i+1]*Volume)/MAX_VOLUME)+Dst[i+1], INT16_MIN), INT16_MAX);
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
		Channel_t *Channel=&Channels[i];

		// If the channel is empty, skip on the to next.
		if(!Channel->Data)
			continue;

		// If there's a position set, use it.
		vec3 Position=Vec3b(0.0f);
		if(Channel->xyz)
			Position=*Channel->xyz;

		// Interpolate HRIR samples that are closest to the sound's position
		HRIRInterpolate(Position);

		// Calculate the remaining amount of data to process.
		size_t remainingData=Channel->Length-Channel->Position;

		// Remaining data runs off end of primary buffer.
		// Clamp it to buffer size, we'll get the rest later.
		if(remainingData>=Length)
			remainingData=Length;

		// Calculate the amount to fill the convolution buffer.
		// The convolve buffer needs to be at least NUM_SAMPLE+HRIR length,
		//   but to stop annoying pops/clicks and other discontinuities, we need to copy ahead,
		//   which is either the full input sample length OR the full buffer+HRIR sample length.
		size_t toFill=(Channel->Length-Channel->Position);

		if(toFill>=(MAX_AUDIO_SAMPLES+Sphere.SampleLength))
			toFill=(MAX_AUDIO_SAMPLES+Sphere.SampleLength);
		else if(toFill>=Channel->Length)
			toFill=Channel->Length;

		// Copy the samples.
		for(size_t dataIdx=0;dataIdx<(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES);dataIdx++)
		{
			if(dataIdx<=toFill)
				Channel->Working[dataIdx]=Channel->Data[Channel->Position+dataIdx];
			else
			{
				// Zero out the remaining buffer size.
				Channel->Working[dataIdx]=0;
			}
		}

		// Convolve the samples with the interpolated HRIR sample to produce a stereo sample to mix into the output buffer
		Convolve(Channel->Working, audioBuffer, remainingData, HRIRSamples, Sphere.SampleLength);

		// Mix out the samples into the output buffer
		MixAudio(out, audioBuffer, (uint32_t)remainingData, (int8_t)(Channel->Volume*MAX_VOLUME));

		// Advance the sample position by what we've used, next time around will take another chunk.
		Channel->Position+=(uint32_t)remainingData;

		// Reached end of audio sample, either remove from channels list, or
		//     if loop flag was set, reset position to 0.
		if(Channel->Position>=Channel->Length)
		{
			if(Channel->Looping)
				Channel->Position=0;
			else
			{
				// Remove from list
				memset(Channel, 0, sizeof(Channel_t));
			}
		}
	}

	if(streamBuffer.Playing)
	{
		size_t remainingData=min(MAX_STREAM_SAMPLES-streamBuffer.Position, Length);

		// If there's an assigned callback, call it to load more audio data
		if(streamBuffer.StreamCallback)
		{
			streamBuffer.StreamCallback(&streamBuffer.Buffer[streamBuffer.Position], remainingData);
			MixAudio(out, &streamBuffer.Buffer[streamBuffer.Position], remainingData, (int8_t)(streamBuffer.Volume*MAX_VOLUME));
			streamBuffer.Position+=remainingData;

			if(streamBuffer.Position>=MAX_STREAM_SAMPLES)
				streamBuffer.Position=0;
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
void Audio_PlaySample(Sample_t *Sample, const bool Looping, const float Volume, vec3 *Position)
{
	int32_t index;

	// Look for an empty sound channel slot.
	for(index=0;index<MAX_CHANNELS;index++)
	{
		// If it's either done playing or is still the initial zero.
		if(Channels[index].Position==Channels[index].Length)
			break;
	}

	// return if there aren't any channels available.
	if(index>=MAX_CHANNELS)
		return;

	// otherwise set the channel's data pointer to this sample's pointer
	// and set the length, reset play position, and loop flag.
	Channels[index].Data=Sample->Data;
	Channels[index].Length=Sample->Length;
	Channels[index].Position=0;
	Channels[index].Looping=Looping;

	if(Position)
		Channels[index].xyz=Position;
	else
		Channels[index].xyz=&Sample->xyz;

	Channels[index].Volume=min(1.0f, max(0.0f, Volume));
}

void Audio_StopSample(Sample_t *Sample)
{
	int32_t index;

	// Search for the sample in the channels list
	for(index=0;index<MAX_CHANNELS;index++)
	{
		if(Channels[index].Data==Sample->Data)
			break;
	}

	// return if it didn't find the sample
	if(index>=MAX_CHANNELS)
		return;

	// Set the position to the end and allow the callback to resolve the removal
	Channels[index].Position=Channels[index].Length-1;
	Channels[index].Looping=false;
}

void Audio_SetStreamCallback(void (*StreamCallback)(void *Buffer, size_t Length))
{
	streamBuffer.StreamCallback=StreamCallback;
}

void Audio_SetStreamVolume(const float Volume)
{
	streamBuffer.Volume=min(1.0f, max(0.0f, Volume));
}

void Audio_StartStream(void)
{
	streamBuffer.Playing=true;
}

void Audio_StopStream(void)
{
	streamBuffer.Playing=false;
}

bool HRIR_Init(void)
{
	FILE *Stream=NULL;

	Stream=fopen("assets/hrir_full.bin", "rb");

	if(!Stream)
		return false;

	fread(&Sphere, sizeof(uint32_t), 5, Stream);

	if(Sphere.Magic!=('H'|('R'<<8)|('I'<<16)|('R'<<24)))
		return false;

	if(Sphere.SampleLength>MAX_HRIR_SAMPLES)
		return false;

	Sphere.Indices=(uint32_t *)Zone_Malloc(Zone, sizeof(uint32_t)*Sphere.NumIndex);

	if(Sphere.Indices==NULL)
		return false;

	fread(Sphere.Indices, sizeof(uint32_t), Sphere.NumIndex, Stream);

	Sphere.Vertices=(HRIR_Vertex_t *)Zone_Malloc(Zone, sizeof(HRIR_Vertex_t)*Sphere.NumVertex);

	if(Sphere.Vertices==NULL)
		return false;

	memset(Sphere.Vertices, 0, sizeof(HRIR_Vertex_t)*Sphere.NumVertex);

	for(uint32_t i=0;i<Sphere.NumVertex;i++)
	{
		fread(&Sphere.Vertices[i].Vertex, sizeof(vec3), 1, Stream);

		fread(Sphere.Vertices[i].Left, sizeof(float), Sphere.SampleLength, Stream);
		fread(Sphere.Vertices[i].Right, sizeof(float), Sphere.SampleLength, Stream);
	}

	fclose(Stream);

	// Pre-calculate the normal vector of the HRIR sphere triangles
	for(uint32_t i=0;i<Sphere.NumIndex;i+=3)
	{
		HRIR_Vertex_t *v0=&Sphere.Vertices[Sphere.Indices[i+0]];
		HRIR_Vertex_t *v1=&Sphere.Vertices[Sphere.Indices[i+1]];
		HRIR_Vertex_t *v2=&Sphere.Vertices[Sphere.Indices[i+2]];
		vec3 Normal=Vec3_Cross(Vec3_Subv(v1->Vertex, v0->Vertex), Vec3_Subv(v2->Vertex, v0->Vertex));
		Vec3_Normalize(&Normal);

		v0->Normal=Vec3_Addv(v0->Normal, Normal);
		v1->Normal=Vec3_Addv(v1->Normal, Normal);
		v2->Normal=Vec3_Addv(v2->Normal, Normal);
	}

	return true;
}

int Audio_Init(void)
{
	// Clear out mixing channels
	memset(Channels, 0, sizeof(Channel_t)*MAX_CHANNELS);

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
	AAudioStreamBuilder_setSampleRate(streamBuilder, SAMPLE_RATE);
	AAudioStreamBuilder_setPerformanceMode(streamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
	AAudioStreamBuilder_setDataCallback(streamBuilder, Audio_Callback, NULL);

	// Opens the stream.
	if(AAudioStreamBuilder_openStream(streamBuilder, &AudioStream)!=AAUDIO_OK)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Error opening stream\n");
		return false;
	}

	if(AAudioStream_getSampleRate(AudioStream)!=SAMPLE_RATE)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Sample rate mismatch\n");
		return false;
	}

	// Sets the buffer size. 
	AAudioStream_setBufferSizeInFrames(AudioStream, AAudioStream_getFramesPerBurst(AudioStream)*NUM_SAMPLES);

	// Starts the stream.
	if(AAudioStream_requestStart(AudioStream)!=AAUDIO_OK)
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
	if(Pa_OpenStream(&AudioStream, NULL, &outputParameters, AUDIO_SAMPLE_RATE, MAX_AUDIO_SAMPLES, paNoFlag, Audio_Callback, NULL)!=paNoError)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio: Unable to open PortAudio stream.\n");
		Pa_Terminate();
		return false;
	}

	// Start audio stream
	if(Pa_StartStream(AudioStream)!=paNoError)
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
	AAudioStream_requestStop(AudioStream);
	AAudioStream_close(AudioStream);
#else
	// Shut down PortAudio
	Pa_AbortStream(AudioStream);
	Pa_StopStream(AudioStream);
	Pa_CloseStream(AudioStream);
	Pa_Terminate();
#endif
}
