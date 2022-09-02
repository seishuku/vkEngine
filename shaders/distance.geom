#version 450

layout(triangles) in;
layout(triangle_strip, max_vertices=18) out;

layout (binding=0) uniform ubo
{
	mat4 mv[6];
	mat4 proj;
	vec4 Light_Pos;
	int index;
	int pad[11];
};

layout (location=0) in vec4 gPosition[];
layout (location=0) out vec4 Position;

void main()
{
    for(int i=0;i<6;i++)
    {
        gl_Layer=6*index+i;

        for(int j=0;j<3;j++)
        {
            Position=gPosition[j];
            gl_Position=proj*mv[i]*Position;

            EmitVertex();
        }

        EndPrimitive();
    }
}
