#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 uOffset;

	vec4 uNebulaAColor;
	vec4 uNebulaBColor;

	float uStarsScale;
	float uStarDensity;
	float pad0[2];

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad1[2];
	vec4 uSunColor;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;
layout (location=1) out vec3 UV;
layout (location=2) out mat3 Tangent;

void main()
{
	gl_Position=mvp*vPosition;

	Position=vPosition.xyz;
	UV=vUV.xyz;

	Tangent=mat3(vTangent.xyz, vBinormal.xyz, vNormal.xyz);
}
