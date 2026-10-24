#ifndef __IK_H__
#define __IK_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "animation.h"

#define IK_MAX_CHAIN	8
#define IK_MAX_CHAINS	4

typedef struct IK_s IK_t;

typedef bool (*IKGroundFunc_t)(IK_t *ik, uint32_t chain, vec3 chainTip, float *offset);

typedef struct
{
	int32_t bone[IK_MAX_CHAIN];	// Root to tip, each bone the parent of the next (thigh, calf, foot)
	uint32_t numBone;
} IKChain_t;

struct IK_s
{
	Animator_t *animator;

	IKChain_t chain[IK_MAX_CHAINS];
	uint32_t numChain;
	float offset[IK_MAX_CHAINS];

	int32_t pelvisBone;		// -1 = no pelvis adjustment
	vec3 up;				// Model space up
	float maxStep;			// Clamp on ground offsets (model units)
	float smoothRate;		// Higher is faster
	float weight;			// 0 to 1 blend weight, eased by the smoothing

	IKGroundFunc_t groundFunc;
	void *userData;
};

void IK_Init(IK_t *ik, Animator_t *animator);

int32_t IK_AddChain(IK_t *ik, const char *const *boneNames, uint32_t numBone);
bool IK_SetPelvis(IK_t *ik, const char *boneName);

#endif
