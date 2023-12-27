#ifndef __BMODEL_H__
#define __BMODEL_H__

#include <stdint.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"

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
	char name[256];
	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
	vec3 emission;
	float shininess;
	char texture[256];
} BModel_Material_t;

typedef struct
{
	char name[256];
	char materialName[256];
	uint32_t materialNumber;

	uint32_t numFace;
	uint32_t *face;

	VkuBuffer_t indexBuffer;
} BModel_Mesh_t;

typedef struct
{
	uint32_t numMesh;
	BModel_Mesh_t *mesh;

	uint32_t numMaterial;
	BModel_Material_t *material;

	uint32_t numVertex;
	float *vertex;
	float *UV;
	float *tangent;
	float *binormal;
	float *normal;

	VkuBuffer_t vertexBuffer;
} BModel_t;

bool LoadBModel(BModel_t *model, const char *filename);
void FreeBModel(BModel_t *model);
void BuildMemoryBuffersBModel(VkuContext_t *Context, BModel_t *model);

#endif
