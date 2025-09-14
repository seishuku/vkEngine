#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 Color;
	vec4 Verts[3];
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec4 vColor;


void main()
{
	vec3 tVerts[3]={ Verts[0].xyz, Verts[1].xyz, Verts[2].xyz }; // Apparently some don't like indexing uniforms?
	gl_Position=mvp*vec4(tVerts[gl_VertexIndex].xyz, 1.0f);

	const vec3 p=vec3(Verts[0].w, Verts[1].w, Verts[2].w);
	const vec3 ab=tVerts[1]-tVerts[0], ac=tVerts[2]-tVerts[0], ap=p-tVerts[0];

	const float d00=dot(ab, ab);
	const float d01=dot(ab, ac);
	const float d11=dot(ac, ac);
	const float d20=dot(ap, ab);
	const float d21=dot(ap, ac);
	const float invDenom=1.0f/(d00*d11-d01*d01);

	vColor.x=(d11*d20-d01*d21)*invDenom;
	vColor.y=(d00*d21-d01*d20)*invDenom;
	vColor.z=1.0-vColor.x-vColor.y;
	vColor.w=1.0f;
}
