#version 450

layout (location=0) in vec3 vPosition;
layout (location=1) in vec2 vUV;
layout (location=2) in vec3 vTangent;
layout (location=3) in vec3 vBinormal;
layout (location=4) in vec3 vNormal;

layout (push_constant) uniform ubo
{
    mat4 mvp;
	mat4 local;
    vec4 eye;
	vec4 light_color;
	vec4 light_direction;

	uint NumLights;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;
layout (location=1) out vec2 UV;
layout (location=2) out mat3 Tangent;

void main()
{
	gl_Position=mvp*local*vec4(vPosition, 1.0);

	Position=vPosition;
	UV=vUV;

	Tangent=mat3(vTangent.xyz, vBinormal.xyz, vNormal.xyz);
}
