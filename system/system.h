#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#ifndef DEBUG_ERROR
#define DEBUG_ERROR "\x1B[91m"
#endif

#ifndef DEBUG_WARNING
#define DEBUG_WARNING "\x1B[93m"
#endif

#ifndef DEBUG_INFO
#define DEBUG_INFO "\x1B[92m"
#endif

#ifndef DEBUG_NONE
#define DEBUG_NONE "\x1B[97m"
#endif

#ifndef DBGPRINTF
#define DBGPRINTF(level, ...) fprintf(stderr, level __VA_ARGS__)
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

#include "../utils/memzone.h"

extern MemZone_t *Zone;

#endif