#ifndef __ID_H__
#define __ID_H__

#include <stdint.h>

#ifndef ID_MAX
#define ID_MAX 8192
#endif

#ifndef ID_BITS
#define ID_BITS (sizeof(uint32_t)*8)
#endif

#ifndef ID_WORDS
#define ID_WORDS ((ID_MAX+ID_BITS-1)/ID_BITS)
#endif

typedef uint32_t ID_t[ID_WORDS];

void ID_Init(ID_t pool);
uint32_t ID_Generate(ID_t pool);
void ID_Remove(ID_t pool, int id);

#endif
