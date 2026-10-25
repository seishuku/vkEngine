#ifndef __IK_H__
#define __IK_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "animation.h"

#define IK_MAX_CHAIN	8
#define IK_MAX_CHAINS	8

typedef struct IK_s IK_t;

// Target function output structure, this can be extended later
typedef struct
{
	vec3 position;		// Tip of the chain target
} IKTarget_t;

typedef bool (*IKTargetFunc_t)(IK_t *ik, uint32_t chain, const vec3 chainTip, IKTarget_t *target);

typedef enum
{
	IK_BODY_LOWEST=0,	// Body drops to the lowest supporting chain and never rises, higher ones just bend more (bipeds)
	IK_BODY_AVERAGE		// Body follows the average pull of the supporting chains, up or down (non-bipedal models?)
} IKBodyMode_e;

typedef struct
{
	int32_t bone[IK_MAX_CHAIN];	// Root to tip, each bone the parent of the next (thigh, calf, foot)
	uint32_t numBone;

	IKTargetFunc_t targetFunc;
	void *userData;

	float weight;				// 0 to 1, how much of the target is applied
	float maxDistance;			// Max distance the target tip can reach
	float smoothRate;			// How fast it eases in and out

	vec3 delta;
} IKChain_t;

struct IK_s
{
	Animator_t *animator;

	IKChain_t chain[IK_MAX_CHAINS];
	uint32_t numChain;

	int32_t bodyBone;		// -1 is no body shift. This is the bone that moves when supporting chains move (pelvis, vehicle's chassis, etc).
	IKBodyMode_e bodyMode;

	vec3 up;				// Model space up, the body shifts along this
	float weight;			// 0 to 1 for all chains together, eased by each chain's smoothing

	const BAnim_BoneFrame_t *modelPose;			// Animated model space pose of every bone, only valid inside a target callback
};

void IK_Init(IK_t *ik, Animator_t *animator);

// Names are root to tip, returns the chain index or -1 if a bone is missing or aren't in order.
int32_t IK_AddChain(IK_t *ik, const char *const *boneNames, uint32_t numBone, IKTargetFunc_t targetFunc, void *userData);
bool IK_SetBody(IK_t *ik, const char *boneName, IKBodyMode_e mode);

#endif
