#version 450

#define NUM_CASCADES 4

layout (binding=3) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

layout(location=0) out vec3 Position;
layout(location=1) flat out float Scale;

vec3 Cube[]=
{
	{ +1.0, -1.0, -1.0 }, { +1.0, -1.0, +1.0 }, { +1.0, +1.0, +1.0 }, { +1.0, +1.0, -1.0 }, { +1.0, -1.0, -1.0 }, { +1.0, +1.0, +1.0 },
	{ -1.0, -1.0, +1.0 }, { -1.0, -1.0, -1.0 }, { -1.0, +1.0, -1.0 }, { -1.0, +1.0, +1.0 }, { -1.0, -1.0, +1.0 }, { -1.0, +1.0, -1.0 },
	{ -1.0, +1.0, -1.0 }, { +1.0, +1.0, -1.0 }, { +1.0, +1.0, +1.0 }, { -1.0, +1.0, +1.0 }, { -1.0, +1.0, -1.0 }, { +1.0, +1.0, +1.0 },
	{ -1.0, -1.0, +1.0 }, { +1.0, -1.0, +1.0 }, { +1.0, -1.0, -1.0 }, { -1.0, -1.0, -1.0 }, { -1.0, -1.0, +1.0 }, { +1.0, -1.0, -1.0 },
	{ +1.0, -1.0, +1.0 }, { -1.0, -1.0, +1.0 }, { -1.0, +1.0, +1.0 }, { +1.0, +1.0, +1.0 }, { +1.0, -1.0, +1.0 }, { -1.0, +1.0, +1.0 },
	{ -1.0, -1.0, -1.0 }, { +1.0, -1.0, -1.0 }, { +1.0, +1.0, -1.0 }, { -1.0, +1.0, -1.0 }, { -1.0, -1.0, -1.0 }, { +1.0, +1.0, -1.0 },
};

void main()
{	float cascadeSplits[NUM_CASCADES+1];

	Scale=1500.0;
	vec3 vPosition=Cube[gl_VertexIndex]*Scale.xxx;

	gl_Position=projection*HMD*modelview*vec4(vPosition, 1.0);

	Position=vPosition.xyz;
}
