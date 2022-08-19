#version 450

layout (location=0) in vec3 Position;

layout (push_constant) uniform PushConsts 
{
	mat4 mvp;
	vec4 Light_Pos;
};

layout (location=0) out float Output;

void main()
{
	Output=length(Position-Light_Pos.xyz)*Light_Pos.w;
}
