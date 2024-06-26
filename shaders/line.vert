#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 Color;
	vec4 Verts[2];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position=mvp*vec4(Verts[gl_VertexIndex].xyz, 1.0f);
}
