#version 450

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in mat3 Tangent;
layout (location=5) in mat4 iMatrix;
layout (location=9) in vec4 Shadow;

layout (binding=3) uniform ubo
{
	mat4 HMD;
	mat4 projection;
    mat4 modelview;
	mat4 light_mvp;
	vec4 light_color;
	vec4 light_direction;
};

layout (push_constant) uniform pc
{
	mat4 local;
};

layout (binding=0) uniform sampler2D TexBase;
layout (binding=1) uniform sampler2D TexNormal;
layout (binding=2) uniform sampler2DShadow TexShadow;

layout (location=0) out vec4 Output;

float ShadowPCF(vec4 Coords)
{
	vec2 delta=(light_direction.w*500.0)*(1.0/vec2(textureSize(TexShadow, 0)));

	float shadow=0.0;
	int count=0;
	int range=5;
	
	for(int x=-range;x<range;x++)
	{
		for(int y=-range;y<range;y++)
		{
			shadow+=texture(TexShadow, (Coords.xyz+vec3(delta*vec2(x, y), 0.0))/Coords.w);
			count++;
		}
	}

	return shadow/count;
}

void main()
{
	vec4 Base=texture(TexBase, UV);
	vec3 n=normalize(iMatrix*local*vec4(Tangent*(2*texture(TexNormal, UV)-1).xyz, 0.0)).xyz;

	Output=vec4(sqrt(Base.xyz*light_color.xyz*max(0.045, dot(n, light_direction.xyz)*ShadowPCF(Shadow))), 1.0);
}