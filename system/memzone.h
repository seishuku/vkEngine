#ifndef __MEMZONE_H__
#define __MEMZONE_H__

#include <threads.h>
#include <stdbool.h>

typedef struct
{
	mtx_t mutex;

	size_t allocations;
	size_t size;
	void *memory;
} MemZone_t;

MemZone_t *Zone_Init(size_t size);
void Zone_Destroy(MemZone_t *zone);
void Zone_Free(MemZone_t *zone, void *ptr);
void *Zone_Malloc(MemZone_t *zone, size_t size);
void *Zone_Calloc(MemZone_t *zone, size_t size, size_t count);
void *Zone_Realloc(MemZone_t *zone, void *ptr, size_t size);
bool Zone_VerifyHeap(MemZone_t *zone);
void Zone_Print(MemZone_t *zone);

#endif
