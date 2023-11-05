#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>
#include <stdio.h>
#include "memzone.h"

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

#ifdef ANDROID
#include <android/log.h>
#ifndef DBGPRINTF
#define DBGPRINTF(level, ...) __android_log_print(ANDROID_LOG_INFO, "vkEngine", level __VA_ARGS__)
#endif
#else
#ifndef DBGPRINTF
#define DBGPRINTF(level, ...) fprintf(stderr, level __VA_ARGS__)
#endif
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

extern MemZone_t *Zone;

double GetClock(void);

#endif
