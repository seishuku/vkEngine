#ifndef __MEMZONE_H__
#define __MEMZONE_H__

#include <pthread.h>

typedef struct
{
	pthread_mutex_t Mutex;

	size_t Allocations;
	size_t Size;
	void *Memory;
} MemZone_t;

MemZone_t *Zone_Init(size_t Size);
void Zone_Destroy(MemZone_t *Zone);
void Zone_Free(MemZone_t *Zone, void *Ptr);
void *Zone_Malloc(MemZone_t *Zone, size_t Size);
void *Zone_Calloc(MemZone_t *Zone, size_t Size, size_t Count);
void *Zone_Realloc(MemZone_t *Zone, void *Ptr, size_t Size);
void Zone_Print(MemZone_t *Zone);

#endif
