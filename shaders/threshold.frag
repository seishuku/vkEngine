#version 450

layout(location=0) in vec2 UV;

layout(binding=0) uniform sampler2D original;

layout(location=0) out vec4 Output;

void main(void)
{
	vec2 size=1.0/textureSize(original, 0);

    vec4 Image=vec4(0.0);

    for(int y=-1;y<1;y++)
    {
        for(int x=-1;x<1;x++)
            Image+=texture(original, UV+(size*vec2(x, y)))*0.111;
    }

    float brightness=dot(Image.xyz, vec3(0.2126, 0.7152, 0.0722));

    if(brightness>1.0)
        Output=Image;
    else
        Output=vec4(0.0);
}
