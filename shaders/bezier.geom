#version 450

layout(lines_adjacency) in;
layout(line_strip, max_vertices=64) out;

layout (push_constant) uniform pc
{
	mat4 proj;
};

// This controls detail level
const uint numSegments=10;

layout(location=0) in vec4 gColor[4];
layout(location=0) out vec4 Color;

// Cubic Bezier curve evaluation function
vec4 Bezier(float t, vec4 p0, vec4 p1, vec4 p2, vec4 p3)
{
	vec4 c=3.0*(p1-p0);
	vec4 b=3.0*(p2-p1)-c;
	vec4 a=p3-p0-c-b;

	return (a*(t*t*t))+(b*(t*t))+(c*t)+p0;
}

void main()
{
	for(int i=0;i<=numSegments;i++)
	{
		float t=float(i)/numSegments;

		Color=Bezier(t, gColor[0], gColor[1], gColor[2], gColor[3]);
		gl_Position=proj*Bezier(t, gl_in[0].gl_Position, gl_in[1].gl_Position, gl_in[2].gl_Position, gl_in[3].gl_Position);

		EmitVertex();
	}

	EndPrimitive();                                                                 
}
