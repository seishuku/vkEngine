#version 450

layout (location=0) in vec3 Position;
layout (location=1) in vec2 UV;
layout (location=2) in mat3 Tangent;

layout (push_constant) uniform ubo
{
    mat4 mvp;
    vec4 eye;

	uint NumLights;
};

struct Light_t
{
	int ID;

	uint Pad[3];

	vec4 Position;
	vec4 Kd;

	vec4 SpotDirection;
	vec4 SpotParams;
};

layout(std430, binding=0) readonly buffer layoutLights
{
	Light_t Lights[];
};

layout (binding=1) uniform sampler2D TexBase;
layout (binding=2) uniform sampler2D TexNormal;
layout (binding=3) uniform samplerCubeArray TexDistance;

layout (location=0) out vec4 Output;

float SpotLight(vec3 pos, vec3 dir, float innerCutOff, float outerCutOff, float exponent)
{
	if(outerCutOff>0.0)
	{
		float outerCutOffAngle=cos(radians(outerCutOff));
		float innercutOffAngle=cos(radians(innerCutOff));

		float spot=dot(normalize(dir), -pos);

		if(spot<outerCutOffAngle)
			return 0.0;
		else
			return pow(smoothstep(outerCutOffAngle, innercutOffAngle, spot), exponent);
	}

	return 1.0;
}

void main()
{
	vec4 Base=texture(TexBase, UV);
	vec3 Specular=vec3(1.0);
	vec3 n=normalize(Tangent*(2*texture(TexNormal, UV)-1).xyz);
	vec3 uE=eye.xyz-Position;
	vec3 e=normalize(uE);
	vec3 r=reflect(-e, n);

	vec3 temp=vec3(0.0);

	for(int i=0;i<NumLights;i++)
	{
		vec3 lPos=Lights[i].Position.xyz-Position;

		// Light volume, distance attenuation, and shadows need to be done before light and eye vector normalization

		// Volume
		vec4 lVolume=vec4(clamp(dot(lPos, uE)/dot(uE, uE), 0.0, 1.0)*uE-lPos, 0.0);
		lVolume.w=1.0/(pow(Lights[i].Position.w*0.5*dot(lVolume.xyz, lVolume.xyz), 2.0)+1.0);

		// Attenuation = 1.0-(Light_Position*(1/Light_Radius))^2
		float lAtten=max(0.0, 1.0-length(lPos*Lights[i].Position.w));

		// Shadow map compare, divide the light distance by the radius to match the depth map distance space
		float Shadow=(texture(TexDistance, vec4(-lPos, i)).x+0.01)>=length(lPos*Lights[i].Position.w)?1.0:0.0;

		// Now we can normalize the light position vector
		lPos=normalize(lPos);

		// Diffuse = Kd*(N.L)
		vec3 lDiffuse=Lights[i].Kd.rgb*max(0.0, dot(lPos, n));

		// Specular = Ks*((R.L)^n)*(N.L)*Gloss
		vec3 lSpecular=Lights[i].Kd.rgb*max(0.0, pow(dot(lPos, r), 16.0)*dot(lPos, n));

		// Multiply spotlight with attenuation, so it mixes in with everything else correctly
		// The light is only a spotlight if the outer cone is greater than 0
		lAtten*=SpotLight(lPos, Lights[i].SpotDirection.xyz, Lights[i].SpotParams.x, Lights[i].SpotParams.y, Lights[i].SpotParams.z);

		// I=(base*diffuse+specular)*shadow*attenuation*lightvolumeatten+volumelight
		temp+=(Base.xyz*lDiffuse+(lSpecular*Specular))*Shadow*lAtten*(1.0-lVolume.w)+(lVolume.w*Lights[i].Kd.xyz);
	}

	Output=vec4(temp, 1.0);
}