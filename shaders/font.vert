#version 450

layout (location=0) in vec4 InstancePos;	// Instanced data position
layout (location=1) in vec4 InstanceColor;	// Instanced data color

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
	vec2 pad;
	mat4 mvp;
};

layout (location=0) out vec4 UV;			// Output texture coords
layout (location=1) out vec4 Color;			// Output color

const vec4 Verts[4]={
	vec4(-0.5f, 0.5f, -1.0f, 1.0f),
	vec4(-0.5f, -0.5f, -1.0f, -1.0f),
	vec4(0.5f, 0.5f, 1.0f, 1.0f),
	vec4(0.5f, -0.5f, 1.0f, -1.0f)
};

void main()
{
	vec2 Vert=Verts[gl_VertexIndex].xy*InstancePos.w;

	// Transform vertex from window coords to NDC, plus flip the Y coord for Vulkan
	gl_Position=mvp*vec4(((Vert+InstancePos.xy)/(Viewport*0.5)-1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vec4(Verts[gl_VertexIndex].zw, InstancePos.z, InstancePos.w);

	// Pass color
	Color=InstanceColor;
}
