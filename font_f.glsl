#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location=0) in vec2 UV;
layout (location=1) in vec4 Color;

layout (binding=1) uniform sampler2D Texture;

layout (location=0) out vec4 Output;

void main()
{
	Output=vec4(Color.rgb, texture(Texture, UV).r);
}
