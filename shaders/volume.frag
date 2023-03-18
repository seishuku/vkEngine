#version 450

layout(location=0) in vec3 Position;
layout(location=1) flat in float Scale;

layout(push_constant) uniform pc
{
    mat4 mv;
    mat4 proj;
	uint uFrame;
};

layout(location=0) out vec4 Output;

vec3 mod289(vec3 x)
{
	return x-floor(x*(1.0/289.0))*289.0;
}

vec4 permute(vec4 x)
{
	vec4 r=((x*34.0)+10.0)*x;
	return r-floor(r*(1.0/289.0))*289.0;
}

vec4 taylorInvSqrt(vec4 r)
{
	return 1.79284291400159-0.85373472095314*r;
}

// Classic Perlin noise
float noise(vec3 P)
{
	vec3 Pi0=mod289(floor(P));
	vec3 Pi1=mod289(Pi0+1.0);

	vec3 Pf0=fract(P);
	vec3 Pf1=Pf0-1.0;

	vec4 ix=vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
	vec4 iy=vec4(Pi0.yy, Pi1.yy);
	vec4 iz0=Pi0.zzzz;
	vec4 iz1=Pi1.zzzz;

	vec4 ixy=permute(permute(ix)+iy);
	vec4 ixy0=permute(ixy+iz0);
	vec4 ixy1=permute(ixy+iz1);

	vec4 gx0=ixy0*(1.0/7.0);
	vec4 gy0=fract(floor(gx0)*(1.0/7.0))-0.5;
	gx0=fract(gx0);
	vec4 gz0=0.5-abs(gx0)-abs(gy0);
	vec4 sz0=step(gz0, vec4(0.0));
	gx0-=sz0*(step(0.0, gx0)-0.5);
	gy0-=sz0*(step(0.0, gy0)-0.5);

	vec4 gx1=ixy1*(1.0/7.0);
	vec4 gy1=fract(floor(gx1)*(1.0/7.0))-0.5;
	gx1=fract(gx1);
	vec4 gz1=0.5-abs(gx1)-abs(gy1);
	vec4 sz1=step(gz1, vec4(0.0));
	gx1-=sz1*(step(0.0, gx1)-0.5);
	gy1-=sz1*(step(0.0, gy1)-0.5);

	vec3 g000=vec3(gx0.x,gy0.x,gz0.x);
	vec3 g100=vec3(gx0.y,gy0.y,gz0.y);
	vec3 g010=vec3(gx0.z,gy0.z,gz0.z);
	vec3 g110=vec3(gx0.w,gy0.w,gz0.w);
	vec3 g001=vec3(gx1.x,gy1.x,gz1.x);
	vec3 g101=vec3(gx1.y,gy1.y,gz1.y);
	vec3 g011=vec3(gx1.z,gy1.z,gz1.z);
	vec3 g111=vec3(gx1.w,gy1.w,gz1.w);

	vec4 norm0=taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
	g000*=norm0.x;
	g010*=norm0.y;
	g100*=norm0.z;
	g110*=norm0.w;

	vec4 norm1=taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
	g001*=norm1.x;
	g011*=norm1.y;
	g101*=norm1.z;
	g111*=norm1.w;

	float n000=dot(g000, Pf0);
	float n100=dot(g100, vec3(Pf1.x, Pf0.yz));
	float n010=dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
	float n110=dot(g110, vec3(Pf1.xy, Pf0.z));
	float n001=dot(g001, vec3(Pf0.xy, Pf1.z));
	float n101=dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
	float n011=dot(g011, vec3(Pf0.x, Pf1.yz));
	float n111=dot(g111, Pf1);

	vec3 fade_xyz=Pf0*Pf0*Pf0*(Pf0*(Pf0*6.0-15.0)+10.0);
	vec4 n_z=mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
	vec2 n_yz=mix(n_z.xy, n_z.zw, fade_xyz.y);

	return 2.2*mix(n_yz.x, n_yz.y, fade_xyz.x);
}

float nebula(vec3 p)
{
    const int iterations=6;
	float turb=0.0f, scale=1.0f;

	for(int i=0;i<iterations;i++)
	{
		scale*=0.5f;
		turb+=scale*noise(p/scale);
	}

    return clamp(turb, 0.0, 1.0);
}

float getSample(vec3 position)
{
	return nebula(position);
}

vec3 computeGradient(vec3 position, float step)
{
	return normalize(vec3(
		getSample(vec3(position.x+step, position.y, position.z))-getSample(vec3(position.x-step, position.y, position.z)),
		getSample(vec3(position.x, position.y+step, position.z))-getSample(vec3(position.x, position.y-step, position.z)),
		getSample(vec3(position.x, position.y, position.z+step))-getSample(vec3(position.x, position.y, position.z-step))
	));
}

vec2 intersectBox(vec3 r_o, vec3 r_d)
{
    const vec3 boxmin=vec3(-1.0, -1.0, -1.0);
    const vec3 boxmax=vec3(1.0, 1.0, 1.0);

    // compute intersection of ray with all six bbox planes
    vec3 invR = 1.0/r_d;

    vec3 tbot=invR*(boxmin-r_o);
    vec3 ttop=invR*(boxmax-r_o);

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

vec3 transformDir(mat4 transform, vec3 dir)
{
	return normalize(transform * vec4(dir, 0.0)).xyz;
}

void main()
{
    vec3 ray_origin=-inverse(mv)[3].xyz/Scale;
    vec3 ray_direction=normalize(inverse(mv)[3].xyz-Position);

    vec2 hit=intersectBox(ray_origin, ray_direction);

	if(hit.y<hit.x)
	{
		discard;
		return;
	}

	hit.x=max(hit.x, 0.0);

	vec3 dt_vec=1.0/(vec3(Scale)*abs(ray_direction));
	float stepSize=min(dt_vec.x, min(dt_vec.y, dt_vec.z));

	uint seed=uint(gl_FragCoord.x*1452.0+gl_FragCoord.y*734.0+uFrame*9525.0);
	float random=randomFloat(seed);

	Output=vec4(0.0);
	for(float dist=hit.x;dist<hit.y;dist+=stepSize)
	{
		vec3 pos=ray_origin+ray_direction*(dist+stepSize*random);

		vec3 gradient=computeGradient(pos, stepSize);
		vec3 normal=transformDir(mv, -gradient);

		float d=clamp(1.0-length(pos), 0.0, 1.0);

		float val=pow(nebula(pos), 1.0)*d*d;
//		val=smoothstep(0.13, 0.26, val);
		vec4 val_color=vec4(TurboColormap(val), val)*max(0.0, dot(normal, normalize(-vec3(1.0, 1.0, 1.0))));

		val_color.a=1.0-pow(1.0-val_color.a, Scale);

		Output.rgb+=(1.0-Output.a)*val_color.a*val_color.rgb;
		Output.a+=(1.0-Output.a)*val_color.a;

		if(Output.a>=0.95)
			break;
	}
}
