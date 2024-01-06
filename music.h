#ifndef __MUSIC_H__
#define __MUSIC_H__

typedef struct
{
	char string[256];
} String_t;

extern String_t *musicList;
extern uint32_t numMusic, currentMusic;

void StartStreamCallback(void *arg);
void StopStreamCallback(void *arg);
void PrevTrackCallback(void *arg);
void NextTrackCallback(void *arg);

void Music_Init(void);
void Music_Destroy(void);

#endif
