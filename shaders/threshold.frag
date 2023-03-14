#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;

layout(location=0) out vec4 Output;

vec4 texThreshold(sampler2D image, vec2 uv)
{
    vec4 Image=texture(image, uv);

    // Brightness threshold
    if(dot(Image.xyz, vec3(0.2126, 0.7152, 0.0722))>1.0)
        return Image;
    else
        return vec4(0.0);
}

void main(void)
{
	vec2 size=1.0/textureSize(original, 0);

    // Box filter on downsample to aid in image quality
    vec4 Image=vec4(0.0);
    for(int y=-2;y<=2;y++)
    {
        for(int x=-2;x<=2;x++)
            Image+=texThreshold(original, UV+(size*vec2(x, y)))*0.04;
    }

    Output=Image;
}
