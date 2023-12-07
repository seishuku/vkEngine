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

#define HRIR_MAGIC ('H'|('R'<<8)|('I'<<16)|('R'<<24))

// HRIR sphere model and audio samples
typedef struct
{
	vec3 Vertex, Normal;
	float Left[NUM_SAMPLES], Right[NUM_SAMPLES];
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

HRIR_Sphere_t Sphere;

#define MAX_VOLUME 255
#define MAX_CHANNELS 128

typedef struct
{
	int16_t *Data;
	uint32_t Position, Length;
	bool Looping;
	float Volume;
	vec3 *xyz;
	int16_t Working[NUM_SAMPLES+MAX_HRIR_SAMPLES];
} Channel_t;

static Channel_t Channels[MAX_CHANNELS];

static vec3 Zero={ 0.0f, 0.0f, 0.0f };

// HRIR interpolation buffers
float HRIRSamples[2*NUM_SAMPLES];

// Audio buffers after HRTF convolve
int16_t audioBuffer[2*(NUM_SAMPLES+MAX_HRIR_SAMPLES)];

extern Camera_t Camera;

static const float BaryInterpolate(const float h0, const float h1, const float h2, const float alpha, const float beta, const float gamma)
{
	return alpha*h0+beta*h1+gamma*h2;
}

static vec2 BarycentricCalc(const vec3 p, const vec3 a, const vec3 b, const vec3 c)
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

static void HRIRInterpolate(vec3 xyz)
{
	// Sound distance drop-off constant, this is the radius of the hearable range
	const float invRadius=1.0f/500.0f;

	// Calculate relative position of the sound source to the camera
	const vec3 relPosition=Vec3_Subv(xyz, Camera.Position);
	vec3 Position=Vec3(
		Vec3_Dot(relPosition, Camera.Right),
		Vec3_Dot(relPosition, Camera.Up),
		Vec3_Dot(relPosition, Camera.Forward)
	);

	// Normalize also returns the length of the vector...
	// So use that to calculate the distance fall-off,
	//     I know this makes this line of code look messy, deal with it. :P
	float falloffDist=1.0f-max(Vec3_Normalize(&Position)*invRadius, 0.0f);

	// TODO: Fix in the HRIR sphere generation tool:
	Position=Matrix3x3MultVec3(Position, MatrixRotate(deg2rad(90.0f), 0.0f, 1.0f, 0.0f));

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
	const vec2 g=Vec2_Clamp(BarycentricCalc(Position, v0->Vertex, v1->Vertex, v2->Vertex), 0.0f, 1.0f);
	const float det=1.0f-g.x-g.y;

	if(g.x>=0.0f&&g.y>=0.0f&&det>=0.0f)
	{
		for(uint32_t j=0;j<Sphere.SampleLength;j++)
		{
			HRIRSamples[2*j+0]=falloffDist*BaryInterpolate(v0->Left[j], v1->Left[j], v2->Left[j], g.x, g.y, det);
			HRIRSamples[2*j+1]=falloffDist*BaryInterpolate(v0->Right[j], v1->Right[j], v2->Right[j], g.x, g.y, det);
		}
	}
}

static void Convolve(int16_t *input, int16_t *output, size_t audio_len, float *kernel, size_t kernel_len)
{
	for(size_t i=0;i<audio_len+kernel_len-1;i++)
	{
		float sum[2]={ 0.0f, 0.0f };

		for(size_t j=0;j<kernel_len;j++)
		{
			if(i-j>=0&&i-j<audio_len)
			{
				sum[0]+=((float)input[i-j]/INT16_MAX)*kernel[2*j+0];
				sum[1]+=((float)input[i-j]/INT16_MAX)*kernel[2*j+1];
			}
		}

		output[2*i+0]=(int16_t)(sum[0]*INT16_MAX);
		output[2*i+1]=(int16_t)(sum[1]*INT16_MAX);
	}
}

static void MixAudio(int16_t *Dst, const int16_t *Src, uint32_t Length, int Volume)
{
	if(Volume==0)
		return;

	for(size_t i=0;i<Length*2;i+=2)
	{
		Dst[i+0]=min(max(((Src[i+0]*Volume)/128)+Dst[i+0], INT16_MIN), INT16_MAX);
		Dst[i+1]=min(max(((Src[i+1]*Volume)/128)+Dst[i+1], INT16_MIN), INT16_MAX);
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
		vec3 Position=Zero;
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

		if(toFill>=(NUM_SAMPLES+Sphere.SampleLength))
			toFill=(NUM_SAMPLES+Sphere.SampleLength);
		else if(toFill>=Channel->Length)
			toFill=Channel->Length;

		// Copy the samples.
		for(size_t dataIdx=0;dataIdx<(NUM_SAMPLES+MAX_HRIR_SAMPLES);dataIdx++)
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
		MixAudio(out, audioBuffer, (uint32_t)remainingData, 128);

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
void Audio_PlaySample(Sample_t *Sample, bool Looping, float Volume, vec3 *Position)
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

bool HRIR_Init(void)
{
	FILE *Stream=NULL;

	Stream=fopen("assets/hrir_full.bin", "rb");

	if(!Stream)
		return false;

	fread(&Sphere, sizeof(uint32_t), 5, Stream);

	if(Sphere.Magic!=HRIR_MAGIC)
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

	for(uint32_t i=0;i<Sphere.NumIndex;i+=3)
	{
		// Calculate the normal vector of the current triangle
		// TODO: PRECALCULATE NORMALS
		HRIR_Vertex_t *v0=&Sphere.Vertices[Sphere.Indices[i+0]];
		HRIR_Vertex_t *v1=&Sphere.Vertices[Sphere.Indices[i+1]];
		HRIR_Vertex_t *v2=&Sphere.Vertices[Sphere.Indices[i+2]];
		vec3 Normal=Vec3_Cross(Vec3_Subv(v1->Vertex, v0->Vertex), Vec3_Subv(v2->Vertex, v0->Vertex));
		Vec3_Normalize(&Normal);

		v0->Normal=Normal;
		v1->Normal=Normal;
		v2->Normal=Normal;
	}

	return true;
}

int Audio_Init(void)
{
	// Clear out mixing channels
	memset(Channels, 0, sizeof(Channel_t)*MAX_CHANNELS);

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
