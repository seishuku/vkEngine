#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;
layout(binding=1) uniform sampler2D blur;

layout(location=0) out vec4 Output;

vec4 texBlur(sampler2D image, vec2 UV)
{
	const float weights[]={ 0.0044299121055113265, 0.00895781211794, 0.0215963866053, 0.0443683338718, 0.0776744219933, 0.115876621105, 0.147308056121, 0.159576912161 };
	vec2 size=1.0/textureSize(image, 0), direction;

	vec4 s=vec4(0.0);

	direction=vec2(size.x, 0.0);
	s+=texture(image, UV+(direction*-14.0))*weights[0];
	s+=texture(image, UV+(direction*-12.0))*weights[1];
	s+=texture(image, UV+(direction*-10.0))*weights[2];
	s+=texture(image, UV+(direction*-8.0))*weights[3];
	s+=texture(image, UV+(direction*-6.0))*weights[4];
	s+=texture(image, UV+(direction*-4.0))*weights[5];
	s+=texture(image, UV+(direction*-2.0))*weights[6];
	s+=texture(image, UV                 )*weights[7];
	s+=texture(image, UV+(direction*+2.0))*weights[6];
	s+=texture(image, UV+(direction*+4.0))*weights[5];
	s+=texture(image, UV+(direction*+6.0))*weights[4];
	s+=texture(image, UV+(direction*+8.0))*weights[3];
	s+=texture(image, UV+(direction*+10.0))*weights[2];
	s+=texture(image, UV+(direction*+12.0))*weights[1];
	s+=texture(image, UV+(direction*+14.0))*weights[0];

	direction=vec2(0.0, size.y);
	s+=texture(image, UV+(direction*-14.0))*weights[0];
	s+=texture(image, UV+(direction*-12.0))*weights[1];
	s+=texture(image, UV+(direction*-10.0))*weights[2];
	s+=texture(image, UV+(direction*-8.0))*weights[3];
	s+=texture(image, UV+(direction*-6.0))*weights[4];
	s+=texture(image, UV+(direction*-4.0))*weights[5];
	s+=texture(image, UV+(direction*-2.0))*weights[6];
	s+=texture(image, UV                 )*weights[7];
	s+=texture(image, UV+(direction*+2.0))*weights[6];
	s+=texture(image, UV+(direction*+4.0))*weights[5];
	s+=texture(image, UV+(direction*+6.0))*weights[4];
	s+=texture(image, UV+(direction*+8.0))*weights[3];
	s+=texture(image, UV+(direction*+10.0))*weights[2];
	s+=texture(image, UV+(direction*+12.0))*weights[1];
	s+=texture(image, UV+(direction*+14.0))*weights[0];

	return s;
}

void main(void)
{
	Output=1.0-exp(-(texture(original, UV)+texBlur(blur, UV))*1.0);
//	Output=texBlur(blur, UV);
//	Output=texture(blur, UV);
//	Output=mix(texBlur(blur, UV), texture(original, UV), 0.6);
}
