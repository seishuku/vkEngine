#version 450

layout (push_constant) uniform ubo
{
	ivec2 Viewport;
	ivec2 pad;
	mat4 mvp;
	vec4 Color;
};

layout (location=0) out vec4 Output;

void main()
{
    Output=Color;
}
