#version 450

layout (location=0) in vec3 Position;

layout (push_constant) uniform PushConsts 
{
	mat4 mvp;
	vec4 Light_Pos;
};

void main()
{
	gl_FragDepth=length(Position-Light_Pos.xyz)*Light_Pos.w;
}
