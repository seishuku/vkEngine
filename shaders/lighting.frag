#version 450

#define NUM_CASCADES 4

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in mat3 Tangent;
layout (location=5) in float ViewDepth;
layout (location=6) in vec4 Shadow[NUM_CASCADES];

layout (binding=3) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
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
layout (binding=2) uniform sampler2DArrayShadow TexShadow;

layout (location=0) out vec4 Output;

void SelectCascade(float viewDepth, out int cascadeIndex, out float blend)
{
	cascadeIndex=NUM_CASCADES-1;
	blend=0.0;

	for(int i=0;i<NUM_CASCADES;i++)
	{
		float splitNear=cascadeSplits[i];
		float splitFar=cascadeSplits[i+1];

		if(viewDepth>=splitNear&&viewDepth<splitFar)
		{
			cascadeIndex=i;

			float range=splitFar-splitNear;
			float blendStart=splitFar-range*0.5;

			blend=clamp((viewDepth-blendStart)/(splitFar-blendStart), 0.0, 1.0);

			return;
		}
	}
}

float ShadowPCF(const int cascade)
{
	vec2 delta=(lightDirection.w*500.0)*(1.0/vec2(textureSize(TexShadow, 0)));

	float shadow=0.0;
	int count=0;
	int range=5/(cascade+1); // Reduce samples for higher cascades, not sure if I like this

	vec3 projCoords=Shadow[cascade].xyz/Shadow[cascade].w;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(TexShadow, vec4(projCoords.xy+delta*vec2(x, y), float(cascade), projCoords.z));
			count++;
		}
	}

	return shadow/count;
}

void main()
{
	vec4 BaseTex=texture(TexBase, UV);
	vec3 NormalTex=texture(TexNormal, UV).xyz*2.0-1.0;
	vec3 n=normalize(Tangent*NormalTex);

	int cascade;
	float blend;
	SelectCascade(ViewDepth, cascade, blend);

	float shadow0=ShadowPCF(cascade);
	float shadow=shadow0;

	if(blend>0.0&&cascade<NUM_CASCADES-1)
		shadow=mix(shadow0, ShadowPCF(cascade+1), blend);

	Output=vec4(BaseTex.xyz*lightColor.xyz*max(0.01, dot(n, lightDirection.xyz)*shadow), 1.0);
}
