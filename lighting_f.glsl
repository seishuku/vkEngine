#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in vec3 TangentX;
layout (location=3) in vec3 TangentY;
layout (location=4) in vec3 TangentZ;
layout (location=5) in vec3 Eye;

layout (binding=0) uniform ubo {
    mat4 mvp;
    vec4 eye;

    vec4 Light0_Pos;
    vec4 Light0_Kd;
    vec4 Light1_Pos;
    vec4 Light1_Kd;
    vec4 Light2_Pos;
    vec4 Light2_Kd;
};

layout (binding=1) uniform sampler2D TexBase;
layout (binding=2) uniform sampler2D TexNormal;

layout (location=0) out vec4 Output;

void main()
{
	vec3 temp=2.0*texture(TexNormal, UV).xyz-1.0;
	vec4 Base=texture(TexBase, UV);
	vec3 n=normalize(vec3(dot(TangentX, temp), dot(TangentY, temp), dot(TangentZ, temp)));
	vec3 e=eye.xyz-Position, r;

	vec3 l0=Light0_Pos.xyz-Position;
	vec3 l1=Light1_Pos.xyz-Position;
	vec3 l2=Light2_Pos.xyz-Position;

	// Light volume, distance attenuation, and shadows need to be done before light and eye vector normalization

	// Volume
	vec4 l0_volume=vec4(clamp(dot(l0, e)/dot(e, e), 0.0, 1.0)*e-l0, 0.0 );
	l0_volume.w=1.0/(pow(Light0_Pos.w*0.125*dot(l0_volume.xyz, l0_volume.xyz), 2.0)+1.0);

	vec4 l1_volume=vec4(clamp(dot(l1, e)/dot(e, e), 0.0, 1.0)*e-l1, 0.0);
	l1_volume.w=1.0/(pow(Light1_Pos.w*0.125*dot(l1_volume.xyz, l1_volume.xyz), 2.0)+1.0);

	vec4 l2_volume=vec4(clamp(dot(l2, e)/dot(e, e), 0.0, 1.0)*e-l2, 0.0);
	l2_volume.w=1.0/(pow(Light2_Pos.w*0.125*dot(l2_volume.xyz, l2_volume.xyz), 2.0)+1.0);

	// Attenuation = 1.0-(Light_Position*(1/Light_Radius))^2
	float l0_atten=max(0.0, 1.0-length(l0*Light0_Pos.w));
	float l1_atten=max(0.0, 1.0-length(l1*Light1_Pos.w));
	float l2_atten=max(0.0, 1.0-length(l2*Light2_Pos.w));

	e=normalize(e);
	l0=normalize(l0);
	l1=normalize(l1);
	l2=normalize(l2);

	r=normalize((2.0*dot(e, n))*n-e);

	// Diffuse = Kd*(N.L)
	vec3 l0_diffuse=Light0_Kd.rgb*max(0.0, dot(l0, n));

	// Specular = Ks*((R.L)^n)*(N.L)*Gloss
	vec3 l0_specular=Light0_Kd.rgb*max(0.0, pow(dot(l0, r), 16.0)*dot(l0, n)*Base.a)*l0_diffuse;

	vec3 l1_diffuse=Light1_Kd.rgb*max(0.0, dot(l1, n));
	vec3 l1_specular=Light1_Kd.rgb*max(0.0, pow(dot(l1, r), 16.0)*dot(l1, n)*Base.a)*l1_diffuse;

	vec3 l2_diffuse=Light2_Kd.rgb*max(0.0, dot(l2, n));
	vec3 l2_specular=Light2_Kd.rgb*max(0.0, pow(dot(l2, r), 16.0)*dot(l2, n)*Base.a)*l2_diffuse;

	// I=(base*diffuse+specular)*shadow*attenuation+volumelight
	temp =(Base.rgb*l0_diffuse+l0_specular)*l0_atten*(1.0-l0_volume.w)+(l0_volume.w*Light0_Kd.rgb);
	temp+=(Base.rgb*l1_diffuse+l1_specular)*l1_atten*(1.0-l1_volume.w)+(l1_volume.w*Light1_Kd.rgb);
	temp+=(Base.rgb*l2_diffuse+l2_specular)*l2_atten*(1.0-l2_volume.w)+(l2_volume.w*Light2_Kd.rgb);

	Output=vec4(temp, 1.0);
}