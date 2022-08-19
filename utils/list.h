#ifndef __LIST_H__
#define __LIST_H__

typedef struct
{
    size_t Size;
	size_t bufSize;
	size_t Stride;
    uint8_t *Buffer;
} List_t;

bool List_Init(List_t *List, const size_t Stride, const size_t Count, const void *Data);
bool List_Add(List_t *List, void *Data);
bool List_Del(List_t *List, const size_t Index);
void *List_GetPointer(List_t *List, const size_t Index);
void List_GetCopy(List_t *List, const size_t Index, void *Data);
size_t List_GetCount(List_t *List);
void *List_GetBufferPointer(List_t *List);
bool List_ShrinkFit(List_t *List);
void List_Clear(List_t *List);
void List_Destroy(List_t *List);

#endif
