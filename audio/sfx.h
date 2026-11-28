#ifndef __SFX_H__
#define __SFX_H__

#include <stdbool.h>
#include "../math/math.h"

void SFX_Init(void);
void SFX_Destroy(void);
void SFX_PlayFootstep(vec3 position, bool running);

#endif
