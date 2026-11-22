#version 450

#define NUM_CASCADES 4

layout (location=0) in vec3 iPosition;
layout (location=1) in vec2 iUV;
layout (location=2) in mat3 iTangent;
layout (location=5) in float iViewDepth;
layout (location=6) in vec4 iShadow[NUM_CASCADES];

layout (binding=0) uniform sampler2D TexBase;
layout (binding=1) uniform sampler2D TexNormal;
layout (binding=2) uniform sampler2DArrayShadow TexShadow;

layout (binding=3) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 view;
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

struct Light
{
	uint ID;
	int shadowSlot;
	uint pad0;
	uint pad1;

	vec3 position;
	float radius;			// Stored as 1/radius

	vec4 Kd;				// Color, alpha isn't used

	vec4 spotDirection;		// Zero for a point light, otherwise the way the spot points
	float spotOuterCone;	// Cosines of the cone's half angles, outer is the smaller one
	float spotInnerCone;
	float spotExponent;		// 0 for a plain smooth edge, more to tighten it
	float spotPad;
};

layout (binding=6) readonly buffer LightBuffer
{
	uint numLights;
	Light lights[];
};

layout (binding=7) readonly buffer LightShadowBuffer
{
	mat4 lightShadowMatrix[];
};
layout (binding=8) uniform sampler2DArrayShadow TexLightShadow;

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

	vec3 projCoords=iShadow[cascade].xyz/iShadow[cascade].w;
	
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

// Which face of a light's shadow map a direction out from the light lands on, the one on the axis it's longest on: +X, -X, +Y, -Y, +Z, -Z
// This kind of emulates cube maps, but makes PCF filtering easier.
int SelectFace(const vec3 direction)
{
	vec3 a=abs(direction);

	if(a.x>=a.y&&a.x>=a.z)
		return (direction.x>=0.0)?0:1;

	if(a.y>=a.z)
		return (direction.y>=0.0)?2:3;

	return (direction.z>=0.0)?4:5;
}

float LightShadow(const Light light, const vec3 position, const vec3 geometricNormal)
{
	const mat4 biasMat=mat4(
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );

	ivec3 size=textureSize(TexLightShadow, 0);

	// Slide the point out along the surface by a bit more than a texel of the face it's on, or the surface shadows itself
	float texel=2.0*length(position-light.position)/float(size.x);
	vec3 offsetPosition=position+geometricNormal*texel*1.5;

	int layer=light.shadowSlot*6+SelectFace(offsetPosition-light.position);

	vec4 coords=biasMat*lightShadowMatrix[layer]*vec4(offsetPosition, 1.0);
	vec3 projCoords=coords.xyz/coords.w;

	vec2 delta=1.0/vec2(size.xy);
	float shadow=0.0;

	for(int x=-1;x<=1;x++)
	{
		for(int y=-1;y<=1;y++)
			shadow+=texture(TexLightShadow, vec4(projCoords.xy+delta*vec2(x, y), float(layer), projCoords.z));
	}

	return shadow/9.0f;
}

// What the point and spot lights add to a surface. position is in world space, n is the normal map's normal, geometricNormal is the surface's normal.
vec3 Lights(const vec3 position, const vec3 n, const vec3 geometricNormal)
{
	vec3 result=vec3(0.0);

	for(uint i=0;i<numLights;i++)
	{
		const Light light=lights[i];

		vec3 toLight=light.position-position;
		float dist=max(length(toLight), 0.0001);

		float attenuation=clamp(1.0-dist*light.radius, 0.0, 1.0);

		if(attenuation<=0.0)
			continue;

		vec3 l=toLight/dist;
		float lambert=max(dot(n, l), 0.0);

		if(lambert<=0.0)
			continue;

		attenuation*=attenuation;

		// A spot light has a direction, inside the inner cone it's full strength and it fades out to the outer one
		if(dot(light.spotDirection.xyz, light.spotDirection.xyz)>0.0)
		{
			float spot=smoothstep(light.spotOuterCone, light.spotInnerCone, dot(-l, normalize(light.spotDirection.xyz)));

			if(light.spotExponent>0.0)
				spot=pow(spot, light.spotExponent);

			attenuation*=spot;
		}

		if(light.shadowSlot>=0&&attenuation>0.0)
			attenuation*=LightShadow(light, position, geometricNormal);

		result+=light.Kd.rgb*(lambert*attenuation);
	}

	return result;
}

void main()
{
	vec4 BaseTex=texture(TexBase, iUV);
	vec3 NormalTex=texture(TexNormal, iUV).xyz*2.0-1.0;
	vec3 n=normalize(iTangent*NormalTex);

	int cascade;
	float blend;
	SelectCascade(iViewDepth, cascade, blend);

	float shadow0=ShadowPCF(cascade);
	float shadow=shadow0;

	if(blend>0.0&&cascade<NUM_CASCADES-1)
		shadow=mix(shadow0, ShadowPCF(cascade+1), blend);

	vec3 lit=lightColor.xyz*max(0.01, dot(n, lightDirection.xyz)*shadow);

	lit+=Lights(iPosition, n, normalize(iTangent[2]));

	Output=vec4(BaseTex.xyz*lit, 1.0);
}
