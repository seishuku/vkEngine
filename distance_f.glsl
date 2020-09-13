#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location=0) in vec3 Position;

layout (push_constant) uniform PushConsts 
{
	mat4 mvp;
	vec4 Light_Pos;
};

layout (location=0) out float Output;

void main()
{
	Output=length((Light_Pos.xyz-Position)*Light_Pos.w);
}
