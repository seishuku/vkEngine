#version 450

layout (location=0) in vec3 vPosition;

layout (push_constant) uniform PushConsts 
{
	mat4 mvp;
	vec4 Light_Pos;
};

layout (location=0) out vec3 Position;

void main()
{
	gl_Position=mvp*vec4(vPosition, 1.0);

	Position=vPosition;
}
