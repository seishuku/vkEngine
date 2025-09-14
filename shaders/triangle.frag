#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 Color;
	vec4 Verts[3];
};

layout (location=0) out vec4 Output;

void main()
{
	Output=Color;
}
