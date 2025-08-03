#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D blur;
layout(binding=2) uniform sampler2DMS depthTex;
layout(binding=3) uniform sampler2DShadow shadowTex;

layout(binding=4) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 lightMVP;
	vec4 lightColor;
	vec4 lightDirection;
};

layout(push_constant) uniform PC
{
	uint uFrame;
	uint uWidth, uHeight;
	uint uSamples;
};

layout(location=0) out vec4 Output;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

uint seed;

uint wang_hash()
{
	seed=(seed^61u)^(seed>>16u);
	seed*=9u;
	seed=seed^(seed>>4u);
	seed*=0x27d4eb2du;
	seed=seed^(seed>>15u);

	return seed;
}

float randomFloat()
{
	return float(wang_hash())/4294967296.0;
}

float ShadowPCF(vec3 pos)
{
	const mat4 biasMat = mat4( 
		0.5, 0.0, 0.0, 0.0,
		0.0, 0.5, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.5, 0.5, 0.0, 1.0 );
	const vec4 projCoords=biasMat*lightMVP*vec4(pos, 1.0);
	const vec2 delta=(lightDirection.w*300.0)*(1.0/vec2(textureSize(shadowTex, 0)));

	float shadow=0.0;
	int count=0;
	const int range=2;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(shadowTex, (projCoords.xyz+vec3(delta*vec2(x, y), 0.0))/projCoords.w);
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

float volumetricLightScattering(const vec3 lightPos, const vec3 rayOrigin, const vec3 rayEnd)
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

	float L=0.0;
	float lDecay=1.0;
	vec3 rayPos=rayOrigin+rayStep*randomFloat();

	for(int i=0;i<numSteps;i++)
	{
		// TODO: sign/biasing cosTheta and raising it's power seems to be better, but are there better ways of handling this?
		L+=(ShadowPCF(rayPos)*pow(cosTheta, 4.0))*scattering*lDecay;
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
	seed=uint(gl_FragCoord.x+uWidth*gl_FragCoord.y)*uFrame;

    vec3 ro=inverse(HMD*modelview)[3].xyz;

	vec4 worldPos=depth2World();

	vec3 lightVolume=volumetricLightScattering(lightDirection.xyz, ro, worldPos.xyz)*lightColor.xyz;
	Output=1.0-exp(-(texture(original, UV)+texture(blur, UV))*1.0)+vec4(lightVolume, 0.0);
}
