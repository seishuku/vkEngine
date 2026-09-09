#version 450

const vec3 cube[]=
{
	// +X
	vec3( 0.5,-0.5,-0.5 ), vec3( 0.5, 0.5,-0.5 ), vec3( 0.5, 0.5, 0.5 ),
	vec3( 0.5,-0.5,-0.5 ), vec3( 0.5, 0.5, 0.5 ), vec3( 0.5,-0.5, 0.5 ),
	// -X
	vec3(-0.5,-0.5,-0.5 ), vec3(-0.5,-0.5, 0.5 ), vec3(-0.5, 0.5, 0.5 ),
	vec3(-0.5,-0.5,-0.5 ), vec3(-0.5, 0.5, 0.5 ), vec3(-0.5, 0.5,-0.5 ),
	// +Y
	vec3(-0.5, 0.5,-0.5 ), vec3(-0.5, 0.5, 0.5 ), vec3( 0.5, 0.5, 0.5 ),
	vec3(-0.5, 0.5,-0.5 ), vec3( 0.5, 0.5, 0.5 ), vec3( 0.5, 0.5,-0.5 ),
	// -Y
	vec3(-0.5,-0.5,-0.5 ), vec3( 0.5,-0.5,-0.5 ), vec3( 0.5,-0.5, 0.5 ),
	vec3(-0.5,-0.5,-0.5 ), vec3( 0.5,-0.5, 0.5 ), vec3(-0.5,-0.5, 0.5 ),
	// +Z
	vec3(-0.5,-0.5, 0.5 ), vec3( 0.5,-0.5, 0.5 ), vec3( 0.5, 0.5, 0.5 ),
	vec3(-0.5,-0.5, 0.5 ), vec3( 0.5, 0.5, 0.5 ), vec3(-0.5, 0.5, 0.5 ),
	// -Z
	vec3( 0.5,-0.5,-0.5 ), vec3(-0.5,-0.5,-0.5 ), vec3(-0.5, 0.5,-0.5 ),
	vec3( 0.5,-0.5,-0.5 ), vec3(-0.5, 0.5,-0.5 ), vec3( 0.5, 0.5,-0.5 )
};

layout(push_constant) uniform ubo
{
	mat4 mvp;
	vec4 color;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location=0) out vec4 Color;

void main()
{
	vec3 v=cube[gl_VertexIndex];

	Color=vec4(color.xyz*normalize(v*0.5+0.5), 1.0);

	gl_Position=mvp*vec4(v.xyz, 1.0);
}
