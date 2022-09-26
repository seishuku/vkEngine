#version 450

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in mat3 Tangent;
layout (location=5) in mat4 iMatrix;

layout (push_constant) uniform ubo
{
    mat4 mvp;
	mat4 local;
    vec4 eye;
	vec4 light_color;
	vec4 light_direction;
};

layout (binding=0) uniform sampler2D TexBase;
layout (binding=1) uniform sampler2D TexNormal;

layout (location=0) out vec4 Output;

void main()
{
	vec4 Base=texture(TexBase, UV);
	vec3 n=normalize(iMatrix*local*vec4(Tangent*(2*texture(TexNormal, UV)-1).xyz, 0.0)).xyz;
	vec3 uE=eye.xyz-Position;
	vec3 e=normalize(uE);
	vec3 r=reflect(-e, n);

	Output=vec4(Base.xyz*light_color.xyz*dot(light_direction.xyz, n), 1.0);
}