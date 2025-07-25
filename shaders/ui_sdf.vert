#version 450

layout (location=0) in vec4 vVert;			// Incoming vertex position

layout (location=1) in vec4 InstancePos;	// Instanced data position and size
layout (location=2) in vec4 InstanceColor;	// Instanced data color and value
layout (location=3) in uvec4 InstanceType;	// Instanced data type

layout (push_constant) uniform ubo {
	vec2 Viewport;	// Window width/height
	vec2 Pad;
	mat4 mvp;
};

layout (location=0) out vec2 UV;					// Output coords
layout (location=1) out flat vec4 Color;			// Control color
layout (location=2) out flat uint Type;				// Control type
layout (location=3) out flat uint Flag;				// Control flag
layout (location=4) out flat vec2 Size;				// Control size

const uint UI_CONTROL_BARGRAPH	=0;
const uint UI_CONTROL_BUTTON	=1;
const uint UI_CONTROL_CHECKBOX	=2;
const uint UI_CONTROL_CURSOR	=3;
const uint UI_CONTROL_EDITTEXT	=4;
const uint UI_CONTROL_SPRITE	=5;
const uint UI_CONTROL_TEXT		=6;
const uint UI_CONTROL_WINDOW	=7;

vec2 rotate(vec2 v, float a)
{
	float s=sin(a);
	float c=cos(a);

	return mat2(c, s, -s, c)*v;
}

void main()
{
	vec2 Vert=vVert.xy;

	if(InstanceType.x==UI_CONTROL_TEXT)
		Vert*=InstancePos.w;
	else
		Vert*=InstancePos.zw;

	if(InstanceType.x==UI_CONTROL_SPRITE)
		Vert=rotate(Vert, InstanceColor.w);

	// Transform vertex from window coords to NDC, plus flip the Y coord for Vulkan
	gl_Position=mvp*vec4(((Vert+InstancePos.xy)/(Viewport*0.5)-1.0)*vec2(1.0, 1.0), 0.0, 1.0);

	// Offset texture coords to position in texture atlas
	UV=vVert.zw;

	// Pass color
	Color=InstanceColor;

	// Pass type
	Type=InstanceType.x;

	// Pass flag
	Flag=InstanceType.y;

	// Pass size
	Size=InstancePos.zw;
}
