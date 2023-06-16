#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D blur;

layout(location=0) out vec4 Output;

void main(void)
{
//	Output=texture(original, UV).xxxx;
	Output=1.0-exp(-(texture(original, UV)+texture(blur, UV))*1.0);
}
