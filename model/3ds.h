#ifndef __3DS_H__
#define __3DS_H__

typedef struct
{
	char Name[32];
	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	vec3 Emission;
	float Shininess;
	char Texture[32];
	uint32_t TexBaseID, TexNormalID, TexSpecularID;
} Material3DS_t;

typedef struct
{
	char Name[32];
	char MaterialName[32];
	int32_t MaterialNumber;

	uint16_t NumVertex;
	float *Vertex, *UV, *Normal, *Tangent, *Binormal;

	uint16_t NumFace;
	uint16_t *Face;

	VkuBuffer_t VertexBuffer, IndexBuffer;
} Mesh3DS_t;

typedef struct
{
	int32_t NumMaterial;
	Material3DS_t *Material;

	int32_t NumMesh;
	Mesh3DS_t *Mesh;
} Model3DS_t;

bool Load3DS(Model3DS_t *Model, char *Filename);
void Free3DS(Model3DS_t *Model);

#endif
