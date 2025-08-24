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
	vec3 tVerts[2]={ Verts[0].xyz, Verts[1].xyz }; // Apparently some don't like indexing uniforms?
	gl_Position=mvp*vec4(tVerts[gl_VertexIndex].xyz, 1.0f);
}
