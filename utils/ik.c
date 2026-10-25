#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include "../system/system.h"
#include "../math/math.h"
#include "animation.h"
#include "ik.h"

typedef struct
{
	vec3 position;
	float length;
} IKJoint_t;

static void ComputeModelPose(const BModel_t *model, const BAnim_BoneFrame_t *local, BAnim_BoneFrame_t *pose)
{
	for(uint32_t i=0;i<model->numBone;i++)
	{
		const int32_t parent=model->bone[i].parent;

		if(parent<0)
		{
			pose[i].position=local[i].position;
			pose[i].orientation=local[i].orientation;
		}
		else
		{
			pose[i].position=Vec3_Addv(pose[parent].position, QuatRotate(pose[parent].orientation, local[i].position));
			pose[i].orientation=QuatMultiply(pose[parent].orientation, local[i].orientation);
		}
	}
}

static void SolveFABRIK(IKJoint_t *joints, const uint32_t numJoint, const vec3 target, const int maxIterations, const float tolerance)
{
	const vec3 root=joints[0].position;
	float totalLength=0.0f;

	for(uint32_t i=0;i<numJoint-1;i++)
		totalLength+=joints[i].length;

	const vec3 toTarget=Vec3_Subv(target, root);
	const float dist=Vec3_Length(toTarget);

	if(dist>=totalLength&&dist>FLT_EPSILON)
	{
		const vec3 dir=Vec3_Muls(toTarget, 1.0f/dist);

		for(uint32_t i=1;i<numJoint;i++)
			joints[i].position=Vec3_Addv(joints[i-1].position, Vec3_Muls(dir, joints[i-1].length));

		return;
	}

	for(int iter=0;iter<maxIterations;iter++)
	{
		if(Vec3_Distance(joints[numJoint-1].position, target)<tolerance)
			return;

		joints[numJoint-1].position=target;

		for(int32_t i=(int32_t)numJoint-2;i>=0;i--)
		{
			vec3 dir=Vec3_Subv(joints[i].position, joints[i+1].position);
			Vec3_Normalize(&dir);

			joints[i].position=Vec3_Addv(joints[i+1].position, Vec3_Muls(dir, joints[i].length));
		}

		joints[0].position=root;

		for(uint32_t i=1;i<numJoint;i++)
		{
			vec3 dir=Vec3_Subv(joints[i].position, joints[i-1].position);
			Vec3_Normalize(&dir);

			joints[i].position=Vec3_Addv(joints[i-1].position, Vec3_Muls(dir, joints[i-1].length));
		}
	}
}

static void ApplyPole(IKJoint_t *joints, const uint32_t numJoint, const vec3 poleDir)
{
	const vec3 root=joints[0].position;

	vec3 axis=Vec3_Subv(joints[numJoint-1].position, root);

	if(Vec3_Normalize(&axis)<FLT_EPSILON)
		return;

	vec3 pole=Vec3_Subv(poleDir, Vec3_Muls(axis, Vec3_Dot(poleDir, axis)));

	if(Vec3_Normalize(&pole)<FLT_EPSILON)
		return;

	for(uint32_t i=1;i<numJoint-1;i++)
	{
		const vec3 rel=Vec3_Subv(joints[i].position, root);
		vec3 perp=Vec3_Subv(rel, Vec3_Muls(axis, Vec3_Dot(rel, axis)));

		if(Vec3_Normalize(&perp)<FLT_EPSILON)
			continue;

		const float sinAngle=Vec3_Dot(axis, Vec3_Cross(perp, pole));
		const float cosAngle=clampf(Vec3_Dot(perp, pole), -1.0f, 1.0f);
		const float angle=atan2f(sinAngle, cosAngle);

		const float c=cosf(angle);
		const float s=sinf(angle);

		joints[i].position=Vec3_Addv(root, Vec3_Addv(Vec3_Addv(Vec3_Muls(rel, c), Vec3_Muls(Vec3_Cross(axis, rel), s)), Vec3_Muls(axis, Vec3_Dot(axis, rel)*(1.0f-c))));
	}
}

static void SolveChain(const BModel_t *model, BAnim_BoneFrame_t *pose, const BAnim_BoneFrame_t *postPose, const IKChain_t *chain, const vec3 target)
{
	const uint32_t n=chain->numBone;
	IKJoint_t joints[IK_MAX_CHAIN];
	BAnim_BoneFrame_t boneFrame[IK_MAX_CHAIN];

	for(uint32_t k=0;k<n;k++)
	{
		boneFrame[k].position=postPose[chain->bone[k]].position;
		joints[k].position=boneFrame[k].position;
	}

	float totalLength=0.0f;

	for(uint32_t k=0;k<n-1;k++)
	{
		joints[k].length=Vec3_Distance(boneFrame[k].position, boneFrame[k+1].position);
		totalLength+=joints[k].length;
	}

	joints[n-1].length=0.0f;

	vec3 pole=Vec3(0.0f, 0.0f, 0.0f);
	vec3 axis=Vec3_Subv(boneFrame[n-1].position, boneFrame[0].position);

	if(n>=3&&Vec3_Normalize(&axis)>FLT_EPSILON)
	{
		const vec3 rel=Vec3_Subv(boneFrame[1].position, boneFrame[0].position);
		pole=Vec3_Subv(rel, Vec3_Muls(axis, Vec3_Dot(rel, axis)));
	}

	SolveFABRIK(joints, n, target, 3, 0.001f);

	if(Vec3_Length(pole)>0.02f*totalLength)
	{
		ApplyPole(joints, n, pole);

		// Restore segment lengths and the endpoint constraint.
		// TODO: Is this really needed?
		SolveFABRIK(joints, n, target, 3, 0.001f);
	}

	vec4 swing=Vec4(0.0f, 0.0f, 0.0f, 1.0f);

	for(uint32_t k=0;k<n-1;k++)
	{
		vec3 oldDir=Vec3_Subv(boneFrame[k+1].position, boneFrame[k].position);
		vec3 newDir=Vec3_Subv(joints[k+1].position, joints[k].position);

		if(Vec3_Normalize(&oldDir)<FLT_EPSILON||Vec3_Normalize(&newDir)<FLT_EPSILON)
			continue;

		const float d=clampf(Vec3_Dot(oldDir, newDir), -1.0f, 1.0f);

		if(d>0.99999f)
			swing=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		else if(d<-0.99999f)
		{
			vec3 basis;

			if(fabsf(oldDir.x)<=fabsf(oldDir.y)&&fabsf(oldDir.x)<=fabsf(oldDir.z))
				basis=Vec3(1.0f, 0.0f, 0.0f);
			else if(fabsf(oldDir.y)<=fabsf(oldDir.z))
				basis=Vec3(0.0f, 1.0f, 0.0f);
			else
				basis=Vec3(0.0f, 0.0f, 1.0f);

			vec3 axis=Vec3_Cross(oldDir, basis);
			Vec3_Normalize(&axis);

			swing=Vec4(axis.x, axis.y, axis.z, 0.0f);
		}
		else
		{
			const vec3 c=Vec3_Cross(oldDir, newDir);

			swing=Vec4(c.x, c.y, c.z, 1.0f+d);
			Vec4_Normalize(&swing);
		}

		boneFrame[k].orientation=QuatMultiply(swing, postPose[chain->bone[k]].orientation);
	}

	boneFrame[n-1].orientation=QuatMultiply(swing, postPose[chain->bone[n-1]].orientation);

	const int32_t parent=model->bone[chain->bone[0]].parent;
	vec4 parentRot=(parent<0)?Vec4(0.0f, 0.0f, 0.0f, 1.0f):postPose[parent].orientation;

	for(uint32_t k=0;k<n;k++)
	{
		pose[chain->bone[k]].orientation=QuatMultiply(QuatInverse(parentRot), boneFrame[k].orientation);
		parentRot=boneFrame[k].orientation;
	}
}

static void PostPose(Animator_t *animator, BAnim_BoneFrame_t *pose, float dt, void *userData)
{
	IK_t *ik=(IK_t *)userData;
	const BModel_t *model=animator->model;

	if(!ik->numChain||!model)
		return;

	BAnim_BoneFrame_t posePose[ANIM_MAX_BONE];
	vec3 targets[IK_MAX_CHAINS];

	ComputeModelPose(model, pose, posePose);

	ik->modelPose=posePose;

	float lowest=0.0f, total=0.0f;
	uint32_t numSupport=0;
	bool active=false;

	for(uint32_t c=0;c<ik->numChain;c++)
	{
		IKChain_t *chain=&ik->chain[c];
		const vec3 chainTip=posePose[chain->bone[chain->numBone-1]].position;
		IKTarget_t target;
		vec3 desired=Vec3b(0.0f);

		// No target (or no callback) means no change, which eases the chain back to the animation
		if(chain->targetFunc&&chain->targetFunc(ik, c, chainTip, &target))
		{
			desired=Vec3_Subv(target.position, chainTip);

			const float length=Vec3_Length(desired);

			if(chain->maxDistance>0.0f&&length>chain->maxDistance)
				desired=Vec3_Muls(desired, chain->maxDistance/length);

			desired=Vec3_Muls(desired, chain->weight*ik->weight);
		}

		// Ease the change, not the target, so the tip is free to move
		const float blend=(chain->smoothRate>0.0f)?1.0f-expf(-chain->smoothRate*dt):1.0f;

		chain->delta=Vec3_Lerp(chain->delta, desired, blend);
		targets[c]=Vec3_Addv(chainTip, chain->delta);

		if(Vec3_Dot(chain->delta, chain->delta)>1e-8f)
			active=true;

		const float shift=Vec3_Dot(chain->delta, ik->up);

		lowest=fminf(lowest, shift);
		total+=shift;
		numSupport++;
	}

	ik->modelPose=NULL;

	if(!active)
		return;

	if(ik->bodyBone>=0&&numSupport)
	{
		const float shift=(ik->bodyMode==IK_BODY_AVERAGE)?total/(float)numSupport:lowest;

		if(fabsf(shift)>FLT_EPSILON)
		{
			const int32_t parent=model->bone[ik->bodyBone].parent;
			const vec4 parentRot=(parent<0)?Vec4(0.0f, 0.0f, 0.0f, 1.0f):posePose[parent].orientation;

			pose[ik->bodyBone].position=Vec3_Addv(pose[ik->bodyBone].position, QuatRotate(QuatInverse(parentRot), Vec3_Muls(ik->up, shift)));

			ComputeModelPose(model, pose, posePose);
		}
	}

	for(uint32_t c=0;c<ik->numChain;c++)
		SolveChain(model, pose, posePose, &ik->chain[c], targets[c]);
}

void IK_Init(IK_t *ik, Animator_t *animator)
{
	memset(ik, 0, sizeof(IK_t));

	ik->animator=animator;
	ik->bodyBone=-1;
	ik->bodyMode=IK_BODY_LOWEST;
	ik->up=Vec3(0.0f, 1.0f, 0.0f);
	ik->weight=1.0f;

	if(animator)
	{
		animator->postPose=PostPose;
		animator->postPoseUserData=ik;
	}
}

static int32_t FindBone(const BModel_t *model, const char *name)
{
	for(uint32_t i=0;i<model->numBone;i++)
	{
		if(strcmp(model->bone[i].name, name)==0)
			return (int32_t)i;
	}

	return -1;
}

int32_t IK_AddChain(IK_t *ik, const char *const *boneNames, uint32_t numBone, IKTargetFunc_t targetFunc, void *userData)
{
	if(!ik||!ik->animator||!ik->animator->model||numBone<2||numBone>IK_MAX_CHAIN||ik->numChain>=IK_MAX_CHAINS)
		return -1;

	const BModel_t *model=ik->animator->model;
	IKChain_t chain;

	memset(&chain, 0, sizeof(IKChain_t));

	for(uint32_t i=0;i<numBone;i++)
	{
		chain.bone[i]=FindBone(model, boneNames[i]);

		if(chain.bone[i]<0)
		{
			DBGPRINTF(DEBUG_ERROR, "IK_AddChain: bone '%s' not found.\n", boneNames[i]);
			return -1;
		}

		if(i>0&&model->bone[chain.bone[i]].parent!=chain.bone[i-1])
		{
			DBGPRINTF(DEBUG_ERROR, "IK_AddChain: bone '%s' is not a child of '%s'.\n", boneNames[i], boneNames[i-1]);
			return -1;
		}
	}

	chain.numBone=numBone;
	chain.targetFunc=targetFunc;
	chain.userData=userData;
	chain.weight=1.0f;
	chain.maxDistance=0.4f;
	chain.smoothRate=15.0f;

	ik->chain[ik->numChain]=chain;

	return (int32_t)ik->numChain++;
}

bool IK_SetBody(IK_t *ik, const char *boneName, IKBodyMode_e mode)
{
	if(!ik||!ik->animator||!ik->animator->model)
		return false;

	ik->bodyBone=FindBone(ik->animator->model, boneName);
	ik->bodyMode=mode;

	return ik->bodyBone>=0;
}
