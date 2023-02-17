#version 450

layout(location=0) in vec4 vPosition;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 gColor;

void main()
{
	// Incoming vertices are actually control points
	mat4 scale=mat4(0.005, 0.0, 0.0, 0.0,
					0.0, 0.005, 0.0, 0.0,
					0.0, 0.0, 1.0, 0.0,
					0.0, 0.0, 0.0, 1.0);
	gl_Position=scale*vPosition;
	gColor=vColor;
}
