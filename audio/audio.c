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
#include "../utils/spatialhash.h"
#include "qoa.h"
#include "audio.h"

float audioTime=0.0;

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

static const uint32_t HRIR_MAGIC='H'|'R'<<8|'I'<<16|'R'<<24;

static SpatialHash_t HRIRHash;

static struct
{
	uint32_t magic;
	uint32_t sampleRate;
	uint32_t sampleLength;
	uint32_t numVertex;
	uint32_t numIndex;
	uint32_t *indices;
	HRIR_Vertex_t *vertices;
} sphere;

static float HRIRWindow[MAX_HRIR_SAMPLES]={ 0 };

#define MAX_CHANNELS 128

typedef struct
{
	Sample_t *sample;
	uint32_t position;
	bool looping;
	float volume;
	vec3 xyz;
} Channel_t;

static Channel_t channels[MAX_CHANNELS];

// HRIR interpolation result kernel buffer 
static int16_t HRIRKernel[2*MAX_HRIR_SAMPLES];

// Audio buffers for HRTF convolve
static int16_t preConvolve[MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES];
static int16_t postConvole[2*(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES)];

// Streaming audio
static struct
{
	uint32_t position;
	int16_t buffer[MAX_STREAM_SAMPLES*2];

	struct
	{
		bool playing;
		float volume;
		void (*streamCallback)(void *buffer, size_t length);
	} stream[MAX_AUDIO_STREAMS];
} streamBuffer;

extern Camera_t camera;

static inline int32_t bitwiseClamp32(int32_t x, int32_t min, int32_t max)
{
	const int32_t y=max^((x^max)&-(x<max));
	return y^((y^min)&-(y<min));
}

static vec2 CalculateBarycentric(const vec3 p, const vec3 a, const vec3 b, const vec3 c)
{
	const vec3 v0=Vec3_Subv(b, a), v1=Vec3_Subv(c, a), v2=Vec3_Subv(p, a);

	const float d00=Vec3_Dot(v0, v0);
	const float d01=Vec3_Dot(v0, v1);
	const float d11=Vec3_Dot(v1, v1);
	const float d20=Vec3_Dot(v2, v0);
	const float d21=Vec3_Dot(v2, v1);
	const float invDenom=1.0f/(d00*d11-d01*d01);

	return Vec2((d11*d20-d01*d21)*invDenom, (d00*d21-d01*d20)*invDenom);
}

static float maxDistanceSq=-1.0f;
static uint32_t triangleIndex=-1;

static void HRIR_FindBestTriangle(void *a, void *b)
{
	const vec3 *position=(vec3 *)a;
	const uint32_t tri=*(uint32_t *)b;
	const float d=Vec3_Dot(*position, sphere.vertices[3*tri].normal);

	if(d>maxDistanceSq)
	{
		maxDistanceSq=d;
		triangleIndex=tri;
	}
}

// HRIR sample interpolation, takes world-space position as input.
// HRIR samples are taken as float, but interpolated output is int16.
static void HRIRInterpolate(vec3 xyz)
{
	// Sound distance drop-off constant, this is the radius of the hearable range
	const float invRadius=1.0f/500.0f;

	// Calculate relative position of the sound source to the camera
	const vec3 relPosition=Vec3_Subv(xyz, camera.body.position);
	vec3 position=QuatRotate(QuatInverse(camera.body.orientation), relPosition);

	// Calculate distance fall-off
	float falloffDist=fmaxf(0.0f, 1.0f-Vec3_Length(Vec3_Muls(relPosition, invRadius)));

	Vec3_Normalize(&position);

	maxDistanceSq=-1.0f;
	triangleIndex=-1;

	// Query spatial hash for nearby triangles
	SpatialHash_TestObjects(&HRIRHash, position, &position, HRIR_FindBestTriangle);

	// Calculate the barycentric coordinates and use them to interpolate the HRIR samples.
	const HRIR_Vertex_t *v0=&sphere.vertices[triangleIndex+0];
	const HRIR_Vertex_t *v1=&sphere.vertices[triangleIndex+1];
	const HRIR_Vertex_t *v2=&sphere.vertices[triangleIndex+2];
	const vec2 g=Vec2_Clamp(CalculateBarycentric(position, v0->vertex, v1->vertex, v2->vertex), 0.0f, 1.0f);
	const vec3 coords=Vec3(g.x, g.y, 1.0f-g.x-g.y);

	if(coords.x>=0.0f&&coords.y>=0.0f&&coords.z>=0.0f)
	{
		for(uint32_t i=0;i<sphere.sampleLength;i++)
		{
			const vec3 left=Vec3(v0->left[i], v1->left[i], v2->left[i]);
			const vec3 right=Vec3(v0->right[i], v1->right[i], v2->right[i]);
			const float gain=4.0f;
			const float final=falloffDist*gain*HRIRWindow[i]*INT16_MAX;

			HRIRKernel[2*i+0]=(int16_t)(final*Vec3_Dot(left, coords));
			HRIRKernel[2*i+1]=(int16_t)(final*Vec3_Dot(right, coords));
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

		*outputPtr++=(int16_t)(bitwiseClamp32(sum[0], -0x40000000, 0x3FFFFFFF)>>15);
		*outputPtr++=(int16_t)(bitwiseClamp32(sum[1], -0x40000000, 0x3FFFFFFF)>>15);
	}
}

// Additively mix source into destination
static void MixAudio(int16_t *dst, const int16_t *src, const size_t length, const float volume)
{
	if(volume==0)
		return;

	for(size_t i=0;i<length*2;i+=4)
	{
		const int16_t src0=*src++, src1=*src++, src2=*src++, src3=*src++;
		const int16_t dst0=dst[0], dst1=dst[1], dst2=dst[2], dst3=dst[3];

		*dst++=bitwiseClamp32(src0*volume+dst0, INT16_MIN, INT16_MAX);
		*dst++=bitwiseClamp32(src1*volume+dst1, INT16_MIN, INT16_MAX);
		*dst++=bitwiseClamp32(src2*volume+dst2, INT16_MIN, INT16_MAX);
		*dst++=bitwiseClamp32(src3*volume+dst3, INT16_MIN, INT16_MAX);
	}
}

static void Audio_FillBuffer(void *buffer, uint32_t length)
{
	const double startTime=GetClock();

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
		if(channel->sample==NULL)
			continue;

		// Interpolate HRIR samples that are closest to the sound's position
		HRIRInterpolate(channel->xyz);

		// Calculate the remaining amount of data to process.
		size_t remainingData=channel->sample->length-channel->position;

		// Remaining data runs off end of primary buffer.
		// Clamp it to buffer size, we'll get the rest later.
		if(remainingData>=length)
			remainingData=length;

		// Calculate the amount to fill the convolution buffer.
		// The convolve buffer needs to be at least NUM_SAMPLE+HRIR length,
		//   but to stop annoying pops/clicks and other discontinuities, we need to copy ahead,
		//   which is either the full input sample length OR the full buffer+HRIR sample length.
		size_t toFill=(channel->sample->length-channel->position);

		if(toFill>=(MAX_AUDIO_SAMPLES+sphere.sampleLength))
			toFill=(MAX_AUDIO_SAMPLES+sphere.sampleLength);
		else if(toFill>=channel->sample->length)
			toFill=channel->sample->length;

		// Copy the samples and zero out the remaining buffer size.
		for(size_t dataIdx=0;dataIdx<(MAX_AUDIO_SAMPLES+MAX_HRIR_SAMPLES);dataIdx++)
		{
			if(dataIdx<=toFill)
				preConvolve[dataIdx]=channel->sample->data[channel->position+dataIdx];
			else
				preConvolve[dataIdx]=0;
		}

		// Convolve the samples with the interpolated HRIR sample to produce a stereo sample to mix into the output buffer
		Convolve(preConvolve, postConvole, remainingData, HRIRKernel, sphere.sampleLength);

		// Mix out the samples into the output buffer
		MixAudio(out, postConvole, (uint32_t)remainingData, channel->volume);

		// Advance the sample position by what we've used, next time around will take another chunk.
		channel->position+=(uint32_t)remainingData;

		// Reached end of audio sample, either remove from channels list, or
		//     if loop flag was set, reset position to 0.
		if(channel->position>=channel->sample->length)
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
				MixAudio(out, &streamBuffer.buffer[streamBuffer.position], remainingData, streamBuffer.stream[i].volume);
			}
		}
	}

	streamBuffer.position+=(uint32_t)remainingData;

	if(streamBuffer.position>=MAX_STREAM_SAMPLES)
		streamBuffer.position=0;

	audioTime=(float)(GetClock()-startTime);
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
uint32_t Audio_PlaySample(Sample_t *sample, const bool looping, const float volume, vec3 position)
{
	int32_t index;

	// Look for an empty sound channel slot.
	for(index=0;index<MAX_CHANNELS;index++)
	{
		// If it's either done playing or is still the initial zero.
		if(channels[index].sample==NULL)
			break;
	}

	// return if there aren't any channels available.
	if(index>=MAX_CHANNELS)
		return UINT32_MAX;

	// otherwise set the channel's sample pointer to this sample's address, reset play position, and loop flag.
	channels[index].sample=sample;
	channels[index].position=0;
	channels[index].looping=looping;

	channels[index].xyz=position;

	channels[index].volume=clampf(volume, 0.0f, 1.0f);

	return index;
}

void Audio_UpdateXYZPosition(uint32_t slot, vec3 position)
{
	if(slot>=MAX_CHANNELS)
		return;

	channels[slot].xyz=position;
}

void Audio_StopSample(uint32_t slot)
{
	if(slot>=MAX_CHANNELS)
		return;

	// Set the position to the end and allow the callback to resolve the removal
	channels[slot].position=channels[slot].sample->length-1;
	channels[slot].looping=false;
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

	streamBuffer.stream[stream].volume=clampf(volume, 0.0f, 1.0f);

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

	if(sphere.magic!=HRIR_MAGIC)
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

	if(!SpatialHash_Create(&HRIRHash, 512, 2.0f))
		return false;

	SpatialHash_Clear(&HRIRHash);

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

		SpatialHash_AddObject(&HRIRHash, Vec3_Muls(normal, 100.0f), &sphere.indices[i]);
	}

	for(uint32_t i=0;i<sphere.sampleLength;i++)
		HRIRWindow[i]=0.5f*(0.5f-cosf(2.0f*PI*(float)i/(sphere.sampleLength-1)));

	return true;
}

int Audio_Init(void)
{
	// Clear out mixing channels
	memset(channels, 0, sizeof(Channel_t)*MAX_CHANNELS);

	// Clear out stream buffer
	memset(&streamBuffer, 0, sizeof(streamBuffer));

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

	SpatialHash_Destroy(&HRIRHash);

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

// Simple resample and conversion function to the audio engine's common format (44.1KHz/16bit).
// Based off of what id Software used in Quake, seems to work well enough.
static int16_t *ConvertAndResample(const void *in, const uint32_t numSamples, const bool isFloat, const uint32_t sampleRate, const uint8_t bitsPerSample, const uint8_t channels)
{
	if(in==NULL)
		return NULL;

	const float stepScale=(float)sampleRate/AUDIO_SAMPLE_RATE;
	const uint32_t outCount=(uint32_t)(numSamples/stepScale);

	int16_t *out=(int16_t *)Zone_Malloc(zone, sizeof(int16_t)*outCount*channels);

	if(out==NULL)
		return NULL;

	for(uint32_t i=0, sampleFrac=0;i<outCount;i++, sampleFrac+=(int32_t)(stepScale*256))
	{
		const int32_t srcSample=sampleFrac>>8;
		int32_t sampleL=0, sampleR=0;

		// Input conversion/resample
		if(!isFloat)
		{
			if(bitsPerSample==32) // 32bit signed integer PCM
			{
				if(channels==2)
				{
					sampleL=((int32_t *)in)[2*srcSample+0]>>16;
					sampleR=((int32_t *)in)[2*srcSample+1]>>16;
				}
				else if(channels==1)
					sampleL=((int32_t *)in)[srcSample]>>16;
			}
			else if(bitsPerSample==24) // 24bit signed integer PCM
			{
				if(channels==2)
				{
					sampleL=(((uint8_t *)in)[i*6+0]|(((uint8_t *)in)[i*6+1]<<8)|(((uint8_t *)in)[i*6+2]<<16))>>8;
					sampleR=(((uint8_t *)in)[i*6+3]|(((uint8_t *)in)[i*6+4]<<8)|(((uint8_t *)in)[i*6+5]<<16))>>8;
				}
				else if(channels==1)
					sampleL=(((uint8_t *)in)[i*3+0]|(((uint8_t *)in)[i*3+1]<<8)|(((uint8_t *)in)[i*3+2]<<16))>>8;
			}
			else if(bitsPerSample==16) // 16bit signed integer PCM
			{
				if(channels==2)
				{
					sampleL=((int16_t *)in)[2*srcSample+0];
					sampleR=((int16_t *)in)[2*srcSample+1];
				}
				else if(channels==1)
					sampleL=((int16_t *)in)[srcSample];
			}
			else if(bitsPerSample==8) // 8bit unsigned integer PCM
			{
				if(channels==2)
				{
					sampleL=(((int8_t *)in)[2*srcSample+0]-128)<<8;
					sampleR=(((int8_t *)in)[2*srcSample+1]-128)<<8;
				}
				else if(channels==1)
					sampleL=(((int8_t *)in)[srcSample]-128)<<8;
			}
		}
		else
		{
			if(bitsPerSample==64) // 64bit float
			{
				if(channels==2)
				{
					sampleL=(((double *)in)[2*srcSample+0]*INT16_MAX);
					sampleR=(((double *)in)[2*srcSample+1]*INT16_MAX);
				}
				else
					sampleL=(((double *)in)[srcSample]*INT16_MAX);
			}
			else if(bitsPerSample==32) // 32bit float
			{
				if(channels==2)
				{
					sampleL=(((float *)in)[2*srcSample+0]*INT16_MAX);
					sampleR=(((float *)in)[2*srcSample+1]*INT16_MAX);
				}
				else
					sampleL=(((float *)in)[srcSample]*INT16_MAX);
			}
		}

		// Output (16bit signed int)
		if(channels==2)
		{
			out[2*i+0]=(int16_t)sampleL;
			out[2*i+1]=(int16_t)sampleR;
		}
		else if(channels==1)
			out[i]=(int16_t)sampleL;
	}

	return out;
}

bool Audio_LoadStatic(const char *filename, Sample_t *sample)
{
	const char *extension=strrchr(filename, '.');
	uint32_t numSamples=0, sampleRate=0, bitsPerSample=0, channels=0;
	bool isFloat=false;
	uint8_t *buffer=NULL;

	if(extension!=NULL)
	{
		if(!strcmp(extension, ".wav"))
		{
			WaveFormat_t format={ 0 };
			buffer=(uint8_t *)WavRead(filename, &format, &numSamples);

			if(buffer==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for file %s.\n", filename);
				return false;
			}

			if(format.formatTag==WAVE_FORMAT_IEEE_FLOAT)
				isFloat=true;
			else
				isFloat=false;

			sampleRate=format.samplesPerSec;
			bitsPerSample=format.bitsPerSample;
			channels=format.channels;
		}
		else if(!strcmp(extension, ".qoa"))
		{
			QOA_Desc_t qoa;
			FILE *stream=NULL;

			if((stream=fopen(filename, "rb"))==NULL)
				return false;

			// Seek to end of file to get file size, rescaling to align to 32 bit
			fseek(stream, 0, SEEK_END);

			int32_t size=ftell(stream);

			if(size==-1)
				return false;

			fseek(stream, 0, SEEK_SET);

			uint8_t *data=(uint8_t *)Zone_Malloc(zone, size);

			if(data==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for file %s.\n", filename);
				fclose(stream);
				return false;
			}

			fread(data, 1, size, stream);

			fclose(stream);

			buffer=(uint8_t *)QOA_Decode(data, size, &qoa);

			if(buffer==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "QOA decode for file %s failed.\n", filename);
				Zone_Free(zone, data);
				return false;
			}

			Zone_Free(zone, data);

			if(qoa.channels>2)
			{
				DBGPRINTF(DEBUG_ERROR, "QOA too many channels (%d) for file %s.\n", qoa.channels, filename);
				Zone_Free(zone, buffer);
				Zone_Free(zone, data);
				return false;
			}

			isFloat=false;
			numSamples=qoa.numSamples;
			sampleRate=qoa.sampleRate;
			bitsPerSample=16;
			channels=qoa.channels;
		}
		else
			return false;
	}

	// Covert to match primary buffer sampling rate
	const uint32_t outputSamples=(uint32_t)(numSamples/((float)sampleRate/AUDIO_SAMPLE_RATE));

#ifdef _DEBUG
	DBGPRINTF(DEBUG_INFO, "Converting %s wave format from %dHz/%dbit to %d/16bit.\n", filename, sampleRate, bitsPerSample, AUDIO_SAMPLE_RATE);
#endif

	int16_t *resampledAndConverted=ConvertAndResample(buffer, numSamples, isFloat, sampleRate, bitsPerSample, channels);

	if(resampledAndConverted==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to resample/convert buffer for file %s.\n", filename);
		return false;
	}

	Zone_Free(zone, buffer);

	sample->data=resampledAndConverted;

	sample->length=outputSamples;
	sample->channels=(uint8_t)channels;

	// Done, return out
	return true;
}
