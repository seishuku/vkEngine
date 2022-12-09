#version 450

const vec3 ico[]=
{
	vec3(0.525731f, 0, 0.850651f),
	vec3(-0.525731f, 0, 0.850651f),
	vec3(0, 0.850651f, 0.525731f),
	vec3(0, 0.850651f, 0.525731f),
	vec3(-0.525731f, 0, 0.850651f),
	vec3(-0.850651f, 0.525731f, 0),
	vec3(0, 0.850651f, 0.525731f),
	vec3(-0.850651f, 0.525731f, 0),
	vec3(0, 0.850651f, -0.525731f),
	vec3(0.850651f, 0.525731f, 0),
	vec3(0, 0.850651f, 0.525731f),
	vec3(0, 0.850651f, -0.525731f),
	vec3(0.525731f, 0, 0.850651f),
	vec3(0, 0.850651f, 0.525731f),
	vec3(0.850651f, 0.525731f, 0),
	vec3(0.525731f, 0, 0.850651f),
	vec3(0.850651f, 0.525731f, 0),
	vec3(0.850651f, -0.525731f, 0),
	vec3(0.850651f, -0.525731f, 0),
	vec3(0.850651f, 0.525731f, 0),
	vec3(0.525731f, 0, -0.850651f),
	vec3(0.850651f, 0.525731f, 0),
	vec3(0, 0.850651f, -0.525731f),
	vec3(0.525731f, 0, -0.850651f),
	vec3(0.525731f, 0, -0.850651f),
	vec3(0, 0.850651f, -0.525731f),
	vec3(-0.525731f, 0, -0.850651f),
	vec3(0.525731f, 0, -0.850651f),
	vec3(-0.525731f, 0, -0.850651f),
	vec3(0, -0.850651f, -0.525731f),
	vec3(0.525731f, 0, -0.850651f),
	vec3(0, -0.850651f, -0.525731f),
	vec3(0.850651f, -0.525731f, 0),
	vec3(0.850651f, -0.525731f, 0),
	vec3(0, -0.850651f, -0.525731f),
	vec3(0, -0.850651f, 0.525731f),
	vec3(0, -0.850651f, 0.525731f),
	vec3(0, -0.850651f, -0.525731f),
	vec3(-0.850651f, -0.525731f, 0),
	vec3(0, -0.850651f, 0.525731f),
	vec3(-0.850651f, -0.525731f, 0),
	vec3(-0.525731f, 0, 0.850651f),
	vec3(0, -0.850651f, 0.525731f),
	vec3(-0.525731f, 0, 0.850651f),
	vec3(0.525731f, 0, 0.850651f),
	vec3(0.850651f, -0.525731f, 0),
	vec3(0, -0.850651f, 0.525731f),
	vec3(0.525731f, 0, 0.850651f),
	vec3(-0.850651f, -0.525731f, 0),
	vec3(-0.850651f, 0.525731f, 0),
	vec3(-0.525731f, 0, 0.850651f),
	vec3(-0.525731f, 0, -0.850651f),
	vec3(-0.850651f, 0.525731f, 0),
	vec3(-0.850651f, -0.525731f, 0),
	vec3(0, 0.850651f, -0.525731f),
	vec3(-0.850651f, 0.525731f, 0),
	vec3(-0.525731f, 0, -0.850651f),
	vec3(-0.850651f, -0.525731f, 0),
	vec3(0, -0.850651f, -0.525731f),
	vec3(-0.525731f, 0, -0.850651f)
};

layout (binding=0) uniform ubo
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	vec4 uOffset;

	vec3 uNebulaAColor;
	float uNebulaADensity;
	vec3 uNebulaBColor;
	float uNebulaBDensity;

	float uStarsScale;
	float uStarDensity;
	vec2 pad0;

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	vec2 pad1;
	vec4 uSunColor;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout (location=0) out vec3 Position;

void main()
{
	vec3 vPosition=ico[gl_VertexIndex]*20000.0f;

	gl_Position=projection*modelview*HMD*vec4(vPosition, 1.0);
	Position=normalize(vPosition);
}
