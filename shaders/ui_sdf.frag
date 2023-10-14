// SDF
#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location=0) in vec2 UV;
layout (location=1) in flat vec4 Color;
layout (location=2) in flat uint Type;
layout (location=3) in flat uint DescriptorIndex;
layout (location=4) in flat vec2 Size;

layout (set=0, binding=0) uniform sampler2D Texture[];

layout (location=0) out vec4 Output;

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
};

const uint UI_CONTROL_BUTTON=0;
const uint UI_CONTROL_CHECKBOX=1;
const uint UI_CONTROL_BARGRAPH=2;
const uint UI_CONTROL_SPRITE=3;

float sdCircle(in vec2 p, in float r) 
{
    return length(p)-r;
}

float sdRoundedRect(vec2 p, vec2 s, float r)
{
    vec2 d=abs(p)-s+vec2(r);
	return length(max(d, 0.0))+min(max(d.x, d.y), 0.0)-r;
}

float sdfDistance(float dist)
{
	float edgeWidth=0.5*fwidth(dist);
	return 1.0-smoothstep(-edgeWidth, edgeWidth, dist);
}

void main()
{
	const float corner_radius=0.2;

	vec2 aspect=vec2(Size.x/Size.y, 1.0);
    vec2 UV=UV*aspect;

	switch(Type)
	{
		case UI_CONTROL_BUTTON:
		{
			float ring=sdfDistance(sdRoundedRect(UV, aspect, corner_radius));
			float shadow=sdfDistance(abs(sdRoundedRect(UV+vec2(-0.02, 0.02), aspect-0.04, corner_radius-0.02))-0.04);

			vec3 outer=(Color.xyz*ring)-(Color.xyz*0.25*shadow);
			float outer_alpha=clamp(shadow+ring, 0.0, 1.0);

			Output=vec4(outer, outer_alpha);
			return;
		}

		case UI_CONTROL_CHECKBOX:
		{
			float dist_ring=abs(sdCircle(UV-vec2(0.01), 0.96))-0.02;
			float ring=sdfDistance(dist_ring);

			float dist_shadow=abs(sdCircle(UV+vec2(0.01), 0.96))-0.02;
			float shadow=sdfDistance(dist_shadow);

			vec3 outer=mix(vec3(0.25)*shadow, vec3(1.0)*ring, 0.5);
			float outer_alpha=sdfDistance(min(dist_ring, dist_shadow));

			float center_alpha=0.0;
			
			if(Color.w>0.5)
				center_alpha=sdfDistance(sdCircle(UV, 0.92));

			vec3 center=Color.xyz*center_alpha;

			Output=vec4(outer+center, outer_alpha+center_alpha);
			return;
		}

		case UI_CONTROL_BARGRAPH:
		{
			float dist_ring=abs(sdRoundedRect(UV-vec2(0.02), aspect-0.04, corner_radius))-0.04;
			float ring=sdfDistance(dist_ring);

			float dist_shadow=abs(sdRoundedRect(UV+vec2(0.02), aspect-0.04, corner_radius))-0.04;
			float shadow=sdfDistance(dist_shadow);

			vec3 outer=mix(vec3(0.25)*shadow, vec3(1.0)*ring, 0.5);
			float outer_alpha=sdfDistance(min(dist_ring, dist_shadow));

			float center_alpha=0.0;

			if(Color.w>(UV.x/aspect.x)*0.5+0.5)
				center_alpha=sdfDistance(sdRoundedRect(UV, aspect-0.08, corner_radius-0.04));;

			vec3 center=Color.xyz*center_alpha;

			Output=vec4(outer+center, outer_alpha+center_alpha);
			return;
		}

		case UI_CONTROL_SPRITE:
		{
			Output=texture(Texture[DescriptorIndex], vec2(UV.x, -UV.y)*0.5+0.5);
			return;
		}

		default:
			return;
	}
}
