#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D originalLeft;
layout(binding=1) uniform sampler2D blurLeft;
layout(binding=2) uniform sampler2D originalRight;
layout(binding=3) uniform sampler2D blurRight;

layout(location=0) out vec4 Output;

void main(void)
{
	float mask=0.0;

	if(UV.x<0.5)
		mask=1.0;
	
	vec2 halfUV=UV*vec2(2.0, 1.0);

	// Because the framebuffer images are clamp to edge addressing,
	// need to emulate repeating for the other "half"
	halfUV.x=mod(halfUV.x, 1.0);

	vec4 Original=(texture(originalLeft, halfUV)*mask)+texture(originalRight, halfUV)*(1.0-mask);
	vec4 Blur=(texture(blurLeft, halfUV)*mask)+texture(blurRight, halfUV)*(1.0-mask);

	Output=1.0-exp(-(Original+Blur)*1.0);
}
