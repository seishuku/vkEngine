#ifndef __MEMZONE_H__
#define __MEMZONE_H__

typedef struct MemBlock_s
{
	size_t Size;
	bool Free;
	struct MemBlock_s *Next, *Prev;
} MemBlock_t;

typedef struct
{
	size_t Size;
	MemBlock_t Blocks;
	MemBlock_t *Current;
} MemZone_t;

MemZone_t *Zone_Init(size_t Size);
void Zone_Destroy(MemZone_t *Zone);
void Zone_Free(MemZone_t *Zone, void *Ptr);
void *Zone_Malloc(MemZone_t *Zone, size_t Size);
void *Zone_Realloc(MemZone_t *Zone, void *Ptr, size_t Size);
void Zone_Print(MemZone_t *Zone);

#endif
