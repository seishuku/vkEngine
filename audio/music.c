#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vorbis/vorbisfile.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../audio/audio.h"
#include "../audio/music.h"
#include "../audio/qoa.h"

static mtx_t musicMutex;

static OggVorbis_File oggStream;

#ifdef ANDROID
static const char *musicPath="/storage/emulated/0/Music/";
#else
static const char *musicPath="assets/music/";
#endif

typedef struct
{
	char string[256];
} String_t;

static String_t *musicList;
static uint32_t numMusic=0, currentMusic=0;

const char *GetCurrentMusicTrack(void)
{
	if(musicList==NULL||currentMusic>numMusic)
		return "Invalid track";

	return musicList[currentMusic].string;
}

#if defined(LINUX)||defined(ANDROID)
#include <dirent.h>
#include <sys/stat.h>

static String_t *BuildFileList(const char *dirName, const char *filter, uint32_t *numFiles)
{
	DIR *dp;
	struct dirent *dir_entry;
	String_t *results=NULL;

	if((dp=opendir(dirName))==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "BuildFileList: opendir failed.\n");
		return NULL;
	}

	while((dir_entry=readdir(dp))!=NULL)
	{
		if(dir_entry->d_type==DT_REG)
		{
			const char *ptr=strrchr(dir_entry->d_name, '.');

			if(ptr!=NULL)
			{
				if(!strcmp(ptr, filter))
				{
					results=(String_t *)Zone_Realloc(zone, results, sizeof(String_t)*(*numFiles+1));
					sprintf(results[(*numFiles)++].string, "%s", dir_entry->d_name);
				}
			}
		}
	}

	closedir(dp);

	return results;
}
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static String_t *BuildFileList(const char *dirName, const char *filter, uint32_t *numFiles)
{
	HANDLE hList;
	char szDir[MAX_PATH+1];
	WIN32_FIND_DATA fileData;
	String_t *results=NULL;

	sprintf(szDir, "%s\\*", dirName);

	if((hList=FindFirstFile(szDir, &fileData))==INVALID_HANDLE_VALUE)
		return NULL;

	for(;;)
	{
		if(!(fileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))
		{
			const char *ptr=strrchr(fileData.cFileName, '.');

			if(ptr!=NULL)
			{
				if(!strcmp(ptr, filter))
				{
					results=(String_t *)Zone_Realloc(zone, results, sizeof(String_t)*(*numFiles+1));

					if(results==NULL)
						return NULL;

					sprintf(results[(*numFiles)++].string, "%s", fileData.cFileName);
				}
			}
		}

		if(!FindNextFile(hList, &fileData))
		{
			if(GetLastError()==ERROR_NO_MORE_FILES)
				break;
		}
	}

	FindClose(hList);

	return results;
}
#endif

void MusicStreamData(void *buffer, size_t length)
{
	mtx_lock(&musicMutex);

	// Callback param length is in frames, ov_read needs bytes:
	size_t lengthBytes=length*2*sizeof(int16_t);
	char *bufferBytePtr=(char *)buffer;

	int size=0;

	do
	{
		size=ov_read(&oggStream, bufferBytePtr, (int)lengthBytes, 0, 2, 1, NULL);
		bufferBytePtr+=size;
		lengthBytes-=size;
	} while(lengthBytes&&size);

	mtx_unlock(&musicMutex);

	// Out of OGG audio, get a new track
	if(size==0)
		NextTrackCallback(NULL);
}

void StartStreamCallback(void *arg)
{
	Audio_StartStream(0);
}

void StopStreamCallback(void *arg)
{
	Audio_StopStream(0);
}

void PrevTrackCallback(void *arg)
{
	if(musicList!=NULL)
	{
		mtx_lock(&musicMutex);
		ov_clear(&oggStream);

		char filePath[1024]={ 0 };

		currentMusic=(currentMusic-1)%numMusic;
		sprintf(filePath, "%s%s", musicPath, GetCurrentMusicTrack());

		int result=ov_fopen(filePath, &oggStream);

		if(result<0)
		{
			ov_clear(&oggStream);
			mtx_unlock(&musicMutex);
			return;
		}

		mtx_unlock(&musicMutex);
	}
}

void NextTrackCallback(void *arg)
{
	if(musicList!=NULL)
	{
		mtx_lock(&musicMutex);
		ov_clear(&oggStream);

		char filePath[1024]={ 0 };

		currentMusic=(currentMusic+1)%numMusic;
		sprintf(filePath, "%s%s", musicPath, GetCurrentMusicTrack());

		int result=ov_fopen(filePath, &oggStream);

		if(result<0)
		{
			ov_clear(&oggStream);
			mtx_unlock(&musicMutex);
			return;
		}

		mtx_unlock(&musicMutex);
	}
}

void Music_Init(void)
{
	mtx_init(&musicMutex, mtx_plain);

	musicList=BuildFileList(musicPath, ".ogg", &numMusic);

	if(musicList!=NULL)
	{
		DBGPRINTF(DEBUG_INFO, "\nFound music files in \"%s\":\n", musicPath);
		for(uint32_t i=0;i<numMusic;i++)
			DBGPRINTF(DEBUG_WARNING, "%s\n", musicList[i].string);

		char filePath[1024]={ 0 };

		currentMusic=RandRange(0, numMusic-1);
		sprintf(filePath, "%s%s", musicPath, GetCurrentMusicTrack());

		int result=ov_fopen(filePath, &oggStream);

		if(result<0)
		{
			DBGPRINTF(DEBUG_ERROR, "Music_Init: Unable to ov_open %s\n", filePath);
			ov_clear(&oggStream);
			return;
		}
	}
	else
		return;

	Audio_SetStreamCallback(0, MusicStreamData);
	Audio_StartStream(0);
}

void Music_Destroy(void)
{
	ov_clear(&oggStream);

	if(musicList)
		Zone_Free(zone, musicList);
}
