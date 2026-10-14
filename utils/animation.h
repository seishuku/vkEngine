#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "../model/bmodel.h"
#include "../model/banim.h"

#define ANIM_MAX_ANIMATORS		512
#define ANIM_MAX_BONE			128
#define ANIM_MAX_PALETTE		(ANIM_MAX_ANIMATORS*32)

typedef struct
{
	bool active;

	const BModel_t *model;
	const BAnim_t *clip;

	float time;
	float speed;
	bool loop;
	bool playing;

	uint32_t paletteOffset;
} Animator_t;

typedef struct
{
	Animator_t animators[ANIM_MAX_ANIMATORS];

	uint32_t freeList[ANIM_MAX_ANIMATORS];
	uint32_t freeCount;
	uint32_t highWater;

	BAnim_BoneFrame_t pose[ANIM_MAX_BONE];
	matrix world[ANIM_MAX_BONE];

	struct
	{
		VkuBuffer_t paletteBuffer;
		matrix *palette;
	} perFrame[VKU_MAX_FRAME_COUNT];
} AnimationSystem_t;

bool Animation_Init(AnimationSystem_t *system);
void Animation_Destroy(AnimationSystem_t *system);

uint32_t Animation_Create(AnimationSystem_t *system, const BModel_t *model, const BAnim_t *clip);
void Animation_Free(AnimationSystem_t *system, uint32_t handle);
Animator_t *Animation_Get(AnimationSystem_t *system, uint32_t handle);

void Animator_Play(Animator_t *animator, const BAnim_t *clip, bool loop);
void Animator_Stop(Animator_t *animator);

void Animation_Update(AnimationSystem_t *system, float dt, uint32_t frameIndex);

#endif
