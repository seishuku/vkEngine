#version 450
#extension GL_ARB_separate_shader_objects: enable

layout (location=0) in vec4 vVert;			// Incoming vertex position
layout (location=1) in vec4 InstancePos;	// Instanced data position
layout (location=2) in vec4 InstanceColor;	// Instanced data color

layout (binding=0) uniform ubo {
	ivec2 Viewport;	// Window width/height
};

layout (location=0) out vec2 UV;			// Output texture coords
layout (location=1) out vec4 Color;			// Output color

void main()
{
	// Transform vertex from window coords to NDC
	gl_Position=vec4(((vVert.xy+InstancePos.xy)/(Viewport*0.5)-1.0)*vec2(1.0, -1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vVert.zw+InstancePos.zw;

	// Pass color
	Color=InstanceColor;
}
