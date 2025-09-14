#version 450

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 Color;
	vec4 Verts[3];
};

layout (location=0) in vec4 vColor;

layout (location=0) out vec4 Output;

void main()
{
	float edge=min(min(vColor.x, vColor.y), vColor.z);
	float line=smoothstep(0.0, 0.02, edge);
	float am=fwidth(line);
    Output=smoothstep(0.0, am * 1.5, line).xxxx;
    //Output=normalize(vColor.xyz).xyzz;
}
