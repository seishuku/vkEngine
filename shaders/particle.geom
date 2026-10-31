#version 450

layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(push_constant) uniform ParticlePC
{
	mat4 view;
	mat4 projection;
};

layout(location=0) in vec4 gPosition[1];
layout(location=1) in vec4 gVelocity[1];
layout(location=2) in vec4 gColor[1];

layout(location=0) out flat vec4 vColor;
layout(location=1) out vec2 vUV;
layout(location=2) out flat vec3 vVelocity;
layout(location=3) out flat vec3 vPosition;

void main()
{
	const float Scale=gPosition[0].w*min(1.0, gColor[0].w);		// Quad size.
	const vec4 Right=vec4(view[0].x, view[1].x, view[2].x, view[3].x);
	const vec4 Up=vec4(view[0].y, view[1].y, view[2].y, view[3].y);

	/* Quad as a single triangle strip:

		0 *----* 2
		  |   /|
		  |  / |
		  | /  |
		  |/   |
		1 *----* 3
	*/

	vColor=gColor[0];
	vVelocity=(view*vec4(gVelocity[0].xyz, 0.0)).xyz;
	vPosition=(view*vec4(gPosition[0].xyz, 1.0)).xyz;

	gl_Position=projection*view*vec4(gPosition[0].xyz-Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 1.0);
	EmitVertex();

	gl_Position=projection*view*vec4(gPosition[0].xyz-Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(0.0, 0.0);
	EmitVertex();

	gl_Position=projection*view*vec4(gPosition[0].xyz+Right.xyz*Scale+Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 1.0);
	EmitVertex();

	gl_Position=projection*view*vec4(gPosition[0].xyz+Right.xyz*Scale-Up.xyz*Scale, 1.0);
	vUV=vec2(1.0, 0.0);
	EmitVertex();

	EndPrimitive();                                                                 
}
