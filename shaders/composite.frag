#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D blur;
layout(binding=2) uniform sampler2D depthTex;
layout(binding=3) uniform sampler2DShadow shadowDepth;

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
	uint pad;
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

float ShadowPCF(vec4 Coords)
{
	vec2 delta=(lightDirection.w*300.0)*(1.0/vec2(textureSize(shadowDepth, 0)));

	float shadow=0.0;
	int count=0;
	int range=2;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(shadowDepth, (Coords.xyz+vec3(delta*vec2(x, y), 0.0))/Coords.w);
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
	const float g=0.01, mieCoefficient=0.09, decay=0.97;
	const int numSteps=8;
	const float fNumSteps=1.0/float(numSteps);
	const vec3 rayVector=rayEnd-rayOrigin;
	const float rayLength=length(rayVector);
	const vec3 rayDirection=rayVector/rayLength;
    const vec3 rayStep=rayDirection*rayLength*fNumSteps;
	const float cosTheta=dot(rayDirection, normalize(lightPos));

	const float miePhase=MiePhase(cosTheta, g);
	const float scattering=mieCoefficient/(1.0-g*g);

	float L=0.0;
	vec4 rayPos=vec4(rayOrigin+rayStep*randomFloat(), 1.0);

	for(int i=0;i<numSteps;i++)
	{
		L+=ShadowPCF(biasMat*lightMVP*rayPos)*scattering;
		rayPos.xyz+=rayStep;
	}

	return L*fNumSteps;
}

vec4 depth2World(float viewZ)
{
	vec4 clipPosition=inverse(projection)*vec4(vec3(UV*2-1, viewZ), 1.0);
	return inverse(HMD*modelview)*clipPosition/clipPosition.w;
}

void main(void)
{
	seed=uint(gl_FragCoord.x+uWidth*gl_FragCoord.y)+uWidth*uHeight*(uFrame%32);

    vec3 ro=inverse(HMD*modelview)[3].xyz;
	vec4 worldPos=depth2World(max(texture(depthTex, UV).x, 0.00009));

	vec3 lightVolume=volumetricLightScattering(lightDirection.xyz, ro, worldPos.xyz)*lightColor.xyz;
	Output=1.0-exp(-(texture(original, UV)+texture(blur, UV))*1.0)+vec4(lightVolume, 0.0);
//	Output=texture(original, UV);
}
