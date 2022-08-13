#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#ifdef WIN32
#include <Windows.h>
#define DBGPRINTF(...) { char buf[512]; snprintf(buf, sizeof(buf), __VA_ARGS__); OutputDebugString(buf); }
#else
#define DBGPRINTF(...) { fprintf(stderr, __VA_ARGS__); }
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#endif