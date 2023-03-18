#version 450

layout(push_constant) uniform pc
{
    mat4 mv;
    mat4 proj;
};

layout(location=0) out vec3 Position;
layout(location=1) flat out float Scale;

vec3 CubeVerts[]=
{
	{ +1.0, -1.0, -1.0 },
	{ +1.0, -1.0, +1.0 },
	{ +1.0, +1.0, +1.0 },
	{ +1.0, +1.0, -1.0 },
	{ -1.0, -1.0, +1.0 },
	{ -1.0, -1.0, -1.0 },
	{ -1.0, +1.0, -1.0 },
	{ -1.0, +1.0, +1.0 },
	{ -1.0, +1.0, -1.0 },
	{ +1.0, +1.0, -1.0 },
	{ +1.0, +1.0, +1.0 },
	{ -1.0, +1.0, +1.0 },
	{ -1.0, -1.0, +1.0 },
	{ +1.0, -1.0, +1.0 },
	{ +1.0, -1.0, -1.0 },
	{ -1.0, -1.0, -1.0 },
	{ +1.0, -1.0, +1.0 },
	{ -1.0, -1.0, +1.0 },
	{ -1.0, +1.0, +1.0 },
	{ +1.0, +1.0, +1.0 },
	{ -1.0, -1.0, -1.0 },
	{ +1.0, -1.0, -1.0 },
	{ +1.0, +1.0, -1.0 },
	{ -1.0, +1.0, -1.0 },
};

uint CubeTris[]=
{
	0,  1,  2,  3,  0,  2,	// Right
	4,  5,  6,  7,  4,  6,	// Left
	8,  9,  10, 11, 8,  10,	// Top
	12, 13, 14, 15, 12, 14,	// Bottom
	16, 17, 18, 19, 16, 18, // Front
	20, 21, 22, 23, 20, 22	// Back
};

void main()
{
	Scale=50.0;
	vec3 vPosition=CubeVerts[CubeTris[gl_VertexIndex]]*Scale.xxx;

	gl_Position=proj*mv*vec4(vPosition, 1.0);

	Position=vPosition.xyz;
}
