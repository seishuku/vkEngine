#version 450

layout(location=0) in vec3 Position;
layout(location=1) flat in float Scale;

layout(binding=0) uniform sampler3D volumeTex;
layout(binding=1) uniform sampler2DMS depthTex;
layout(binding=2) uniform sampler2DShadow shadowTex;

layout(binding=3) uniform MainUBO
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

layout(push_constant) uniform PC
{
	uint uFrame;
	uint uWidth, uHeight;
	float fShift;
	uint uSamples;
	uint pad[3];
};

layout(location=0) out vec4 Output;

vec4 depth2Eye()
{
	const float invSamples=1.0/float(uSamples);
	float depth=0.0;

	for(int i=0;i<uSamples;i++)
		depth+=texelFetch(depthTex, ivec2(gl_FragCoord.xy), i).x*invSamples;

	const vec4 clipPosition=inverse(projection)*vec4(vec3((gl_FragCoord.xy/vec2(uWidth, uHeight))*2-1, depth), 1.0);
	return clipPosition/clipPosition.w;
}

vec2 intersectBox(vec3 origin, vec3 direction)
{
    // compute intersection of ray with all six bbox planes
    const vec3 invR=1.0/direction;

    const vec3 tbot=invR*(vec3(-1.0)-origin);
    const vec3 ttop=invR*(vec3(1.0)-origin);

    // re-order intersections to find smallest and largest on each axis
    const vec3 tmin=min(ttop, tbot);
    const vec3 tmax=max(ttop, tbot);

    // find the largest tmin and the smallest tmax
	const float t0=max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
	const float t1=min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    return vec2(t0, t1);
}

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

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
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
	const int range=1;
	
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

float CloudPhase(float cosTheta) {
    float forward=MiePhase(cosTheta, 0.8);
    float backward=MiePhase(cosTheta, -0.8);
    float isotropic=0.1;
    
    return mix(forward, backward, 0.1)+isotropic;
}

void main()
{
	const vec3 eye=inverse(modelview)[3].xyz;
    const vec3 ro=eye/Scale;
    const vec3 rd=normalize(Position-eye);

	vec2 hit=intersectBox(ro, rd);

	if(hit.y<hit.x)
	{
		discard;
		return;
	}

	const vec4 worldPos=depth2Eye()/Scale;
	const float localSceneDepth=length(worldPos);

	seed=uint(gl_FragCoord.x+uWidth*gl_FragCoord.y)*uFrame;

	// make sure near hit doesn't go negative and apply some random jitter to smooth banding
	hit.x=max(hit.x, 0.0)+randomFloat()*0.1;
	// clamp far hit to scene depth for proper mixing with exisiting scene
	hit.y=min(hit.y, localSceneDepth);

	const vec3 dt_vec=1.0/(Scale.xxx*abs(rd));
	const float stepSize=min(dt_vec.x, min(dt_vec.y, dt_vec.z))*8.0;

	// Phase angle between light and view
	const float cosTheta=dot(rd, lightDirection.xyz);
	const float phase=CloudPhase(cosTheta);

	Output=0.0.xxxx;

	for(float dist=hit.x;dist<hit.y;dist+=stepSize)
	{
		const vec3 pos=ro+rd*dist;

		const float d=clamp(1.0-length(pos), 0.0, 1.0);
		const float density=1.0-exp(-(texture(volumeTex, pos*0.5+0.5).r*d)*2.0);

		// colorize the cloud sample
		vec4 val_color=vec4(hsv2rgb(vec3(density+fShift, 1.0, 1.0)), density);

		// Apply scattering term
		val_color.rgb*=lightColor.rgb*phase*mix(1.0, ShadowPCF(pos*Scale), 0.3);

		Output.rgb+=(1.0-Output.a)*val_color.rgb*val_color.a;
		Output.a+=(1.0-Output.a)*val_color.a;

		// Jump distance in low density areas where it doesn't matter
		if(density<0.0001)
			dist+=stepSize;

		if(Output.a>=0.99)
			break;
	}

	// boost final color some for a more dramatic cloud
	Output=clamp(Output*1.3, 0.0, 1.0);
}
