#version 450

layout (location=0) in vec4 vVert;			// Incoming vertex position

layout (location=1) in vec4 InstancePos;	// Instanced data position and size
layout (location=2) in vec4 InstanceColor;	// Instanced data color and value
layout (location=3) in uvec4 InstanceType;	// Instanced data type

layout (push_constant) uniform ubo {
	ivec2 Viewport;	// Window width/height
};

layout (location=0) out vec2 UV;			// Output coords
layout (location=1) out flat vec4 Color;	// Control color
layout (location=2) out flat uint Type;		// Control type
layout (location=3) out flat vec2 Size;		// Control size

void main()
{
	vec2 Vert=vVert.xy*InstancePos.zw;

	// Transform vertex from window coords to NDC, plus flip the Y coord for Vulkan
	gl_Position=vec4(((Vert+InstancePos.xy)/(Viewport*0.5)-1.0)*vec2(1.0, -1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vVert.zw;

	// Pass color
	Color=InstanceColor;

	// Pass type
	Type=InstanceType.x;

	// Pass size
	Size=InstancePos.zw;
}
