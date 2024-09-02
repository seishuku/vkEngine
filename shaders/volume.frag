#version 450

layout(location=0) in vec3 Position;
layout(location=1) flat in float Scale;

layout(binding=0) uniform sampler3D Volume;
layout(binding=1) uniform sampler2DMS Depth;
layout(binding=2) uniform sampler2DShadow Shadow;

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
};

layout(location=0) out vec4 Output;

vec4 depth2Eye()
{
	const float viewZ=max((
		texelFetch(Depth, ivec2(gl_FragCoord.xy), 0).x+
		texelFetch(Depth, ivec2(gl_FragCoord.xy), 1).x+
		texelFetch(Depth, ivec2(gl_FragCoord.xy), 2).x+
		texelFetch(Depth, ivec2(gl_FragCoord.xy), 3).x
	)*0.25, 0.000009);

	const vec4 clipPosition=inverse(projection)*vec4(vec3((gl_FragCoord.xy/vec2(uWidth, uHeight))*2-1, viewZ), 1.0);
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

vec3 TurboColormap(in float x)
{
	const vec4 kRedVec4=vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
	const vec4 kGreenVec4=vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
	const vec4 kBlueVec4=vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
	const vec2 kRedVec2=vec2(-152.94239396, 59.28637943);
	const vec2 kGreenVec2=vec2(4.27729857, 2.82956604);
	const vec2 kBlueVec2=vec2(-89.90310912, 27.34824973);

	x=clamp(x, 0.0, 1.0);

	vec4 v4=vec4(1.0, x, x*x, x*x*x);
	vec2 v2=v4.zw*v4.z;

	return vec3(
		dot(v4, kRedVec4)+dot(v2, kRedVec2),
		dot(v4, kGreenVec4)+dot(v2, kGreenVec2),
		dot(v4, kBlueVec4)+dot(v2, kBlueVec2)
	);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
	const vec3 eye=inverse(modelview)[3].xyz;
    const vec3 ro=-eye/Scale;
    const vec3 rd=normalize(eye-Position);

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

	Output=0.0.xxxx;

	for(float dist=hit.x;dist<hit.y;dist+=stepSize)
	{
		const vec3 pos=ro+rd*dist;

		const float d=clamp(1.0-length(pos), 0.0, 1.0);
		const float density=1.0-exp(-(texture(Volume, pos*0.5+0.5).r*d)*2.0);

		// colorize the cloud sample
		vec4 val_color=vec4(hsv2rgb(vec3(density+fShift, 1.0, 1.0)), density);//vec4(TurboColormap((density+1.0)*1.2), density);

		// apply some simple lighting
		val_color.xyz*=lightColor.xyz*max(0.1, dot(pos, -lightDirection.xyz));

		Output.rgb+=(1.0-Output.a)*val_color.rgb*val_color.a;
		Output.a+=(1.0-Output.a)*val_color.a;

		if(Output.a>=0.8)
			break;
	}

	// boost final color some for a more dramatic cloud
	Output=clamp(Output*1.3, 0.0, 1.0);
	//Output=normalize(worldPos)*0.5+0.5;
}
