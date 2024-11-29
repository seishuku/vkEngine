#version 450

layout (location=0) in vec4 vPosition;

layout (push_constant) uniform ubo
{
	ivec2 Viewport;
	ivec2 pad;
	mat4 mvp;
	vec4 Color;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
	gl_Position=mvp*vec4((vPosition.xy/(Viewport*0.5)-1.0)*vec2(1.0, 1.0), 0.0, 1.0);
}
