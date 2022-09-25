#ifndef __OBJ_H__
#define __OBJ_H__

typedef struct
{
	char Name[255];
	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	vec3 Emission;
	float Shininess;
	char Texture[255];

//	uint32_t TexBaseID, TexNormalID, TexSpecularID;
} MaterialOBJ_t;

typedef struct
{
	char Name[255];
	char MaterialName[255];
	int32_t MaterialNumber;

	uint32_t NumFace;
	uint32_t *Face;

	VkuBuffer_t IndexBuffer;
} MeshOBJ_t;

typedef struct
{
	char MaterialFilename[255];
	uint32_t NumMaterial;
	MaterialOBJ_t *Material;

	uint32_t NumVertex;
	float *Vertex, *UV, *Normal, *Tangent, *Binormal;

	VkuBuffer_t VertexBuffer;

	uint32_t NumMesh;
	MeshOBJ_t *Mesh;
} ModelOBJ_t;

bool LoadOBJ(ModelOBJ_t *Model, const char *Filename);
void FreeOBJ(ModelOBJ_t *Model);
void BuildMemoryBuffersOBJ(VkuContext_t *Context, ModelOBJ_t *Model);

#endif
