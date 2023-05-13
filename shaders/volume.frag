#version 450

layout(location=0) in vec3 Position;
layout(location=1) flat in float Scale;

layout(binding=0) uniform sampler3D Volume;

layout(binding=1) uniform MainUBO
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 light_mvp;
	vec4 light_color;
	vec4 light_direction;
};

layout(push_constant) uniform PC
{
	uint uFrame;
};

layout(location=0) out vec4 Output;

vec2 intersectBox(vec3 origin, vec3 direction)
{
    // compute intersection of ray with all six bbox planes
    vec3 invR=1.0/direction;

    vec3 tbot=invR*(vec3(-1.0)-origin);
    vec3 ttop=invR*(vec3(1.0)-origin);

    // re-order intersections to find smallest and largest on each axis
    vec3 tmin=min(ttop, tbot);
    vec3 tmax=max(ttop, tbot);

    // find the largest tmin and the smallest tmax
    return vec2(max(max(tmin.x, tmin.y), max(tmin.x, tmin.z)), min(min(tmax.x, tmax.y), min(tmax.x, tmax.z)));
}

uint wang_hash(inout uint seed)
{
	seed=(seed^61u)^(seed>>16u);
	seed*=9u;
	seed=seed^(seed>>4u);
	seed*=0x27d4eb2du;
	seed=seed^(seed>>15u);

	return seed;
}

float randomFloat(inout uint seed)
{
	return float(wang_hash(seed))/4294967296.0;
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

void main()
{
    vec3 ray_origin=-inverse(modelview)[3].xyz/Scale;
    vec3 ray_direction=normalize(inverse(modelview)[3].xyz-Position);

    vec2 hit=intersectBox(ray_origin, ray_direction);

	if(hit.y<hit.x)
	{
		discard;
		return;
	}

	hit.x=max(hit.x, 0.0);

	vec3 dt_vec=1.0/(vec3(Scale)*abs(ray_direction));
	float stepSize=min(dt_vec.x, min(dt_vec.y, dt_vec.z))*4.0;

	uint seed=uint(gl_FragCoord.x*1452.0+gl_FragCoord.y*734.0+uFrame*9525.0);
	float random=randomFloat(seed);

	Output=vec4(0.0);

	for(float dist=hit.x;dist<hit.y;dist+=stepSize)
	{
		vec3 pos=ray_origin+ray_direction*(dist+stepSize*random);

		float d=clamp(1.0-length(pos), 0.0, 1.0);
		float val=texture(Volume, pos*0.5+0.5).r*d;
		vec4 val_color=vec4(TurboColormap(val*5.0), val);

		Output.rgb+=(1.0-Output.a)*val_color.rgb*val_color.a;
		Output.a+=(1.0-Output.a)*val_color.a;

		if(Output.a>=0.8)
			break;
	}
}
