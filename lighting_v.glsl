#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vTangent;
layout(location = 3) in vec3 vBinormal;
layout(location = 4) in vec3 vNormal;

layout(binding = 0) uniform ubo {
    mat4 mvp;
    mat4 mvinv;

    vec4 Light0_Pos;
    vec4 Light0_Kd;
    vec4 Light1_Pos;
    vec4 Light1_Kd;
    vec4 Light2_Pos;
    vec4 Light2_Kd;
};

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) out vec3 Position;
layout(location = 1) out vec2 UV;
layout(location = 2) out vec3 TangentX;
layout(location = 3) out vec3 TangentY;
layout(location = 4) out vec3 TangentZ;
layout(location = 5) out vec3 Eye;

void main()
{
	gl_Position=mvp*vec4(vPosition, 1.0);

	Position=vPosition;
	UV=vUV;

	TangentX.x=vTangent.x;
	TangentX.y=vBinormal.x;
	TangentX.z=vNormal.x;

	TangentY.x=vTangent.y;
	TangentY.y=vBinormal.y;
	TangentY.z=vNormal.y;

	TangentZ.x=vTangent.z;
	TangentZ.y=vBinormal.z;
	TangentZ.z=vNormal.z;
}
