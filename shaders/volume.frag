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

float randomNormalDistribution(inout uint seed)
{
	float theta=2.0*3.1415926*randomFloat(seed);
	float rho=sqrt(-2.0*log(randomFloat(seed)));

	return rho*cos(theta);
}

vec3 randomDirection(inout uint seed)
{
	return normalize(vec3(randomNormalDistribution(seed), randomNormalDistribution(seed), randomNormalDistribution(seed)));
}

vec3 randomHemiDirection(vec3 normal, inout uint seed)
{
	vec3 dir=randomDirection(seed);

	return dir*sign(dot(normal, dir));
}

struct Sphere_t
{
	vec3 center;
	vec3 color;
	vec3 emissive;
	float roughness;
	float radius;
};

struct rayhit_t
{
	bool hit;
	float dist;
	vec3 point;
	vec3 normal;

	vec3 color;
	vec3 emissive;
	float roughness;
};

Sphere_t Spheres[]=
{
	{ vec3(-0.5, -0.5, 0.0), vec3(1.0, 1.0, 1.0), vec3(10.0, 10.0, 10.0), 0.0, 0.0625 },
	{ vec3(0.25, 0.25, 0.0), vec3(1.0, 0.0, 0.0), vec3(0.0, 0.0, 0.0), 0.0, 0.75 },
	{ vec3(-0.34, -0.26, 0.0), vec3(0.0, 1.0, 1.0), vec3(0.0, 0.0, 0.0), 0.75, 0.03125 },
	{ vec3(-0.31, -0.31, 0.0), vec3(1.0, 0.0, 1.0), vec3(0.0, 0.0, 0.0), 0.75, 0.03125 },
};

rayhit_t intersectSphere(vec3 origin, vec3 direction, vec3 center, float radius)
{
	rayhit_t hit={ false, 0, vec3(0), vec3(0), vec3(0), vec3(0), 0 };
	vec3 offset=origin-center;

	float a=dot(direction, direction);
	float b=2.0*dot(offset, direction);
	float c=dot(offset, offset)-radius*radius;
	float discriminant=b*b-4.0*a*c;

	if(discriminant>=0.0)
	{
		float dist=(-b-sqrt(discriminant))/(2.0*a);

		if(dist>=0.0)
		{
			hit.hit=true;
			hit.dist=dist;
			hit.point=origin+direction*dist;
			hit.normal=normalize(hit.point-center);
		}
	}

	return hit;
}

rayhit_t RayCollision(vec3 origin, vec3 direction)
{
	const int NumSpheres=Spheres.length();
	rayhit_t hit={ false, 1.0/0.0, vec3(0), vec3(0), vec3(0), vec3(0), 0 };

	for(int i=0;i<NumSpheres;i++)
	{
		rayhit_t sphereHit=intersectSphere(origin, direction, Spheres[i].center, Spheres[i].radius);

		if(sphereHit.hit&&sphereHit.dist<hit.dist)
		{
			hit=sphereHit;

			hit.color=Spheres[i].color;
			hit.emissive=Spheres[i].emissive;
			hit.roughness=Spheres[i].roughness;
		}
	}

	return hit;
}

vec4 Trace(vec3 origin, vec3 direction, inout uint seed)
{
	const int MaxBounce=3;
	vec3 rayColor=vec3(1.0);
	vec3 incomingLight=vec3(0.0);
	float alpha=0.0;

	for(int i=0;i<=MaxBounce;i++)
	{
		rayhit_t hit=RayCollision(origin, direction);

		if(hit.hit)
		{
			origin=hit.point;
			vec3 diffuseDir=normalize(hit.normal+randomDirection(seed));
			vec3 specularDir=reflect(direction, hit.normal);
			bool specularBounce=hit.roughness>=randomFloat(seed);
			direction=mix(diffuseDir, specularDir, hit.roughness*float(specularBounce));

			rayColor*=mix(hit.color, vec3(1.0), float(specularBounce));
			incomingLight+=rayColor*hit.emissive;
			alpha=1.0;
		}
		else
			break;
	}

	return vec4(incomingLight, alpha);
}

void main()
{
    vec3 ray_origin=-inverse(modelview)[3].xyz/Scale;
    vec3 ray_direction=normalize(inverse(modelview)[3].xyz-Position);

/*    vec2 hit=intersectBox(ray_origin, ray_direction);

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

		float val=texture(Volume, pos*0.5+0.5).r;
		vec4 val_color=vec4(TurboColormap(val*5.0), val);

		Output.rgb+=(1.0-Output.a)*val_color.rgb*val_color.a;
		Output.a+=(1.0-Output.a)*val_color.a;

		if(Output.a>=0.95)
			break;
	}

	vec4 depth=projection*modelview*vec4(ray_origin+ray_direction*hit.x, 1.0);
	gl_FragDepth=depth.z/depth.w;*/

	uint seed=uint(gl_FragCoord.x*gl_FragCoord.y+gl_FragCoord.x+uFrame*9525.0);
	float random=randomFloat(seed);

	vec4 accum=vec4(vec3(0.0), 1.0);

	const int numSamples=500;

	for(int i=0;i<numSamples;i++)
		accum+=Trace(ray_origin, ray_direction, seed);
	accum/=numSamples;

	Output=accum;
}
