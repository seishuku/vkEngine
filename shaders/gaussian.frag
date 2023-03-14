#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D Image;

layout(location=0) out vec4 Output;

layout(push_constant) uniform pc
{
	vec2 direction;
};

void main(void)
{
	const float weights[]=
	{
		0.000951, 0.001768, 0.003140,
		0.005324, 0.008622, 0.013337,
		0.019704, 0.027805, 0.037476,
		0.048243, 0.059316, 0.069658,
		0.078131, 0.083702, 0.085646
	};
	vec2 size=(1.0/textureSize(Image, 0))*direction;

	Output=vec4(0.0);
	Output+=texture(Image, UV+(size*-14.0))*weights[0];
	Output+=texture(Image, UV+(size*-13.0))*weights[1];
	Output+=texture(Image, UV+(size*-12.0))*weights[2];
	Output+=texture(Image, UV+(size*-11.0))*weights[3];
	Output+=texture(Image, UV+(size*-10.0))*weights[4];
	Output+=texture(Image, UV+(size* -9.0))*weights[5];
	Output+=texture(Image, UV+(size* -8.0))*weights[6];
	Output+=texture(Image, UV+(size* -7.0))*weights[7];
	Output+=texture(Image, UV+(size* -6.0))*weights[8];
	Output+=texture(Image, UV+(size* -5.0))*weights[9];
	Output+=texture(Image, UV+(size* -4.0))*weights[10];
	Output+=texture(Image, UV+(size* -3.0))*weights[11];
	Output+=texture(Image, UV+(size* -2.0))*weights[12];
	Output+=texture(Image, UV+(size* -1.0))*weights[13];
	Output+=texture(Image, UV             )*weights[14];
	Output+=texture(Image, UV+(size* +1.0))*weights[13];
	Output+=texture(Image, UV+(size* +2.0))*weights[12];
	Output+=texture(Image, UV+(size* +3.0))*weights[11];
	Output+=texture(Image, UV+(size* +4.0))*weights[10];
	Output+=texture(Image, UV+(size* +5.0))*weights[9];
	Output+=texture(Image, UV+(size* +6.0))*weights[8];
	Output+=texture(Image, UV+(size* +7.0))*weights[7];
	Output+=texture(Image, UV+(size* +8.0))*weights[6];
	Output+=texture(Image, UV+(size* +9.0))*weights[5];
	Output+=texture(Image, UV+(size*+10.0))*weights[4];
	Output+=texture(Image, UV+(size*+11.0))*weights[3];
	Output+=texture(Image, UV+(size*+12.0))*weights[2];
	Output+=texture(Image, UV+(size*+13.0))*weights[1];
	Output+=texture(Image, UV+(size*+14.0))*weights[0];
}
