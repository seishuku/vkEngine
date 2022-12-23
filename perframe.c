#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "model/bmodel.h"
#include "skybox.h"
#include "models.h"
#include "perframe.h"

PerFrame_t PerFrame[VKU_MAX_FRAME_COUNT];
