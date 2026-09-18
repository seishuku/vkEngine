#ifndef BANIM_H
#define BANIM_H

#include <stdint.h>
#include "../math/math.h"

typedef struct
{
    vec3 position;
    vec4 orientation;
} BAnim_BoneFrame_t;

typedef struct
{
    char name[256];

    float duration;
    float frameRate;

    uint32_t numFrame;
    uint32_t numBone;

    BAnim_BoneFrame_t *frame;
} BAnim_t;

bool LoadBAnim(BAnim_t *animation, const char *filename);
void FreeBAnim(BAnim_t *animation);

#endif
