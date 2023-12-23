#version 450

layout (location=0) in vec4 vVert;			// Incoming vertex position
layout (location=1) in vec4 InstancePos;	// Instanced data position
layout (location=2) in vec4 InstanceColor;	// Instanced data color

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
	vec2 pad;
	mat4 mvp;
};

layout (location=0) out vec3 UV;			// Output texture coords
layout (location=1) out vec4 Color;			// Output color

void main()
{
	vec2 Vert=vVert.xy*InstancePos.w;

	// Transform vertex from window coords to NDC, plus flip the Y coord for Vulkan
	gl_Position=mvp*vec4(((Vert+InstancePos.xy)/(Viewport*0.5)-1.0)*vec2(1.0, 1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vec3(vVert.zw, InstancePos.z);

	// Pass color
	Color=vec4(InstanceColor.xyz, 1.0);
}
