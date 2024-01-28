#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;

layout (location=5) in mat4 iPosition;

layout (binding=3) uniform ubo
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP;
	vec4 lightColor;
	vec4 lightDirection;
};

layout (push_constant) uniform ubo_pc
{
	mat4 local;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;
layout (location=1) out vec2 UV;
layout (location=2) out mat3 Tangent;
layout (location=5) out mat4 iMatrix;
layout (location=9) out vec4 Shadow;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main()
{
	gl_Position=projection*HMD*modelview*iPosition*local*vec4(vPosition.xyz, 1.0);

	Position=vPosition.xyz;
	UV=vUV.xy;

	Tangent=mat3(vTangent.xyz, vBinormal.xyz, vNormal.xyz);
	iMatrix=iPosition;

	Shadow=biasMat*lightMVP*iPosition*local*vec4(vPosition.xyz, 1.0);
}
