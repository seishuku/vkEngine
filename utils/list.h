#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    size_t size;
	size_t bufSize;
	size_t stride;
    uint8_t *buffer;
} List_t;

bool List_Init(List_t *list, const size_t stride, const size_t count, const void *data);
bool List_Add(List_t *list, void *data);
bool List_Del(List_t *list, const size_t index);
void *List_GetPointer(List_t *list, const size_t index);
void List_GetCopy(List_t *list, const size_t index, void *data);
size_t List_GetCount(List_t *list);
void *List_GetBufferPointer(List_t *list);
bool List_ShrinkFit(List_t *list);
void List_Clear(List_t *list);
void List_Destroy(List_t *list);

#endif
