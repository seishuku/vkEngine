#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../perframe.h"
#include "animation.h"

extern VkuContext_t vkContext;

bool Animation_Init(AnimationSystem_t *system)
{
	memset(system, 0, sizeof(*system));

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(!vkuCreateHostBuffer(&vkContext, &system->perFrame[i].paletteBuffer, sizeof(matrix)*ANIM_MAX_PALETTE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
		{
			Animation_Destroy(system);
			return false;
		}

		system->perFrame[i].palette=system->perFrame[i].paletteBuffer.memory->mappedPointer;
	}

	return true;
}

void Animation_Destroy(AnimationSystem_t *system)
{
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(system->perFrame[i].paletteBuffer.buffer)
			vkuDestroyBuffer(&vkContext, &system->perFrame[i].paletteBuffer);
	}

	memset(system, 0, sizeof(*system));
}

uint32_t Animation_Create(AnimationSystem_t *system, const BModel_t *model, const BAnim_t *clip)
{
	if(!model||model->numBone==0||model->numBone>ANIM_MAX_BONE)
	{
		DBGPRINTF(DEBUG_ERROR, "Animation_Create: bone count mismatch.\n");
		return UINT32_MAX;
	}

	uint32_t handle;

	if(system->freeCount)
		handle=system->freeList[--system->freeCount];
	else if(system->highWater<ANIM_MAX_ANIMATORS)
		handle=system->highWater++;
	else
	{
		DBGPRINTF(DEBUG_ERROR, "Animation_Create: out of animator space.\n");
		return UINT32_MAX;
	}

	system->animators[handle]=(Animator_t)
	{
		.active=true,
		.model=model,
		.clip=clip,
		.time=0.0f,
		.speed=1.0f,
		.loop=true,
		.playing=(clip!=NULL),
	};

	return handle;
}

void Animation_Free(AnimationSystem_t *system, uint32_t handle)
{
	if(handle>=system->highWater||!system->animators[handle].active)
		return;

	system->animators[handle].active=false;
	system->freeList[system->freeCount++]=handle;
}

void Animation_Clear(AnimationSystem_t *system)
{
	memset(system->animators, 0, sizeof(system->animators));
	system->freeCount=0;
	system->highWater=0;
}

Animator_t *Animation_Get(AnimationSystem_t *system, uint32_t handle)
{
	if(handle>=system->highWater||!system->animators[handle].active)
		return NULL;

	return &system->animators[handle];
}

void Animator_Play(Animator_t *animator, const BAnim_t *clip, bool loop)
{
	if((clip&&animator)&&clip==animator->clip&&animator->playing)
	{
		animator->loop=loop;
		return;
	}

	animator->onFinish=NULL;
	animator->userData=NULL;

	if(ANIM_BLEND_TIME>0.0f&&animator->hasPose)
	{
		memcpy(animator->fromPose, animator->lastPose, sizeof(BAnim_BoneFrame_t)*animator->model->numBone);
		animator->blendTime=0.0f;
		animator->blendDuration=ANIM_BLEND_TIME;
	}
	else
		animator->blendDuration=0.0f;

	animator->clip=clip;
	animator->time=0.0f;
	animator->loop=loop;
	animator->playing=true;
}

void Animator_Stop(Animator_t *animator)
{
	animator->playing=false;
}

void Animator_SetOnFinish(Animator_t *animator, AnimatorCallback_t callback, void *userData)
{
	if(!animator)
		return;

	animator->onFinish=callback;
	animator->userData=userData;
}

static void BindPose(const BModel_t *model, BAnim_BoneFrame_t *out)
{
	for(uint32_t b=0;b<model->numBone;b++)
	{
		out[b].position=model->bone[b].position;
		out[b].orientation=model->bone[b].orientation;
	}
}

static void SamplePose(const BAnim_t *clip, float time, bool loop, uint32_t numBone, BAnim_BoneFrame_t *out)
{
	const float position=time*clip->frameRate;
	uint32_t frameA=(uint32_t)position;
	float t=position-(float)frameA;
	uint32_t frameB=frameA+1;

	if(loop)
	{
		frameA%=clip->numFrame;
		frameB%=clip->numFrame;
	}
	else if(frameA>=clip->numFrame-1)
	{
		frameA=frameB=clip->numFrame-1;
		t=0.0f;
	}

	const BAnim_BoneFrame_t *a=&clip->frame[frameA*clip->numBone];
	const BAnim_BoneFrame_t *b=&clip->frame[frameB*clip->numBone];

	for(uint32_t i=0;i<numBone;i++)
	{
		out[i].orientation=QuatSlerp(a[i].orientation, b[i].orientation, t);
		out[i].position=Vec3_Lerp(a[i].position, b[i].position, t);
	}
}

static void AdvanceTime(Animator_t *animator, float dt)
{
	const BAnim_t *clip=animator->clip;

	if(!animator->playing||!clip||clip->numFrame==0||clip->frameRate<=0.0f)
		return;

	float time=animator->time+dt*animator->speed;

	if(animator->loop)
	{
		const float length=(float)clip->numFrame/clip->frameRate;
		const float wraps=floorf(time/length);

		time-=wraps*length;

		if(wraps!=0.0f)
			animator->finished=true;
	}
	else
	{
		const float end=(float)(clip->numFrame-1)/clip->frameRate;

		if(time>=end)
		{
			time=end;
			animator->playing=false;
			animator->finished=true;
		}
		else if(time<0.0f)
		{
			time=0.0f;
			animator->playing=false;
			animator->finished=true;
		}
	}

	animator->time=time;
}

void Animation_Update(AnimationSystem_t *system, float dt, uint32_t frameIndex)
{
	matrix *palette=system->perFrame[frameIndex].palette;
	uint32_t offset=0;

	for(uint32_t i=0;i<system->highWater;i++)
	{
		Animator_t *animator=&system->animators[i];

		if(!animator->active)
			continue;

		const BModel_t *model=animator->model;
		const uint32_t numBone=model->numBone;

		if(offset+numBone>ANIM_MAX_PALETTE)
		{
			DBGPRINTF(DEBUG_ERROR, "Animation_Update: palette full, animator %u skipped.\n", i);
			animator->paletteOffset=UINT32_MAX;
			continue;
		}

		AdvanceTime(animator, dt);

		BindPose(model, system->pose);

		if(animator->clip&&animator->clip->numFrame>0)
			SamplePose(animator->clip, animator->time, animator->loop, (numBone<animator->clip->numBone)?numBone:animator->clip->numBone, system->pose);

		if(animator->blendDuration>0.0f)
		{
			animator->blendTime+=fabsf(dt*animator->speed);

			float t=animator->blendTime/animator->blendDuration;

			if(t>=1.0f)
				animator->blendDuration=0.0f;
			else
			{
				t=t*t*(3.0f-2.0f*t);	// Smoothstep

				for(uint32_t b=0;b<numBone;b++)
				{
					system->pose[b].position=Vec3_Lerp(animator->fromPose[b].position, system->pose[b].position, t);
					system->pose[b].orientation=QuatSlerp(animator->fromPose[b].orientation, system->pose[b].orientation, t);
				}
			}
		}

		memcpy(animator->lastPose, system->pose, sizeof(BAnim_BoneFrame_t)*numBone);
		animator->hasPose=true;

		for(uint32_t b=0;b<numBone;b++)
		{
			matrix local=MatrixMult(QuatToMatrix(system->pose[b].orientation), MatrixTranslatev(system->pose[b].position));

			if(model->bone[b].parent>=0)
				local=MatrixMult(local, system->world[model->bone[b].parent]);

			system->world[b]=local;

			palette[offset+b]=MatrixMult(model->bone[b].inverseBind, local);
		}

		animator->paletteOffset=offset;
		offset+=numBone;
	}

	for(uint32_t i=0;i<system->highWater;i++)
	{
		Animator_t *animator=&system->animators[i];

		if(!animator->active||!animator->finished)
			continue;

		animator->finished=false;

		if(animator->onFinish)
			animator->onFinish(animator, animator->userData);
	}
}
