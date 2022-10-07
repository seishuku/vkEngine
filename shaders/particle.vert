#version 450

layout(location=0) in vec4 vPosition;
layout(location=1) in vec4 vColor;

layout (location=0) out vec4 gPosition;
layout (location=1) out vec4 gColor;

void main()
{
	gPosition=vPosition;
	gColor=vColor;
}
