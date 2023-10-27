// SDF
#version 450

layout (location=0) in vec2 UV;
layout (location=1) in flat vec4 Color;
layout (location=2) in flat uint Type;
layout (location=3) in flat vec2 Size;

layout (binding=0) uniform sampler2D Texture;

layout (location=0) out vec4 Output;

layout (push_constant) uniform ubo {
	vec2 Viewport;	// Window width/height
};

const uint UI_CONTROL_BUTTON=0;
const uint UI_CONTROL_CHECKBOX=1;
const uint UI_CONTROL_BARGRAPH=2;
const uint UI_CONTROL_SPRITE=3;
const uint UI_CONTROL_CURSOR=4;

mat2 rotate(float a)
{
	float s=sin(a*0.017453292223), c=cos(a*0.017453292223);
	return mat2(c, s, -s, c);
}

float sdCircle(in vec2 p, in float r) 
{
    return length(p)-r;
}

float sdRoundedRect(vec2 p, vec2 s, float r)
{
    vec2 d=abs(p)-s+vec2(r);
	return length(max(d, 0.0))+min(max(d.x, d.y), 0.0)-r;
}

float sdIsoscelesTriangle(vec2 p, vec2 q)
{
	p.x=abs(p.x);
	vec2 a=p-q*clamp(dot(p,q)/dot(q,q), 0.0, 1.0);
	vec2 b=p-q*vec2(clamp(p.x/q.x, 0.0, 1.0 ), 1.0);
	float s=-sign(q.y);
	vec2 d=min(vec2(dot(a, a), s*(p.x*q.y-p.y*q.x)), vec2(dot(b,b), s*(p.y-q.y)));

	return -sqrt(d.x)*sign(d.y);
}

float sdLine(vec2 p, vec2 a, vec2 b)
{
	vec2 pa=p-a, ba=b-a;
	return length(pa-ba*clamp(dot(pa, ba)/dot(ba, ba), 0.0, 1.0));
}

float sdfDistance(float dist)
{
	float weight=0.01;
	float px=fwidth(dist);
	return 1-smoothstep(weight-px, weight+px, dist);
}

void main()
{
	const float corner_radius=0.2;

	vec2 aspect=vec2(Size.x/Size.y, 1.0);
    vec2 uv=UV*aspect;

	switch(Type)
	{
		case UI_CONTROL_BUTTON:
		{
			float dist_ring=sdRoundedRect(uv-vec2(0.02), aspect-0.04, corner_radius);
			float ring=sdfDistance(dist_ring);

			float dist_shadow=sdRoundedRect(uv+vec2(0.02), aspect-0.04, corner_radius);
			float shadow=sdfDistance(dist_shadow);

			vec3 outer=mix(vec3(0.25)*shadow, vec3(1.0)*ring, 0.5);
			float outer_alpha=sdfDistance(min(dist_ring, dist_shadow));

			Output=vec4(outer, outer_alpha);
			return;
		}

		case UI_CONTROL_CHECKBOX:
		{
			float dist_ring=abs(sdCircle(uv-vec2(0.01), 0.96))-0.02;
			float ring=sdfDistance(dist_ring);

			float dist_shadow=abs(sdCircle(uv+vec2(0.01), 0.96))-0.02;
			float shadow=sdfDistance(dist_shadow);

			vec3 outer=mix(vec3(0.25)*shadow, vec3(1.0)*ring, 0.5);
			float outer_alpha=sdfDistance(min(dist_ring, dist_shadow));

			float center_alpha=0.0;
			
			if(Color.w>0.5)
				center_alpha=sdfDistance(sdCircle(uv, 0.92));

			vec3 center=Color.xyz*center_alpha;

			Output=vec4(outer+center, outer_alpha+center_alpha);
			return;
		}

		case UI_CONTROL_BARGRAPH:
		{
			float dist_ring=abs(sdRoundedRect(uv-vec2(0.02), aspect-0.04, corner_radius))-0.04;
			float ring=sdfDistance(dist_ring);

			float dist_shadow=abs(sdRoundedRect(uv+vec2(0.02), aspect-0.04, corner_radius))-0.04;
			float shadow=sdfDistance(dist_shadow);

			vec3 outer=mix(vec3(0.25)*shadow, vec3(1.0)*ring, 0.5);
			float outer_alpha=sdfDistance(min(dist_ring, dist_shadow));

			float center_alpha=0.0;

			if(Color.w>(uv.x/aspect.x)*0.5+0.5)
				center_alpha=sdfDistance(sdRoundedRect(uv, aspect-0.08, corner_radius-0.04));;

			vec3 center=Color.xyz*center_alpha;

			Output=vec4(outer+center, outer_alpha+center_alpha);
			return;
		}

		case UI_CONTROL_SPRITE:
		{
			Output=texture(Texture, vec2(UV.x, -UV.y)*0.5+0.5);
			return;
		}

		case UI_CONTROL_CURSOR:
		{
			Output=vec4(Color.xyz, sdfDistance(sdIsoscelesTriangle(rotate(-16.0)*(uv+vec2(0.99, -0.99)), vec2(0.6, -1.9))));
			return;
		}

		default:
			return;
	}
}
