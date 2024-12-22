#ifndef ANDROID_FOPEN_H
#define ANDROID_FOPEN_H

#include <stdio.h>

FILE *android_fopen(const char *fname, const char *mode);

#define fopen(name, mode) android_fopen(name, mode)

#endif
