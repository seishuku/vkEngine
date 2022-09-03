#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#ifdef WIN32
#include <Windows.h>
#define DBGPRINTF(...) { static char buf[4096]; snprintf(buf, sizeof(buf), __VA_ARGS__); OutputDebugString(buf); }
#else
#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

#include "../utils/memzone.h"

extern MemZone_t *Zone;

#endif