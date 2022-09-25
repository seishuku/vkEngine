#ifndef __BMODEL_H__
#define __BMODEL_H__

#define BMDL_MAGIC			(((uint32_t)'B')|((uint32_t)'M')<<8|((uint32_t)'D')<<16|((uint32_t)'L')<<24)
#define MESH_MAGIC			(((uint32_t)'M')|((uint32_t)'E')<<8|((uint32_t)'S')<<16|((uint32_t)'H')<<24)
#define MATL_MAGIC			(((uint32_t)'M')|((uint32_t)'A')<<8|((uint32_t)'T')<<16|((uint32_t)'L')<<24)
#define VERT_MAGIC			(((uint32_t)'V')|((uint32_t)'E')<<8|((uint32_t)'R')<<16|((uint32_t)'T')<<24)
#define TEXC_MAGIC			(((uint32_t)'T')|((uint32_t)'E')<<8|((uint32_t)'X')<<16|((uint32_t)'C')<<24)
#define TANG_MAGIC			(((uint32_t)'T')|((uint32_t)'A')<<8|((uint32_t)'N')<<16|((uint32_t)'G')<<24)
#define BNRM_MAGIC			(((uint32_t)'B')|((uint32_t)'N')<<8|((uint32_t)'R')<<16|((uint32_t)'M')<<24)
#define NORM_MAGIC			(((uint32_t)'N')|((uint32_t)'O')<<8|((uint32_t)'R')<<16|((uint32_t)'M')<<24)

typedef struct
{
	char Name[256];
	vec3 Ambient;
	vec3 Diffuse;
	vec3 Specular;
	vec3 Emission;
	float Shininess;
	char Texture[256];
} BModel_Material_t;

typedef struct
{
	char Name[256];
	char MaterialName[256];
	uint32_t MaterialNumber;

	uint32_t NumFace;
	uint32_t *Face;

	VkuBuffer_t IndexBuffer;
} BModel_Mesh_t;

typedef struct
{
	uint32_t NumMesh;
	BModel_Mesh_t *Mesh;

	uint32_t NumMaterial;
	BModel_Material_t *Material;

	uint32_t NumVertex;
	float *Vertex;
	float *UV;
	float *Tangent;
	float *Binormal;
	float *Normal;

	VkuBuffer_t VertexBuffer;
} BModel_t;

bool LoadBModel(BModel_t *Model, const char *Filename);
void FreeBModel(BModel_t *Model);
void BuildMemoryBuffersBModel(VkuContext_t *Context, BModel_t *Model);

#endif
