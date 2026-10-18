#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;
layout (location=5) in uvec4 vBoneIndex;
layout (location=6) in vec4 vBoneWeight;

layout (location=7) in mat4 iTransform;
layout (location=11) in uint iPaletteOffset;

#define NUM_CASCADES 4
#define NO_PALETTE 0xFFFFFFFFu

layout (binding=3) uniform ubo
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

layout (binding=5) readonly buffer palette
{
	mat4 skin[];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 oPosition;
layout (location=1) out vec2 oUV;
layout (location=2) out mat3 oTangent;
layout (location=5) out float oViewDepth;
layout (location=6) out vec4 oShadow[NUM_CASCADES];

void main()
{
	vec4 position=vec4(vPosition.xyz, 1.0);
	vec3 tangent=vTangent.xyz;
	vec3 binormal=vBinormal.xyz;
	vec3 normal=vNormal.xyz;

	if(iPaletteOffset!=NO_PALETTE&&dot(vBoneWeight, vec4(1.0))>0.0)
	{
		const mat4 skinMatrix=
		    skin[iPaletteOffset+vBoneIndex.x]*vBoneWeight.x+
		    skin[iPaletteOffset+vBoneIndex.y]*vBoneWeight.y+
		    skin[iPaletteOffset+vBoneIndex.z]*vBoneWeight.z+
		    skin[iPaletteOffset+vBoneIndex.w]*vBoneWeight.w;

		const mat3 skinRotation=mat3(skinMatrix);

		position=skinMatrix*position;
		tangent=skinRotation*tangent;
		binormal=skinRotation*binormal;
		normal=skinRotation*normal;
	}

	oPosition=(iTransform*position).xyz;
	vec4 worldPosition=modelview*vec4(oPosition, 1.0);

	gl_Position=projection*HMD*worldPosition;

	oViewDepth=-worldPosition.z;
	oUV=vUV.xy;

	// Correct for non-uniform scaling causing shearing
	mat3 M=transpose(inverse(mat3(iTransform)));
	oTangent=mat3(normalize(M*tangent.xyz), normalize(M*binormal.xyz), normalize(M*normal.xyz));

	const mat4 biasMat=mat4(
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );

	for(int i=0;i<NUM_CASCADES;i++)
		oShadow[i]=biasMat*lightMVP[i]*vec4(oPosition.xyz, 1.0);
}
