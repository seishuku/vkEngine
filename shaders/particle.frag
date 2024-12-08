#version 450

layout (location=0) in vec4 vColor;
layout (location=1) in vec2 vUV;

layout (binding=0) uniform sampler2D Particle;

layout (location=0) out vec4 Output;

void main()
{
	float Alpha=1.0-length(vUV*2.0-1.0);

	if(Alpha<0.001)
		discard;

	Output=vec4(vColor.xyz, Alpha*Alpha);
}