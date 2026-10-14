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

Animator_t *Animation_Get(AnimationSystem_t *system, uint32_t handle)
{
	if(handle>=system->highWater||!system->animators[handle].active)
		return NULL;

	return &system->animators[handle];
}

void Animator_Play(Animator_t *animator, const BAnim_t *clip, bool loop)
{
	animator->clip=clip;
	animator->time=0.0f;
	animator->loop=loop;
	animator->playing=(clip!=NULL);
}

void Animator_Stop(Animator_t *animator)
{
	animator->playing=false;
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

	animator->time+=dt*animator->speed;

	if(animator->loop)
	{
		const float length=(float)clip->numFrame/clip->frameRate;

		animator->time=fmodf(animator->time, length);

		if(animator->time<0.0f)
			animator->time+=length;
	}
	else
	{
		const float end=(float)(clip->numFrame-1)/clip->frameRate;

		if(animator->time>=end)
		{
			animator->time=end;
			animator->playing=false;
		}
		else if(animator->time<0.0f)
		{
			animator->time=0.0f;
			animator->playing=false;
		}
	}
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
			animator->paletteOffset=0;
			continue;
		}

		AdvanceTime(animator, dt);

		if(animator->clip&&animator->clip->numFrame>0)
			SamplePose(animator->clip, animator->time, animator->loop, (numBone<animator->clip->numBone)?numBone:animator->clip->numBone, system->pose);
		else
		{
			for(uint32_t b=0;b<numBone;b++)
			{
				system->pose[b].position=model->bone[b].position;
				system->pose[b].orientation=model->bone[b].orientation;
			}
		}

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
}
