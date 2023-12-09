#ifndef __MUSIC_H__
#define __MUSIC_H__

typedef struct
{
	char String[256];
} String_t;

extern const char *MusicPath;

extern String_t *MusicList;
extern uint32_t NumMusic, CurrentMusic;

void StartStreamCallback(void *arg);
void StopStreamCallback(void *arg);
void PrevTrackCallback(void *arg);
void NextTrackCallback(void *arg);

void Music_Init(void);
void Music_Destroy(void);

#endif
