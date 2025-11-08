#version 450

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in mat3 Tangent;
layout (location=5) in mat4 iMatrix;
layout (location=9) in vec4 Shadow;

layout (binding=3) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP;
	vec4 lightColor;
	vec4 lightDirection;
};

layout (binding=4) uniform SkyboxUBO
{
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

layout (binding=0) uniform sampler2D TexBase;
layout (binding=1) uniform sampler2D TexNormal;
layout (binding=2) uniform sampler2DShadow TexShadow;

layout (location=0) out vec4 Output;

float ShadowPCF(vec4 Coords)
{
	vec2 delta=(lightDirection.w*500.0)*(1.0/vec2(textureSize(TexShadow, 0)));

	float shadow=0.0;
	int count=0;
	int range=5;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(TexShadow, (Coords.xyz+vec3(delta*vec2(x, y), 0.0))/Coords.w);
			count++;
		}
	}

	return shadow/count;
}

void main()
{
	vec4 BaseTex=texture(TexBase, UV);
	vec3 NormalTex=texture(TexNormal, UV).xyz*2.0-1.0;
	vec3 n=normalize(mat3(iMatrix)*Tangent*NormalTex);

	Output=vec4(BaseTex.xyz*lightColor.xyz*max(0.01, dot(n, lightDirection.xyz)*ShadowPCF(Shadow)), 1.0);
}
