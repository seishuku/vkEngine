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

void MusicStreamData(void *buffer, size_t length)
{
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

	Audio_SetStreamCallback(0, MusicStreamData);
	Audio_StartStream(0);
}

void Music_Destroy(void)
{
	ov_clear(&oggStream);

	if(MusicList)
		Zone_Free(zone, MusicList);
}
