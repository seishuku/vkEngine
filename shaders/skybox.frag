#version 450

layout (location=0) in vec3 Position;

layout (binding=1) uniform samplerCube skyboxTex;

layout (location=0) out vec4 Output;

void main()
{
    Output=vec4(texture(skyboxTex, Position).xyz, 1.0);
}
