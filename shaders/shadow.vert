#version 450

layout (location=0) in vec4 vPosition;
layout (location=1) in mat4 iPosition;

layout (push_constant) uniform ubo
{
    mat4 mvp;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position=mvp*iPosition*vec4(vPosition.xyz, 1.0);
}
