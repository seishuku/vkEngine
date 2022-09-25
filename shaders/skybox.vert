#version 450

layout (location=0) in vec4 vPosition;

layout (push_constant) uniform ubo
{
	mat4 mvp;
	vec4 uOffset;

	vec3 uNebulaAColor;
	float uNebulaADensity;
	vec3 uNebulaBColor;
	float uNebulaBDensity;

	float uStarsScale;
	float uStarDensity;

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad1[2];
	vec4 uSunColor;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;

void main()
{
	gl_Position=mvp*vPosition;
	Position=vPosition.xyz;
}
