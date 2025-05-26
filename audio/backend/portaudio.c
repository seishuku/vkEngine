#include <stdint.h>
#include <stdbool.h>
#include <portaudio.h>
#include "../../system/system.h"
#include "../audio.h"

static int Pa_Callback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	Audio_FillBuffer(outputBuffer, framesPerBuffer);

	return paContinue;
}

bool AudioPortAudio_Init(void)
{
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
	if(Pa_OpenStream(&audioStream, NULL, &outputParameters, AUDIO_SAMPLE_RATE, MAX_AUDIO_SAMPLES, paNoFlag, Pa_Callback, NULL)!=paNoError)
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
}

void AudioPortAudio_Destroy(void)
{
   	// Shut down PortAudio
	Pa_AbortStream(audioStream);
	Pa_StopStream(audioStream);
	Pa_CloseStream(audioStream);
	Pa_Terminate();
}
