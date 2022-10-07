#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(push_constant) uniform ParticlePC
{
	mat4 mvp;
	vec4 Right;
	vec4 Up;
};

layout(location=0) in vec4 gPosition[1];
layout(location=1) in vec4 gColor[1];

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vUV;

void main()
{
	float Scale=gPosition[0].w*min(1.0, gColor[0].w);		// Quad size.

	/* Quad as a single triangle strip:

		0 *----* 2
		  |   /|
		  |  / |
		  | /  |
		  |/   |
		1 *----* 3
	*/

	gl_Position=mvp*vec4(gPosition[0].xyz-Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 1.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=mvp*vec4(gPosition[0].xyz-Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 0.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=mvp*vec4(gPosition[0].xyz+Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 1.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=mvp*vec4(gPosition[0].xyz+Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 0.0);
	vColor=gColor[0];
	EmitVertex();

	EndPrimitive();                                                                 
}
