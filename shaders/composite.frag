#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2DMS original;

layout(location=0) out vec4 Output;

const vec4 gamma=vec4(0.45, 0.45, 0.45, 0.0);

void main(void)
{
	Output=texelFetch(original, ivec2(UV*ivec2(1280, 720)), 0)*0.25;
	Output+=texelFetch(original, ivec2(UV*ivec2(1280, 720)), 1)*0.25;
	Output+=texelFetch(original, ivec2(UV*ivec2(1280, 720)), 2)*0.25;
	Output+=texelFetch(original, ivec2(UV*ivec2(1280, 720)), 3)*0.25;
}
