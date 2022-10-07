#version 450

layout (location=0) in vec4 vColor;
layout (location=1) in vec2 vUV;

layout (binding=0) uniform sampler2D Particle;

layout (location=0) out vec4 Output;

void main()
{
	float Alpha=texture(Particle, vUV).r;

	Output=vColor*vec4(0.5, 0.5, 0.5, Alpha);

	if(Alpha<0.001)
		discard;
}