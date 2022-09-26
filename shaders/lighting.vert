#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in vec4 vUV;
layout (location=2) in vec4 vTangent;
layout (location=3) in vec4 vBinormal;
layout (location=4) in vec4 vNormal;

layout (location=5) in mat4 iPosition;

layout (push_constant) uniform ubo
{
    mat4 mvp;
	mat4 local;
    vec4 eye;
	vec4 light_color;
	vec4 light_direction;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;
layout (location=1) out vec2 UV;
layout (location=2) out mat3 Tangent;
layout (location=5) out mat4 iMatrix;

void main()
{
	gl_Position=mvp*iPosition*local*vec4(vPosition.xyz, 1.0);

	Position=vPosition.xyz;
	UV=vUV.xy;

	Tangent=mat3(vTangent.xyz, vBinormal.xyz, vNormal.xyz);
	iMatrix=iPosition;
}
