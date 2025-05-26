#include <stdint.h>
#include <stdbool.h>
#include <aaudio/AAudio.h>
#include "../../system/system.h"
#include "../audio.h"

static AAudioStream *audioStream=NULL;

static aaudio_data_callback_result_t aaudio_Callback(AAudioStream *stream, void *userData, void *audioData, int32_t numFrames)
{
	Audio_FillBuffer(audioData, numFrames);

	return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool AudioAndroid_Init(void)
{
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
	AAudioStreamBuilder_setDataCallback(streamBuilder, aaudio_Callback, NULL);

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

	return true;
}

void AudioAndroid_Destroy(void)
{
	AAudioStream_requestStop(audioStream);
	AAudioStream_close(audioStream);
}
