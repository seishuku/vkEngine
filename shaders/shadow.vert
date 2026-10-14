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

#define NO_PALETTE 0xFFFFFFFFu

// layout (binding=0) readonly buffer palette
// {
// 	mat4 skin[];
// };

layout (push_constant) uniform ubo
{
    mat4 mvp;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
   	vec4 position=vec4(vPosition.xyz, 1.0);

	// if(iPaletteOffset!=NO_PALETTE&&dot(vBoneWeight, vec4(1.0))>0.0)
	// {
	// 	const mat4 skinMatrix=
	// 	    skin[iPaletteOffset+vBoneIndex.x]*vBoneWeight.x+
	// 	    skin[iPaletteOffset+vBoneIndex.y]*vBoneWeight.y+
	// 	    skin[iPaletteOffset+vBoneIndex.z]*vBoneWeight.z+
	// 	    skin[iPaletteOffset+vBoneIndex.w]*vBoneWeight.w;

	// 	position=skinMatrix*position;
	// }

	gl_Position=mvp*iTransform*position;
}
