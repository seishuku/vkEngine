#version 450

layout (location=0) in vec4 vVert;			// Incoming vertex position
layout (location=1) in vec4 InstancePos;	// Instanced data position
layout (location=2) in vec3 InstanceColor;	// Instanced data color

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
};

layout (location=0) out vec3 UV;			// Output texture coords
layout (location=1) out vec4 Color;			// Output color

void main()
{
	// Transform vertex from window coords to NDC, plus flip the Y coord for Vulkan
	gl_Position=vec4(((vVert.xy+InstancePos.xy)/(Viewport*0.5)-1.0)*vec2(1.0, -1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vec3(vVert.zw, InstancePos.z);

	// Pass color
	Color=vec4(InstanceColor, 1.0);
}
