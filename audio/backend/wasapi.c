#define INITGUID
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <audioclient.h>
#include <avrt.h>
#include <functiondiscoverykeys_devpkey.h>
#include <guiddef.h>
#include <mmdeviceapi.h>
#include "../../system/system.h"
#include "../audio.h"

#ifndef __MINGW32__
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient2, 0x726778CD, 0xF60A, 0x4EDA, 0x82, 0xDE, 0xE4, 0x76, 0x10, 0xCD, 0x78, 0xAA);
DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
#endif

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

static IAudioClient2 *audioClient=NULL;
static IAudioRenderClient *audioRenderClient=NULL;
static HANDLE audioEvent=NULL;
static HANDLE audioThread=NULL;
static volatile LONG audioThreadRunning=FALSE;

static DWORD WINAPI AudioThreadProc(LPVOID arg)
{
	DWORD taskIndex=0;
	HANDLE mmcssHandle=AvSetMmThreadCharacteristics("Pro Audio", &taskIndex);

	if(mmcssHandle)
	{
		if(!AvSetMmThreadPriority(mmcssHandle, AVRT_PRIORITY_CRITICAL))
			DBGPRINTF(DEBUG_WARNING, "Audio - WASAPI: Failed to set MMCSS priority (%lu).\n", GetLastError());
	}
	else
	{
		DBGPRINTF(DEBUG_WARNING, "Audio - WASAPI: Failed to register MMCSS thread (%lu).\n", GetLastError());

		if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST))
			DBGPRINTF(DEBUG_WARNING, "Audio - WASAPI: Failed to set thread priority (%lu).\n", GetLastError());
	}

	UINT32 bufferFrameCount=0;
	HRESULT hr=IAudioClient2_GetBufferSize(audioClient, &bufferFrameCount);

	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetBufferSize failed (0x%08lx).\n", (unsigned long)hr);
		goto cleanup;
	}

	hr=IAudioClient2_Start(audioClient);

	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Start failed (0x%08lx).\n", (unsigned long)hr);
		goto cleanup;
	}

	static ULONGLONG lastDeadlineWarning=0;

	while(InterlockedCompareExchange(&audioThreadRunning, 0, 0))
	{
		DWORD waitResult=WaitForSingleObject(audioEvent, INFINITE);

		if(waitResult!=WAIT_OBJECT_0)
		{
			if(waitResult==WAIT_FAILED)
				DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: WaitForSingleObject failed (%lu).\n", GetLastError());
			else
				DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Unexpected audio event wait result (%lu).\n", waitResult);
			break;
		}

		UINT32 numFramesPadding=0;
		hr=IAudioClient2_GetCurrentPadding(audioClient, &numFramesPadding);

		if(FAILED(hr))
		{
			DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetCurrentPadding failed (0x%08lx).\n", (unsigned long)hr);
			break;
		}

		if(numFramesPadding>bufferFrameCount)
		{
			DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Padding exceeds buffer size (%u > %u).\n", numFramesPadding, bufferFrameCount);
			break;
		}

		UINT32 framesToWrite=bufferFrameCount-numFramesPadding;

		if(framesToWrite>0)
		{
			BYTE *data=NULL;
			hr=IAudioRenderClient_GetBuffer(audioRenderClient, framesToWrite, &data);

			if(FAILED(hr))
			{
				DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetBuffer failed (0x%08lx).\n", (unsigned long)hr);
				break;
			}

			if(data==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetBuffer returned a null buffer.\n");
				break;
			}

			const double fillStart=GetClock();
			Audio_FillBuffer(data, framesToWrite);
			const double fillTime=GetClock()-fillStart;

			if(fillTime>(double)numFramesPadding/AUDIO_SAMPLE_RATE)
			{
				const ULONGLONG now=GetTickCount64();

				if(now-lastDeadlineWarning>=1000)
				{
					DBGPRINTF(DEBUG_WARNING, "Audio - WASAPI: Render fill took %.2f ms with %u queued frames.\n", fillTime*1000.0, numFramesPadding);
					lastDeadlineWarning=now;
				}
			}

			hr=IAudioRenderClient_ReleaseBuffer(audioRenderClient, framesToWrite, 0);

			if(FAILED(hr))
			{
				DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: ReleaseBuffer failed (0x%08lx).\n", (unsigned long)hr);
				break;
			}
		}
	}

	hr=IAudioClient2_Stop(audioClient);

	if(FAILED(hr))
		DBGPRINTF(DEBUG_WARNING, "Audio - WASAPI: Stop failed (0x%08lx).\n", (unsigned long)hr);

cleanup:
	InterlockedExchange(&audioThreadRunning, FALSE);

	if(mmcssHandle)
		AvRevertMmThreadCharacteristics(mmcssHandle);

	return 0;
}
 
bool AudioWASAPI_Init(void)
{
	if(FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
		return false;

	IMMDeviceEnumerator *devEnumerator=NULL;
	IMMDevice *device=NULL;

	if(FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&devEnumerator)))
		goto error;

	if(FAILED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(devEnumerator, eRender, eConsole, &device)))
		goto error;

	LPWSTR deviceID=NULL;
	if(SUCCEEDED(IMMDevice_GetId(device, &deviceID)))
	{
		wchar_t wbuffer[500];
		char buffer[500];
		swprintf(wbuffer, 500, L"WASAPI Device ID: %s\n", deviceID);
		wcstombs(buffer, wbuffer, 500);
		DBGPRINTF(DEBUG_INFO, "%s", buffer);
		CoTaskMemFree(deviceID);
	}

	IPropertyStore *props=NULL;
	if(SUCCEEDED(IMMDevice_OpenPropertyStore(device, STGM_READ, &props)))
	{
		PROPVARIANT nameProp;
		PropVariantInit(&nameProp);

		if(SUCCEEDED(IPropertyStore_GetValue(props, &PKEY_Device_FriendlyName, &nameProp)))
		{
			wchar_t wbuffer[500];
			char buffer[500];
			swprintf(wbuffer, 500, L"WASAPI Device Name: %s\n", nameProp.pwszVal);
			wcstombs(buffer, wbuffer, 500);
			DBGPRINTF(DEBUG_INFO, "%s", buffer);
		}

		PropVariantClear(&nameProp);
		IPropertyStore_Release(props);
	}

	if(FAILED(IMMDevice_Activate(device, &IID_IAudioClient2, CLSCTX_INPROC_SERVER, NULL, (void **)&audioClient)))
		goto error;

	IMMDevice_Release(device);
	device=NULL;
	IMMDeviceEnumerator_Release(devEnumerator);
	devEnumerator=NULL;

	AudioClientProperties audioProps={ sizeof(AudioClientProperties) };
	audioProps.bIsOffload=FALSE;
	audioProps.eCategory=AudioCategory_GameMedia;
	audioProps.Options=AUDCLNT_STREAMOPTIONS_MATCH_FORMAT;

	HRESULT hr=IAudioClient2_SetClientProperties(audioClient, &audioProps);
	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: SetClientProperties failed (0x%08lx).\n", (unsigned long)hr);
		goto error;
	}

	WAVEFORMATEXTENSIBLE outputFormat={ 0 };
	outputFormat.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
	outputFormat.Format.nChannels=2;
	outputFormat.Format.nSamplesPerSec=AUDIO_SAMPLE_RATE;
	outputFormat.Format.wBitsPerSample=16;
	outputFormat.Format.nBlockAlign=outputFormat.Format.nChannels*outputFormat.Format.wBitsPerSample/8;
	outputFormat.Format.nAvgBytesPerSec=outputFormat.Format.nSamplesPerSec*outputFormat.Format.nBlockAlign;
	outputFormat.Format.cbSize=sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX);
	outputFormat.Samples.wValidBitsPerSample=16;
	outputFormat.dwChannelMask=SPEAKER_FRONT_LEFT|SPEAKER_FRONT_RIGHT;
	outputFormat.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;

	WAVEFORMATEX *closestFormat=NULL;
	hr=IAudioClient2_IsFormatSupported(audioClient, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&outputFormat, &closestFormat);

	if(hr!=S_OK)
	{
		if(hr==S_FALSE&&closestFormat)
			DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Requested 48 kHz stereo PCM is unsupported; closest is %lu Hz, %u ch, %u-bit.\n", closestFormat->nSamplesPerSec, closestFormat->nChannels, closestFormat->wBitsPerSample);
		else
			DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Format support check failed (0x%08lx).\n", (unsigned long)hr);

		if(closestFormat)
			CoTaskMemFree(closestFormat);

		goto error;
	}
	if(closestFormat)
		CoTaskMemFree(closestFormat);

	REFERENCE_TIME defaultPeriod=0, minimumPeriod=0;
	hr=IAudioClient2_GetDevicePeriod(audioClient, &defaultPeriod, &minimumPeriod);

	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetDevicePeriod failed (0x%08lx).\n", (unsigned long)hr);
		goto error;
	}

	REFERENCE_TIME bufferDuration=10000000ll*(MAX_AUDIO_SAMPLES/2)/AUDIO_SAMPLE_RATE;

	if(bufferDuration<minimumPeriod)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Requested buffer duration is below the device minimum.\n");
		goto error;
	}

	audioEvent=CreateEvent(NULL, FALSE, FALSE, NULL);

	if(audioEvent==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Failed to create audio event (%lu).\n", GetLastError());
		goto error;
	}

	hr=IAudioClient2_Initialize(audioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferDuration, 0, (WAVEFORMATEX *)&outputFormat, NULL);

	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Initialize failed (0x%08lx).\n", (unsigned long)hr);
		goto error;
	}

	DBGPRINTF(DEBUG_INFO,
	          "WASAPI Output Format:\n"
	          "\tSample Rate     : %uHz\n"
	          "\tChannels        : %u\n"
	          "\tBits per Sample : %u\n",
	          outputFormat.Format.nSamplesPerSec,
	          outputFormat.Format.nChannels,
	          outputFormat.Format.wBitsPerSample);

	hr=IAudioClient2_SetEventHandle(audioClient, audioEvent);
	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: SetEventHandle failed (0x%08lx).\n", (unsigned long)hr);
		goto error;
	}

	hr=IAudioClient2_GetService(audioClient, &IID_IAudioRenderClient, (void **)&audioRenderClient);
	if(FAILED(hr))
	{
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: GetService failed (0x%08lx).\n", (unsigned long)hr);
		goto error;
	}

	InterlockedExchange(&audioThreadRunning, TRUE);

	audioThread=CreateThread(NULL, 0, AudioThreadProc, NULL, 0, NULL);

	if(audioThread==NULL)
	{
		InterlockedExchange(&audioThreadRunning, FALSE);
		DBGPRINTF(DEBUG_ERROR, "Audio - WASAPI: Failed to create audio thread (%lu).\n", GetLastError());
		goto error;
	}

	return true;

error:
	if(audioRenderClient)
	{
		IAudioRenderClient_Release(audioRenderClient);
		audioRenderClient=NULL;
	}

	if(audioClient)
	{
		IAudioClient2_Release(audioClient);
		audioClient=NULL;
	}

	if(audioEvent)
	{
		CloseHandle(audioEvent);
		audioEvent=NULL;
	}

	if(device)
		IMMDevice_Release(device);

		if(devEnumerator)
		IMMDeviceEnumerator_Release(devEnumerator);

	CoUninitialize();

	return false;
}

void AudioWASAPI_Destroy(void)
{
	InterlockedExchange(&audioThreadRunning, FALSE);

	if(audioEvent)
        SetEvent(audioEvent);

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

	audioRenderClient=NULL;
	audioClient=NULL;
	audioEvent=NULL;

	CoUninitialize();
}
