#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;
layout (location=5) in vec4 vBoneIndex;
layout (location=6) in vec4 vBoneWeight;

#define NUM_CASCADES 4

layout (binding=0) uniform ubo
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

#define MAX_BONE 128

layout (binding=1) uniform bone
{
	mat4 inverseBind[MAX_BONE];
};

layout (binding=2) uniform animation
{
	mat4 animatedWorld[MAX_BONE];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Normal;

vec4 SkinPosition(vec4 position)
{
    vec4 result=vec4(0.0);
    result+=(animatedWorld[int(vBoneIndex.x)]*inverseBind[int(vBoneIndex.x)])*position*vBoneWeight.x;
    result+=(animatedWorld[int(vBoneIndex.y)]*inverseBind[int(vBoneIndex.y)])*position*vBoneWeight.y;
    result+=(animatedWorld[int(vBoneIndex.z)]*inverseBind[int(vBoneIndex.z)])*position*vBoneWeight.z;
    result+=(animatedWorld[int(vBoneIndex.w)]*inverseBind[int(vBoneIndex.w)])*position*vBoneWeight.w;

    return result;
}

vec3 SkinVector(vec3 vector)
{
    vec3 result=vec3(0.0);
    result+=mat3(animatedWorld[int(vBoneIndex.x)]*inverseBind[int(vBoneIndex.x)])*vector*vBoneWeight.x;
    result+=mat3(animatedWorld[int(vBoneIndex.y)]*inverseBind[int(vBoneIndex.y)])*vector*vBoneWeight.y;
    result+=mat3(animatedWorld[int(vBoneIndex.z)]*inverseBind[int(vBoneIndex.z)])*vector*vBoneWeight.z;
    result+=mat3(animatedWorld[int(vBoneIndex.w)]*inverseBind[int(vBoneIndex.w)])*vector*vBoneWeight.w;

    return normalize(result);
}

void main()
{
	vec4 worldPosition=modelview*SkinPosition(vec4(vPosition.xyz, 1.0));
	gl_Position=projection*HMD*worldPosition;

	Normal=SkinVector(vNormal.xyz);
}
