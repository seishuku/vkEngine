#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D blur;
layout(binding=2) uniform sampler2DMS depthTex;
layout(binding=3) uniform sampler2DArrayShadow shadowTex;

#define NUM_CASCADES 4

layout(binding=4) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 view;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

struct Light
{
	uint ID;
	int shadowSlot;
	uint pad0;
	uint pad1;

	vec3 position;
	float radius;

	vec4 Kd;

	vec4 spotDirection;
	float spotOuterCone;
	float spotInnerCone;
	float spotExponent;
	float spotPad;
};

layout(binding=5) readonly buffer LightBuffer
{
	uint numLights;
	Light lights[];
};

layout(binding=6) readonly buffer LightShadowBuffer
{
	mat4 lightShadowMatrix[];
};

layout(binding=7) uniform sampler2DArrayShadow lightShadowTex;

layout(push_constant) uniform PC
{
	uint uWidth, uHeight;
	uint uSamples, uFrame;
};

layout(location=0) out vec4 Output;

float ShadowPCF(int cascade, vec3 pos)
{
	const mat4 biasMat = mat4( 
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );
	vec4 projCoords=biasMat*lightMVP[cascade]*vec4(pos, 1.0);
	const vec2 delta=(lightDirection.w*300.0)*(1.0/vec2(textureSize(shadowTex, 0)));

	float shadow=0.0;
	const int range=1;

	projCoords.xyz/=projCoords.w;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(shadowTex, vec4(projCoords.xy+delta*vec2(x, y), cascade, projCoords.z));
		}
	}

	return shadow*0.25;
}

float MiePhase(float cosTheta, float g)
{
    float gSq=g*g;
    float denominator=max(1.0-2.0*g*cosTheta+gSq, 0.000001);
    return (1.0-gSq)/(pow(denominator, 1.5)*(4.0*3.1415926));
}

float IGN(vec2 pixel, uint frame)
{
	return fract(52.9829189*fract(dot(pixel+5.588238f*float(frame), vec2(0.06711056, 0.00583715))));
}

float volumetricLightScattering(const vec3 lightPos, const vec3 rayOrigin, const vec3 rayEnd, const float ign)
{
	const float g=0.76, mieCoefficient=0.3;
	const int numSteps=8;
	const float fNumSteps=1.0/float(numSteps);
	const vec3 rayVector=rayEnd-rayOrigin;
	const float rayLength=length(rayVector);
	const vec3 rayDirection=rayVector/rayLength;
    const vec3 rayStep=rayDirection*rayLength*fNumSteps;
	const float decay=0.98;
	const float cosTheta=clamp(dot(rayDirection, normalize(lightPos)), -1.0, 1.0);
	const float scattering=mieCoefficient*MiePhase(cosTheta, g);

	float L=0.0;
	float lDecay=1.0;
	vec3 rayPos=rayOrigin+rayStep*ign;

	for(int i=0;i<numSteps;i++)
	{
		L+=ShadowPCF(0, rayPos)*scattering*lDecay;
		lDecay*=decay;
		rayPos.xyz+=rayStep;
	}

	return L*fNumSteps;
}

float resolveDepth()
{
	ivec2 pixel=ivec2(UV*vec2(textureSize(depthTex)));
	float d=texelFetch(depthTex, pixel, 0).x;

	for(int i=1;i<uSamples;i++)
		d=max(d, texelFetch(depthTex, pixel, i).x);

	return d;
}

vec4 depth2World(const mat4 inverseProjection, const mat4 inverseCamera)
{
	const float depth=resolveDepth();
	// The skybox at infinity means we need to clamp depth to avoid division by zero.
	// (should actually be zNear/"world scale", but whatever)
	const float viewZ=max(depth, 0.00009);

	const vec4 clipPosition=inverseProjection*vec4(vec3(UV*2-1, viewZ), 1.0);
	return inverseCamera*clipPosition/clipPosition.w;
}

int SelectLightShadowFace(const vec3 direction)
{
	vec3 a=abs(direction);

	if(a.x>=a.y&&a.x>=a.z)
		return (direction.x>=0.0)?0:1;

	if(a.y>=a.z)
		return (direction.y>=0.0)?2:3;

	return (direction.z>=0.0)?4:5;
}

float LightShadow(const Light light, const vec3 position)
{
	ivec3 size=textureSize(lightShadowTex, 0);
	vec3 lightToPosition=position-light.position;
	float distanceSq=dot(lightToPosition, lightToPosition);

	if(distanceSq<=0.00000001)
		return 1.0;

	vec3 offsetPosition=position-lightToPosition*(3.0/float(size.x));
	int layer=light.shadowSlot*6+SelectLightShadowFace(offsetPosition-light.position);

	const mat4 biasMat=mat4(
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );

	vec4 coords=biasMat*lightShadowMatrix[layer]*vec4(offsetPosition, 1.0);
	vec3 projCoords=coords.xyz/coords.w;
	vec2 delta=1.0/vec2(size.xy);
	float shadow=0.0;

	for(int x=-1;x<=1;x++)
	{
		for(int y=-1;y<=1;y++)
			shadow+=texture(lightShadowTex, vec4(projCoords.xy+delta*vec2(x, y), float(layer), projCoords.z));
	}

	return shadow/9.0;
}

float LightVolumeShadow(const Light light, const vec3 rayOrigin, const vec3 rayVector, const float ign)
{
	const int numSamples=8;
	float rayLengthSq=dot(rayVector, rayVector);

	if(rayLengthSq<=0.000001)
		return LightShadow(light, rayOrigin);

	float lightRange=1.0/light.radius;
	// The radial falloff drops below roughly 1% beyond this distance.
	float influenceRadius=min(lightRange, sqrt(20.0*lightRange));
	vec3 lightOffset=light.position-rayOrigin;
	float closestT=clamp(dot(lightOffset, rayVector)/rayLengthSq, 0.0, 1.0);
	vec3 closestPosition=rayOrigin+closestT*rayVector;
	vec3 closestToLight=closestPosition-light.position;
	float closestDistanceSq=dot(closestToLight, closestToLight);

	if(closestDistanceSq>=influenceRadius*influenceRadius)
		return 1.0;

	float halfSegment=sqrt((influenceRadius*influenceRadius-closestDistanceSq)/rayLengthSq);
	float startT=max(0.0, closestT-halfSegment);
	float endT=min(1.0, closestT+halfSegment);
	float weightedVisibility=0.0;
	float totalWeight=0.0;
	bool isSpotlight=dot(light.spotDirection.xyz, light.spotDirection.xyz)>0.0;
	vec3 spotDirection=isSpotlight?normalize(light.spotDirection.xyz):vec3(0.0);

	for(int i=0;i<numSamples;i++)
	{
		float t=mix(startT, endT, (float(i)+ign)/float(numSamples));
		vec3 samplePosition=rayOrigin+t*rayVector;
		vec3 toSample=samplePosition-light.position;
		float distanceSq=dot(toSample, toSample);
		float falloff=light.radius*0.5*distanceSq;
		float weight=1.0/(falloff*falloff+1.0);

		if(isSpotlight)
		{
			float cosine=(distanceSq>0.000001)?dot(toSample, spotDirection)*inversesqrt(distanceSq):1.0;
			float spot=smoothstep(light.spotOuterCone, light.spotInnerCone, cosine);

			if(light.spotExponent>0.0)
				spot=pow(spot, light.spotExponent);

			weight*=spot;
		}

		weightedVisibility+=weight*LightShadow(light, samplePosition);
		totalWeight+=weight;
	}

	if(totalWeight<=0.000001)
		return 1.0;

	float visibility=weightedVisibility/totalWeight;
	float shadowFade=1.0-smoothstep(influenceRadius*0.75, influenceRadius, sqrt(closestDistanceSq));

	return mix(1.0, visibility, shadowFade);
}

float SpotRayCosine(const vec3 lightOffset, const vec3 rayVector, const float t, const vec3 spotDirection)
{
	vec3 toPoint=t*rayVector-lightOffset;
	float distanceSq=dot(toPoint, toPoint);

	return (distanceSq>0.000001)?dot(toPoint, spotDirection)*inversesqrt(distanceSq):1.0;
}

float MaxSpotRayCosine(const vec3 lightOffset, const vec3 rayVector, const vec3 spotDirection)
{
	vec3 rayStart=-lightOffset;
	float a=dot(rayStart, spotDirection);
	float b=dot(rayVector, spotDirection);
	float c=dot(rayStart, rayStart);
	float d=dot(rayStart, rayVector);
	float e=dot(rayVector, rayVector);
	float denominator=b*d-a*e;

	float maxCosine=SpotRayCosine(lightOffset, rayVector, 0.0, spotDirection);
	float endCosine=SpotRayCosine(lightOffset, rayVector, 1.0, spotDirection);

	if(endCosine>maxCosine)
	{
		maxCosine=endCosine;
	}

	if(abs(denominator)>0.000001)
	{
		float t=(a*d-b*c)/denominator;

		if(t>0.0&&t<1.0)
		{
			float cosine=SpotRayCosine(lightOffset, rayVector, t, spotDirection);

			if(cosine>maxCosine)
				maxCosine=cosine;
		}
	}

	return maxCosine;
}

vec3 LightVolumes(const vec3 rayOrigin, const vec3 rayEnd, const float ign)
{
	vec3 result=vec3(0.0);
	vec3 uE=rayEnd-rayOrigin;
	float uELengthSq=max(dot(uE, uE), 0.000001);

	for(uint i=0;i<numLights;i++)
	{
		const Light light=lights[i];
		vec3 lPos=light.position-rayOrigin;
		float closestT=clamp(dot(lPos, uE)/uELengthSq, 0.0, 1.0);
		vec3 lVolume=closestT*uE-lPos;
		float volumeDistance=length(lVolume);
		float rangeAttenuation=clamp(1.0-volumeDistance*light.radius, 0.0, 1.0);

		if(rangeAttenuation<=0.0)
			continue;

		float falloff=light.radius*0.5*dot(lVolume, lVolume);
		float volume=1.0/(falloff*falloff+1.0);
		volume*=rangeAttenuation*rangeAttenuation;

		float spot=1.0;
		if(dot(light.spotDirection.xyz, light.spotDirection.xyz)>0.0)
		{
			float volumeCone=1.0;

			if(dot(lVolume, lVolume)>0.000001)
				volumeCone=MaxSpotRayCosine(lPos, uE, normalize(light.spotDirection.xyz));

			spot=smoothstep(light.spotOuterCone, light.spotInnerCone, volumeCone);

			if(light.spotExponent>0.0)
				spot=pow(spot, light.spotExponent);
		}

		float attenuation=volume*spot;

		if(light.shadowSlot>=0&&attenuation>0.0001)
			attenuation*=LightVolumeShadow(light, rayOrigin, uE, ign);

		result+=light.Kd.rgb*attenuation;
	}

	return result;
}

void main(void)
{
	mat4 inverseProjection=inverse(projection);
	mat4 inverseCamera=inverse(HMD*view);
	vec3 ro=inverseCamera[3].xyz;
	float ign=IGN(gl_FragCoord.xy, uFrame);

	vec4 worldPos=depth2World(inverseProjection, inverseCamera);
    vec4 sceneColor=texture(original, UV)+texture(blur, UV);
	sceneColor.rgb+=LightVolumes(ro, worldPos.xyz, ign);

	vec3 lightVolume=volumetricLightScattering(lightDirection.xyz, ro, worldPos.xyz, ign)*lightColor.xyz;
    Output=1.0-exp(-sceneColor)+vec4(lightVolume, 0.0);
}
