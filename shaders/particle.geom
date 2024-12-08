#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(push_constant) uniform ParticlePC
{
	mat4 modelview;
	mat4 projection;
};

layout(location=0) in vec4 gPosition[1];
layout(location=1) in vec4 gColor[1];

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vUV;

void main()
{
	const float Scale=gPosition[0].w*min(1.0, gColor[0].w);		// Quad size.
	const vec4 Right=vec4(modelview[0].x, modelview[1].x, modelview[2].x, modelview[3].x);
	const vec4 Up=vec4(modelview[0].y, modelview[1].y, modelview[2].y, modelview[3].y);

	/* Quad as a single triangle strip:

		0 *----* 2
		  |   /|
		  |  / |
		  | /  |
		  |/   |
		1 *----* 3
	*/

	gl_Position=projection*modelview*vec4(gPosition[0].xyz-Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 1.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=projection*modelview*vec4(gPosition[0].xyz-Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 0.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=projection*modelview*vec4(gPosition[0].xyz+Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 1.0);
	vColor=gColor[0];
	EmitVertex();

	gl_Position=projection*modelview*vec4(gPosition[0].xyz+Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 0.0);
	vColor=gColor[0];
	EmitVertex();

	EndPrimitive();                                                                 
}
