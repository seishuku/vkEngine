// SDF
#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location=0) in vec3 UV;
layout (location=1) in vec4 Color;

layout (binding=1) uniform sampler3D Texture;

layout (location=0) out vec4 Output;

void main()
{
	const float edgeDistance=0.4;

	float dist=texture(Texture, UV).r;

	float edgeWidth=0.5*fwidth(dist);
	float alpha=smoothstep(edgeDistance-edgeWidth, edgeDistance+edgeWidth, dist);

	Output=vec4(Color.rgb, alpha);
}
