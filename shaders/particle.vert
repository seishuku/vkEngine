#version 450

layout(location=0) in vec4 vPosition;
layout(location=1) in vec4 vVelocity;
layout(location=2) in vec4 vColor;

layout (location=0) out vec4 gPosition;
layout (location=1) out vec4 gVelocity;
layout (location=2) out vec4 gColor;

void main()
{
	gPosition=vPosition;
	gVelocity=vVelocity;
	gColor=vColor;
}
