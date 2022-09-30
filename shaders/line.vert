#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 Verts[2];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position=mvp*Verts[gl_VertexIndex];
}
