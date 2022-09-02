#version 450

layout (location=0) in vec4 Position;

layout (binding=0) uniform ubo
{
	mat4 mv[6];
	mat4 proj;
	vec4 Light_Pos;
	int index;
	int pad[11];
};

void main()
{
	gl_FragDepth=length(Position.xyz-Light_Pos.xyz)*Light_Pos.w;
}
