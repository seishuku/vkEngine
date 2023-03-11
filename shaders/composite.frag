#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;

layout(location=0) out vec4 Output;

void main(void)
{
	Output=texture(original, UV);
}
