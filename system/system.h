#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#ifndef DBGPRINTF
#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

#include "../utils/memzone.h"

extern MemZone_t *Zone;

#endif