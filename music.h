#ifndef __MUSIC_H__
#define __MUSIC_H__

const char *GetCurrentMusicTrack(void);

void StartStreamCallback(void *arg);
void StopStreamCallback(void *arg);
void PrevTrackCallback(void *arg);
void NextTrackCallback(void *arg);

void Music_Init(void);
void Music_Destroy(void);

#endif
