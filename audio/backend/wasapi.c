#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <guiddef.h>
#include "../../system/system.h"
#include "../audio.h"

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient2, 0x726778CD, 0xF60A, 0x4EDA, 0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78, 0xAA);
DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

static IAudioClient2 *audioClient=NULL;
static IAudioRenderClient *audioRenderClient=NULL;
static HANDLE audioEvent=NULL;
static HANDLE audioThread=NULL;
static bool audioThreadRunning=false;
static WAVEFORMATEXTENSIBLE *mixFormat=NULL;

static DWORD WINAPI AudioThreadProc(LPVOID arg)
{
	UINT32 bufferFrameCount=0;
	IAudioClient2_GetBufferSize(audioClient, &bufferFrameCount);

	IAudioClient2_Start(audioClient);

	while(audioThreadRunning)
	{
		if(WaitForSingleObject(audioEvent, INFINITE)!=WAIT_OBJECT_0)
			break;

		UINT32 numFramesPadding=0;
		IAudioClient2_GetCurrentPadding(audioClient, &numFramesPadding);

		UINT32 framesToWrite=bufferFrameCount-numFramesPadding;

		if(framesToWrite>0)
		{
			BYTE *data=NULL;
			IAudioRenderClient_GetBuffer(audioRenderClient, framesToWrite, &data);
			Audio_FillBuffer(data, framesToWrite);
			IAudioRenderClient_ReleaseBuffer(audioRenderClient, framesToWrite, 0);
		}
	}

	IAudioClient2_Stop(audioClient);

	return 0;
}

bool AudioWASAPI_Init(void)
{
	if(FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
		return false;

	IMMDeviceEnumerator *devEnumerator=NULL;
	if(FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&devEnumerator)))
		return false;

	IMMDevice *device=NULL;
	if(FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(devEnumerator, eRender, eConsole, &device)))
		return false;

	if(FAILED(IMMDevice_Activate(device, &IID_IAudioClient2, CLSCTX_INPROC_SERVER, NULL, (void **)&audioClient)))
		return false;

	AudioClientProperties audioProps={ sizeof(AudioClientProperties) };
	audioProps.bIsOffload=FALSE;
	audioProps.eCategory=AudioCategory_GameMedia;
	audioProps.Options=AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;

	HRESULT hr=IAudioClient2_SetClientProperties(audioClient, &audioProps);

	if(FAILED(IAudioClient2_GetMixFormat(audioClient, (WAVEFORMATEX **)&mixFormat)))
		return false;

	mixFormat->Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
	mixFormat->Format.nChannels=2;
	mixFormat->Format.nSamplesPerSec=AUDIO_SAMPLE_RATE;
	mixFormat->Format.wBitsPerSample=16;
	mixFormat->Format.nBlockAlign=mixFormat->Format.nChannels*mixFormat->Format.wBitsPerSample/8;
	mixFormat->Format.nAvgBytesPerSec=mixFormat->Format.nSamplesPerSec*mixFormat->Format.nBlockAlign;
	mixFormat->Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
	mixFormat->Samples.wValidBitsPerSample=16;
	mixFormat->dwChannelMask=SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
	mixFormat->SubFormat=KSDATAFORMAT_SUBTYPE_PCM;

	REFERENCE_TIME defaultPeriod=0, minimumPeriod=0;
	IAudioClient2_GetDevicePeriod(audioClient, &defaultPeriod, &minimumPeriod);

	REFERENCE_TIME bufferDuration=10000000ll*(MAX_AUDIO_SAMPLES/2)/mixFormat->Format.nSamplesPerSec;

	if(bufferDuration<minimumPeriod)
		return false;

	audioEvent=CreateEvent(NULL, FALSE, FALSE, NULL);

	if(FAILED(hr=IAudioClient2_Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, minimumPeriod, 0, (WAVEFORMATEX *)mixFormat, NULL)))
		return false;

	IAudioClient2_SetEventHandle(audioClient, audioEvent);

	if(FAILED(IAudioClient2_GetService(audioClient, &IID_IAudioRenderClient, (void **)&audioRenderClient)))
		return false;

	audioThreadRunning=true;
	audioThread=CreateThread(NULL, 0, AudioThreadProc, NULL, 0, NULL);

	return true;
}

void AudioWASAPI_Destroy(void)
{
	audioThreadRunning=false;

	if(audioThread)
	{
		WaitForSingleObject(audioThread, INFINITE);
		CloseHandle(audioThread);
		audioThread=NULL;
	}

	if(audioRenderClient)
		IAudioRenderClient_Release(audioRenderClient);

	if(audioClient)
		IAudioClient2_Release(audioClient);

	if(audioEvent)
		CloseHandle(audioEvent);

	CoTaskMemFree(mixFormat);

	audioRenderClient=NULL;
	audioClient=NULL;
	audioEvent=NULL;

	CoUninitialize();
}
