#version 450

layout (location=0) in vec3 Normal;

#define NUM_CASCADES 4

layout (binding=0) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

layout (location=0) out vec4 Output;

void main()
{
	Output=vec4(max(0.0, dot(Normal, lightDirection.xyz)).xxx, 1.0);
}
