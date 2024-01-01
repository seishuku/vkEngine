#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <vorbis/vorbisfile.h>
#include "system/system.h"
#include "math/math.h"
#include "audio/audio.h"
#include "music.h"

FILE *oggFile;
OggVorbis_File oggStream;

const char *MusicPath="assets/music/";

String_t *MusicList;
uint32_t NumMusic=0, CurrentMusic=0;

#ifdef LINUX
#include <dirent.h>
#include <sys/stat.h>

String_t *BuildFileList(const char *DirName, const char *Filter, uint32_t *NumFiles)
{
	DIR *dp;
	struct dirent *dir_entry;
	String_t *Ret=NULL;

	if((dp=opendir(DirName))==NULL)
		return NULL;

	while((dir_entry=readdir(dp))!=NULL)
	{
		if(dir_entry->d_type==DT_REG)
		{
			const char *ptr=strrchr(dir_entry->d_name, '.');

			if(ptr!=NULL)
			{
				if(!strcmp(ptr, Filter))
				{
					Ret=(String_t *)Zone_Realloc(zone, Ret, sizeof(String_t)*(*NumFiles+1));
					sprintf(Ret[(*NumFiles)++].String, "%s", dir_entry->d_name);
				}
			}
		}
	}

	closedir(dp);

	return Ret;
}
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

String_t *BuildFileList(const char *DirName, const char *Filter, uint32_t *NumFiles)
{
	HANDLE hList;
	char szDir[MAX_PATH+1];
	WIN32_FIND_DATA FileData;
	String_t *Ret=NULL;

	sprintf(szDir, "%s\\*", DirName);

	if((hList=FindFirstFile(szDir, &FileData))==INVALID_HANDLE_VALUE)
		return NULL;

	for(;;)
	{
		if(!(FileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			const char *ptr=strrchr(FileData.cFileName, '.');

			if(ptr!=NULL)
			{
				if(!strcmp(ptr, Filter))
				{
					Ret=(String_t *)Zone_Realloc(zone, Ret, sizeof(String_t)*(*NumFiles+1));

					if(Ret==NULL)
						return NULL;

					sprintf(Ret[(*NumFiles)++].String, "%s", FileData.cFileName);
				}
			}
		}

		if(!FindNextFile(hList, &FileData))
		{
			if(GetLastError()==ERROR_NO_MORE_FILES)
				break;
		}
	}

	FindClose(hList);

	return Ret;
}
#endif

#include "camera/camera.h"
extern Camera_t camera;

void GenLaserShoot(WaveParams_t *params)
{
	params->waveType=2;

	params->attackTime=0.0f;
	params->sustainTime=0.5f;
	params->sustainPunch=0.0f;
	params->decayTime=0.0f;

	params->startFrequency=0.33f;
	params->minFrequency=0.0f;

	params->slide=0.31f;
	params->deltaSlide=0.2f;
	params->vibratoDepth=0.57f;
	params->vibratoSpeed=0.51f;

	params->changeAmount=0.0f;
	params->changeSpeed=0.61f;
	params->squareWaveDuty=0.52f;
	params->squareWaveDutySweep=-0.49f;

	params->repeatSpeed=0.55f;
	params->phaserOffset=0.24f;
	params->phaserSweep=0.33f;

	params->lpfCutoff=0.4f;
	params->lpfCutoffSweep=0.0f;
	params->lpfResonance=0.55f;
	params->hpfCutoff=0.0f;
	params->hpfCutoffSweep=0.0f;

	ResetWaveSample(params);
}

void GenEngineNoise(WaveParams_t *params)
{
	params->waveType=3;

	params->attackTime=0.0f;
	params->sustainTime=0.5f;
	params->sustainPunch=0.0f;
	params->decayTime=0.0f;

	params->startFrequency=0.08f;
	params->minFrequency=0.0f;

	params->slide=0.0f;
	params->deltaSlide=0.0f;
	params->vibratoDepth=0.0f;
	params->vibratoSpeed=0.0f;

	params->changeAmount=0.0f;
	params->changeSpeed=0.0f;
	params->squareWaveDuty=0.0f;
	params->squareWaveDutySweep=0.0f;

	params->repeatSpeed=0.0f;
	params->phaserOffset=0.0f;
	params->phaserSweep=0.0f;

	params->lpfCutoff=0.08f;
	params->lpfCutoffSweep=0.0f;
	params->lpfResonance=0.25f;
	params->hpfCutoff=0.0f;
	params->hpfCutoffSweep=0.0f;

	ResetWaveSample(params);
}

static WaveParams_t Laser, Engine;

void StreamData(void *buffer, size_t length)
{
#if 1
	if(oggFile==NULL)
		return;

	int size=0;

	// Callback param length is in frames, ov_read needs bytes:
	size_t lengthBytes=length*2*sizeof(int16_t);
	char *bufferBytePtr=buffer;

	do
	{
		size=ov_read(&oggStream, bufferBytePtr, (int)lengthBytes, 0, 2, 1, NULL);
		bufferBytePtr+=size;
		lengthBytes-=size;
	} while(lengthBytes&&size);

	// Out of OGG audio, get a new track
	if(size==0)
		NextTrackCallback(NULL);
#endif

	// Mix in synth soundfx only when firing
	int16_t *bufferPtr=(int16_t *)buffer;

	for(uint32_t i=0;i<length;i++)
	{
		int16_t sample=0;

		if(camera.shift)
		{
			int16_t laserSample=(int16_t)(GenerateWaveSample(&Laser)*INT16_MAX);
			sample=min(max(sample+laserSample, INT16_MIN), INT16_MAX);
		}

		if(camera.key_w||camera.key_s||camera.key_a||camera.key_d||camera.key_v||camera.key_c)
		{
			int16_t engineSample=(int16_t)(GenerateWaveSample(&Engine)*INT16_MAX);
			sample=min(max(sample+engineSample, INT16_MIN), INT16_MAX);
		}

		bufferPtr[0]=min(max(sample+bufferPtr[0], INT16_MIN), INT16_MAX);
		bufferPtr[1]=min(max(sample+bufferPtr[1], INT16_MIN), INT16_MAX);
		bufferPtr+=2;
	}
}

void StartStreamCallback(void *arg)
{
	Audio_StartStream();
}

void StopStreamCallback(void *arg)
{
	Audio_StopStream();
}

void PrevTrackCallback(void *arg)
{
	if(MusicList!=NULL)
	{
		ov_clear(&oggStream);

		char FilePath[1024]={ 0 };

		CurrentMusic=(CurrentMusic-1)%NumMusic;
		sprintf(FilePath, "%s%s", MusicPath, MusicList[CurrentMusic].String);
		oggFile=fopen(FilePath, "rb");

		if(oggFile==NULL)
			return;

		int result=ov_open(oggFile, &oggStream, NULL, 0);

		if(result<0)
		{
			fclose(oggFile);
			return;
		}
	}
}

void NextTrackCallback(void *arg)
{
	if(MusicList!=NULL)
	{
		ov_clear(&oggStream);

		char FilePath[1024]={ 0 };

		CurrentMusic=(CurrentMusic+1)%NumMusic;
		sprintf(FilePath, "%s%s", MusicPath, MusicList[CurrentMusic].String);
		oggFile=fopen(FilePath, "rb");

		if(oggFile==NULL)
			return;

		int result=ov_open(oggFile, &oggStream, NULL, 0);

		if(result<0)
		{
			fclose(oggFile);
			return;
		}
	}
}

void Music_Init(void)
{
	GenLaserShoot(&Laser);
	GenEngineNoise(&Engine);

	MusicList=BuildFileList(MusicPath, ".ogg", &NumMusic);

	if(MusicList!=NULL)
	{
		char FilePath[1024]={ 0 };

		CurrentMusic=RandRange(0, NumMusic-1);
		sprintf(FilePath, "%s%s", MusicPath, MusicList[CurrentMusic].String);
		oggFile=fopen(FilePath, "rb");

		if(oggFile==NULL)
			return;

		int result=ov_open(oggFile, &oggStream, NULL, 0);

		if(result<0)
		{
			fclose(oggFile);
			return;
		}
	}

	Audio_SetStreamCallback(StreamData);
}

void Music_Destroy(void)
{
	ov_clear(&oggStream);

	if(MusicList)
		Zone_Free(zone, MusicList);
}
