#ifndef __BMODEL_H__
#define __BMODEL_H__

#include <stdint.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"

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

	vec3 bBoxMin;
	vec3 bBoxMax;
	vec3 center;
	float radius;
} BModel_t;

bool LoadBModel(BModel_t *model, const char *filename);
void FreeBModel(BModel_t *model);
void BuildMemoryBuffersBModel(VkuContext_t *Context, BModel_t *model);

#endif
