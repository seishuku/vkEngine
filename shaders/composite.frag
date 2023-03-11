#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;

layout(location=0) out vec4 Output;

const vec4 gamma=vec4(0.45, 0.45, 0.45, 0.0);

void main(void)
{
	Output=texture(original, UV);
}
