#version 450

layout (location=0) in vec4 vPosition;

layout (location=0) out vec4 gPosition;

void main()
{
	gPosition=vPosition;
}
