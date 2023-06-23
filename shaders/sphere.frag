#version 450

layout(push_constant) uniform ubo
{
	mat4 mvp;
	vec4 color;
};

layout(location=0) in vec4 Color;

layout(location=0) out vec4 Output;

void main()
{
    Output=Color;
}
