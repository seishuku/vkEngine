#ifndef __SYSTEM_H__
#define __SYSTEM_H__

#include <stdint.h>
#include <stdarg.h>
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
inline static void DBGPRINTF(const char *level, const char *format, ...)
{
	char string[1024];
	va_list	ap;

	va_start(ap, format);
	vsnprintf(string, 1023, format, ap);
	va_end(ap);

	__android_log_print(ANDROID_LOG_INFO, "vkEngine", "%s%s%s", level, string, DEBUG_NONE);
}
#endif
#else
#ifndef DBGPRINTF
inline static void DBGPRINTF(const char *level, const char *format, ...)
{
	char string[1024];
	va_list	ap;

	va_start(ap, format);
	vsnprintf(string, 1023, format, ap);
	va_end(ap);

	fprintf(stderr, "%s%s%s", level, string, DEBUG_NONE);
}
#endif
#endif

#ifndef BUFFER_OFFSET
#define BUFFER_OFFSET(x) ((char *)NULL+(x))
#endif

extern MemZone_t *zone;

double GetClock(void);

#endif
