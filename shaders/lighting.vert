#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;

layout (location=5) in mat4 iPosition;

#define NUM_CASCADES 4

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

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;
layout (location=1) out vec2 UV;
layout (location=2) out mat3 Tangent;
layout (location=5) out float ViewDepth;
layout (location=6) out vec4 Shadow[NUM_CASCADES];

void main()
{
	gl_Position=projection*HMD*modelview*iPosition*vec4(vPosition.xyz, 1.0);

	Position=(iPosition*vPosition).xyz;
	ViewDepth=-(modelview*vec4(Position, 1.0)).z;
	UV=vUV.xy;

	Tangent=mat3(iPosition)*mat3(vTangent.xyz, vBinormal.xyz, vNormal.xyz);

	const mat4 biasMat = mat4( 
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );

	for(int i=0;i<NUM_CASCADES;i++)
		Shadow[i]=biasMat*lightMVP[i]*vec4(Position, 1.0);
}
