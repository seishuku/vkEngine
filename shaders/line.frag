#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
};

layout (location=0) out vec4 Output;

void main()
{
    Output=vec4(1.0);
}
