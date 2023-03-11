#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D original1;

layout(location=0) out vec4 Output;

void main(void)
{
	float mask=0.0;

	if(UV.x<0.5)
		mask=1.0;

	Output=texture(original, UV*vec2(2.0, 1.0))*mask;
	Output+=texture(original1, UV*vec2(2.0, 1.0))*(1.0-mask);
}
