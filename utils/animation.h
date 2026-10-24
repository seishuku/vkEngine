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

// Crossfade time in seconds when the clip changes
#define ANIM_BLEND_TIME			0.2f

typedef struct Animator_s Animator_t;

typedef void (*AnimatorCallback_t)(Animator_t *animator, void *userData);
typedef void (*AnimatorPostPoseFunc_t)(Animator_t *animator, BAnim_BoneFrame_t *pose, float dt, void *userData);

struct Animator_s
{
	bool active;

	const BModel_t *model;
	const BAnim_t *clip;

	float time;
	float speed;
	bool loop;
	bool playing;

	float blendTime, blendDuration;
	bool hasPose;
	BAnim_BoneFrame_t lastPose[ANIM_MAX_BONE];
	BAnim_BoneFrame_t fromPose[ANIM_MAX_BONE];

	AnimatorCallback_t onFinish;
	void *onFinishUserData;
	bool finished;

	AnimatorPostPoseFunc_t postPose;
	void *postPoseUserData;

	uint32_t paletteOffset;
};

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
void Animation_Clear(AnimationSystem_t *system);
Animator_t *Animation_Get(AnimationSystem_t *system, uint32_t handle);

void Animator_Play(Animator_t *animator, const BAnim_t *clip, bool loop);
void Animator_Stop(Animator_t *animator);
void Animator_SetOnFinish(Animator_t *animator, AnimatorCallback_t callback, void *userData);
void Animator_SetPostPose(Animator_t *animator, AnimatorPostPoseFunc_t callback, void *userData);

void Animation_Update(AnimationSystem_t *system, float dt, uint32_t frameIndex);

#endif
