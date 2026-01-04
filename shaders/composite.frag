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
    mat4 modelview;
	mat4 lightMVP[NUM_CASCADES];
	vec4 lightColor;
	vec4 lightDirection;
	float cascadeSplits[NUM_CASCADES+1];
};

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
	int count=0;
	const int range=1;

	projCoords.xyz/=projCoords.w;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(shadowTex, vec4(projCoords.xy+delta*vec2(x, y), cascade, projCoords.z));
			count++;
		}
	}

	return shadow/count;
}

float MiePhase(float cosTheta, float g)
{
    float gSq=g*g;
    return (1.0+gSq)/pow(1.0-2.0*g*cosTheta+gSq, 1.5)/(4.0*3.1415926);
}

float IGN(vec2 pixel, uint frame)
{
	return fract(52.9829189*fract(dot(pixel+5.588238f*float(frame), vec2(0.06711056, 0.00583715))));
}

float volumetricLightScattering(const float viewDepth, const vec3 lightPos, const vec3 rayOrigin, const vec3 rayEnd)
{
	const float g=0.01, mieCoefficient=0.09;
	const int numSteps=8;
	const float fNumSteps=1.0/float(numSteps);
	const vec3 rayVector=rayEnd-rayOrigin;
	const float rayLength=length(rayVector);
	const vec3 rayDirection=rayVector/rayLength;
    const vec3 rayStep=rayDirection*rayLength*fNumSteps;
	const float decay=0.98;
	const float cosTheta=dot(rayDirection, normalize(lightPos))*0.5+0.5;

	const float miePhase=MiePhase(cosTheta, g);
	const float scattering=mieCoefficient/(1.0-g*g);

	// interleaved gradient noise
	const float ign=IGN(gl_FragCoord.xy, uFrame);

	float L=0.0;
	float lDecay=1.0;
	vec3 rayPos=rayOrigin+rayStep*ign;

	for(int i=0;i<numSteps;i++)
	{
		// TODO: sign/biasing cosTheta and raising it's power seems to be better, but are there better ways of handling this?
		L+=(ShadowPCF(0, rayPos)*pow(cosTheta, 4.0))*scattering*lDecay;
		lDecay*=decay;
		rayPos.xyz+=rayStep;
	}

	return L*fNumSteps;
}

vec4 depth2World()
{
	const float invSamples=1.0f/float(uSamples);
	float depth=0.0;

	for(int i=0;i<uSamples;i++)
		depth+=texelFetch(depthTex, ivec2(UV*textureSize(depthTex)), i).x*invSamples;

	const float viewZ=max(depth, 0.00009);

	const vec4 clipPosition=inverse(projection)*vec4(vec3(UV*2-1, viewZ), 1.0);
	return inverse(HMD*modelview)*clipPosition/clipPosition.w;
}

void main(void)
{
    vec3 ro=inverse(HMD*modelview)[3].xyz;

	vec4 worldPos=depth2World();

	vec3 lightVolume=volumetricLightScattering(worldPos.z, lightDirection.xyz, ro, worldPos.xyz)*lightColor.xyz;
	Output=1.0-exp(-(texture(original, UV)+texture(blur, UV))*1.0)+vec4(lightVolume, 0.0);
}
