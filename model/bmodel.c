#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "bmodel.h"

const uint32_t BMDL_MAGIC='B'|'M'<<8|'D'<<16|'L'<<24;
const uint32_t MESH_MAGIC='M'|'E'<<8|'S'<<16|'H'<<24;
const uint32_t MATL_MAGIC='M'|'A'<<8|'T'<<16|'L'<<24;
const uint32_t VERT_MAGIC='V'|'E'<<8|'R'<<16|'T'<<24;
const uint32_t TEXC_MAGIC='T'|'E'<<8|'X'<<16|'C'<<24;
const uint32_t TANG_MAGIC='T'|'A'<<8|'N'<<16|'G'<<24;
const uint32_t BNRM_MAGIC='B'|'N'<<8|'R'<<16|'M'<<24;
const uint32_t NORM_MAGIC='N'|'O'<<8|'R'<<16|'M'<<24;

static void CalculateTangent(BModel_t *model)
{
	if(model->UV)
	{
		vec3 v0, v1, s, t, n;
		vec2 uv0, uv1;
		float r;

		if(model->tangent)
			Zone_Free(zone, model->tangent);

		model->tangent=(float *)Zone_Malloc(zone, sizeof(float)*3*model->numVertex);

		if(model->tangent==NULL)
			return;

		memset(model->tangent, 0, sizeof(float)*3*model->numVertex);

		if(model->binormal)
			Zone_Free(zone, model->binormal);

		model->binormal=(float *)Zone_Malloc(zone, sizeof(float)*3*model->numVertex);

		if(model->binormal==NULL)
			return;

		memset(model->binormal, 0, sizeof(float)*3*model->numVertex);

		if(model->normal)
			Zone_Free(zone, model->normal);

		model->normal=(float *)Zone_Malloc(zone, sizeof(float)*3*model->numVertex);

		if(model->normal==NULL)
			return;

		memset(model->normal, 0, sizeof(float)*3*model->numVertex);

		for(uint32_t j=0;j<model->numMesh;j++)
		{
			for(uint32_t i=0;i<model->mesh[j].numFace;i++)
			{
				uint32_t i1=model->mesh[j].face[3*i+0];
				uint32_t i2=model->mesh[j].face[3*i+1];
				uint32_t i3=model->mesh[j].face[3*i+2];

				v0.x=model->vertex[3*i2+0]-model->vertex[3*i1+0];
				v0.y=model->vertex[3*i2+1]-model->vertex[3*i1+1];
				v0.z=model->vertex[3*i2+2]-model->vertex[3*i1+2];

				uv0.x=model->UV[2*i2+0]-model->UV[2*i1+0];
				uv0.y=model->UV[2*i2+1]-model->UV[2*i1+1];

				v1.x=model->vertex[3*i3+0]-model->vertex[3*i1+0];
				v1.y=model->vertex[3*i3+1]-model->vertex[3*i1+1];
				v1.z=model->vertex[3*i3+2]-model->vertex[3*i1+2];

				uv1.x=model->UV[2*i3+0]-model->UV[2*i1+0];
				uv1.y=model->UV[2*i3+1]-model->UV[2*i1+1];

				r=(uv0.x*uv1.y-uv1.x*uv0.y);
				r=(fabsf(r)<1e-8f)?0.0f:1.0f/r;

				s.x=(uv1.y*v0.x-uv0.y*v1.x)*r;
				s.y=(uv1.y*v0.y-uv0.y*v1.y)*r;
				s.z=(uv1.y*v0.z-uv0.y*v1.z)*r;
				Vec3_Normalize(&s);

				model->tangent[3*i1+0]+=s.x;	model->tangent[3*i1+1]+=s.y;	model->tangent[3*i1+2]+=s.z;
				model->tangent[3*i2+0]+=s.x;	model->tangent[3*i2+1]+=s.y;	model->tangent[3*i2+2]+=s.z;
				model->tangent[3*i3+0]+=s.x;	model->tangent[3*i3+1]+=s.y;	model->tangent[3*i3+2]+=s.z;

				t.x=(uv0.x*v1.x-uv1.x*v0.x)*r;
				t.y=(uv0.x*v1.y-uv1.x*v0.y)*r;
				t.z=(uv0.x*v1.z-uv1.x*v0.z)*r;
				Vec3_Normalize(&t);

				model->binormal[3*i1+0]+=t.x;	model->binormal[3*i1+1]+=t.y;	model->binormal[3*i1+2]+=t.z;
				model->binormal[3*i2+0]+=t.x;	model->binormal[3*i2+1]+=t.y;	model->binormal[3*i2+2]+=t.z;
				model->binormal[3*i3+0]+=t.x;	model->binormal[3*i3+1]+=t.y;	model->binormal[3*i3+2]+=t.z;

				n=Vec3_Cross(v0, v1);
				Vec3_Normalize(&n);

				model->normal[3*i1+0]+=n.x;	model->normal[3*i1+1]+=n.y;	model->normal[3*i1+2]+=n.z;
				model->normal[3*i2+0]+=n.x;	model->normal[3*i2+1]+=n.y;	model->normal[3*i2+2]+=n.z;
				model->normal[3*i3+0]+=n.x;	model->normal[3*i3+1]+=n.y;	model->normal[3*i3+2]+=n.z;
			}
		}

		for(uint32_t i=0;i<model->numVertex;i++)
		{
			vec3 t, b, n;

			t=Vec3(model->tangent[3*i+0], model->tangent[3*i+1], model->tangent[3*i+2]);
			b=Vec3(model->binormal[3*i+0], model->binormal[3*i+1], model->binormal[3*i+2]);
			n=Vec3(model->normal[3*i+0], model->normal[3*i+1], model->normal[3*i+2]);

			t=Vec3_Subv(t, Vec3_Muls(n, Vec3_Dot(n, t)));

			Vec3_Normalize(&t);
			Vec3_Normalize(&b);
			Vec3_Normalize(&n);

			vec3 NxT=Vec3_Cross(n, t);

			if(Vec3_Dot(NxT, b)<0.0f)
				t=Vec3_Muls(t, -1.0f);

			b=NxT;

			memcpy(&model->tangent[3*i], &t, sizeof(float)*3);
			memcpy(&model->binormal[3*i], &b, sizeof(float)*3);
			memcpy(&model->normal[3*i], &n, sizeof(float)*3);
		}
	}
}

static vec3 CalculateCenterMass(BModel_t *model)
{
	vec3 centerOfMass=Vec3b(0.0f);
	uint32_t vertexCount=0;

	for(uint32_t i=0;i<model->numMesh;i++)
	{
		for(uint32_t j=0;j<model->mesh[i].numFace*3;j++)
		{
			const vec3 vertex=Vec3(model->vertex[3*model->mesh[i].face[j]+0], model->vertex[3*model->mesh[i].face[j]+1], model->vertex[3*model->mesh[i].face[j]+2]);

			centerOfMass=Vec3_Addv(centerOfMass, vertex);
			vertexCount++;
		}
	}

	return Vec3_Muls(centerOfMass, 1.0f/(float)vertexCount);
}

static void CalculateBounds(BModel_t *model)
{
	model->bBoxMin=Vec3b(FLT_MIN);
	model->bBoxMax=Vec3b(-FLT_MAX);
	model->center=CalculateCenterMass(model);
	model->radius=0.0f;

	for(uint32_t i=0;i<model->numMesh;i++)
	{
		for(uint32_t j=0;j<model->mesh[i].numFace*3;j++)
		{
			const vec3 vertex=Vec3(model->vertex[3*model->mesh[i].face[j]+0], model->vertex[3*model->mesh[i].face[j]+1], model->vertex[3*model->mesh[i].face[j]+2]);
			float distance=Vec3_Distance(vertex, model->center);

			if(distance>model->radius)
				model->radius=distance;

			if(vertex.x<model->bBoxMin.x)	model->bBoxMin.x=vertex.x;
			if(vertex.y<model->bBoxMin.y)	model->bBoxMin.y=vertex.y;
			if(vertex.z<model->bBoxMin.z)	model->bBoxMin.z=vertex.z;

			if(vertex.x>model->bBoxMax.x)	model->bBoxMax.x=vertex.x;
			if(vertex.y>model->bBoxMax.y)	model->bBoxMax.y=vertex.y;
			if(vertex.z>model->bBoxMax.z)	model->bBoxMax.z=vertex.z;
		}
	}
}

static void ReadString(char *string, size_t stringLength, FILE *fp)
{
	uint32_t i=0;
	char ch=127;

	while(ch!='\0'&&i<stringLength)
	{
		ch=fgetc(fp);
		string[i++]=ch;
	}
}

bool LoadBModel(BModel_t *model, const char *filename)
{
	FILE *fp;

	if(!(fp=fopen(filename, "rb")))
		return false;

	uint32_t magic=0;
	fread(&magic, sizeof(uint32_t), 1, fp);

	if(magic!=BMDL_MAGIC)
		return false;

	memset(model, 0, sizeof(BModel_t));

	/////// Read in materials

	// Get number of materials
	fread(&model->numMaterial, sizeof(uint32_t), 1, fp);

	// If there materials, allocate memory for them
	if(model->numMaterial)
	{
		model->material=(BModel_Material_t *)Zone_Malloc(zone, sizeof(BModel_Material_t)*model->numMaterial);

		if(model->material==NULL)
			return false;
	}

	// Read them in
	for(uint32_t i=0;i<model->numMaterial;i++)
	{
		uint32_t Magic=0;
		fread(&Magic, sizeof(uint32_t), 1, fp);

		if(Magic!=MATL_MAGIC)
			return false;

		ReadString(model->material[i].name, 256, fp);
		fread(&model->material[i].ambient, sizeof(float), 3, fp);
		fread(&model->material[i].diffuse, sizeof(float), 3, fp);
		fread(&model->material[i].specular, sizeof(float), 3, fp);
		fread(&model->material[i].emission, sizeof(float), 3, fp);
		fread(&model->material[i].shininess, sizeof(float), 1, fp);
		ReadString(model->material[i].texture, 256, fp);
	}
	///////

	/////// Read in meshes

	// Get number of meshes
	fread(&model->numMesh, sizeof(uint32_t), 1, fp);

	// If there meshes, allocate memory for them
	if(model->numMesh)
	{
		model->mesh=(BModel_Mesh_t *)Zone_Malloc(zone, sizeof(BModel_Mesh_t)*model->numMesh);

		if(model->mesh==NULL)
			return false;
	}

	// Read them in
	for(uint32_t i=0;i<model->numMesh;i++)
	{
		uint32_t magic=0;
		fread(&magic, sizeof(uint32_t), 1, fp);

		if(magic!=MESH_MAGIC)
			return false;

		ReadString(model->mesh[i].name, 256, fp);
		ReadString(model->mesh[i].materialName, 256, fp);

		fread(&model->mesh[i].numFace, sizeof(uint32_t), 1, fp);

		if(model->mesh[i].numFace)
		{
			model->mesh[i].face=(uint32_t *)Zone_Malloc(zone, sizeof(uint32_t)*model->mesh[i].numFace*3);

			if(model->mesh[i].face==NULL)
				return false;

			fread(model->mesh[i].face, sizeof(uint32_t), model->mesh[i].numFace*3, fp);
		}
	}

	// Read in number of vertices
	fread(&model->numVertex, sizeof(uint32_t), 1, fp);

	// If there are vertices
	if(model->numVertex)
	{
		// Read out until end of file
		while(!feof(fp))
		{
			uint32_t magic=0;

			// Read magic ID for chunk
			if(!fread(&magic, sizeof(uint32_t), 1, fp))
				break;

			switch(magic)
			{
				case VERT_MAGIC:
					model->vertex=(float *)Zone_Malloc(zone, sizeof(float)*model->numVertex*3);

					if(model->vertex==NULL)
						return false;

					if(fread(model->vertex, sizeof(float)*3, model->numVertex, fp)!=model->numVertex)
					{
						Zone_Free(zone, model->vertex);
						model->vertex=NULL;
					}
					break;

				case TEXC_MAGIC:
					model->UV=(float *)Zone_Malloc(zone, sizeof(float)*model->numVertex*2);

					if(model->UV==NULL)
						return false;

					if(fread(model->UV, sizeof(float)*2, model->numVertex, fp)!=model->numVertex)
					{
						Zone_Free(zone, model->UV);
						model->UV=NULL;
					}
					break;

				case TANG_MAGIC:
					model->tangent=(float *)Zone_Malloc(zone, sizeof(float)*model->numVertex*3);

					if(model->tangent==NULL)
						return false;

					if(fread(model->tangent, sizeof(float)*3, model->numVertex, fp)!=model->numVertex)
					{
						Zone_Free(zone, model->tangent);
						model->tangent=NULL;
					}
					break;

				case BNRM_MAGIC:
					model->binormal=(float *)Zone_Malloc(zone, sizeof(float)*model->numVertex*3);

					if(model->binormal==NULL)
						return false;

					if(fread(model->binormal, sizeof(float)*3, model->numVertex, fp)!=model->numVertex)
					{
						Zone_Free(zone, model->binormal);
						model->binormal=NULL;
					}
					break;

				case NORM_MAGIC:
					model->normal=(float *)Zone_Malloc(zone, sizeof(float)*model->numVertex*3);

					if(model->normal==NULL)
						return false;

					if(fread(model->normal, sizeof(float)*3, model->numVertex, fp)!=model->numVertex)
					{
						Zone_Free(zone, model->normal);
						model->normal=NULL;
					}
					break;

				default:
					break;
			}
		}
	}

	CalculateTangent(model);

	CalculateBounds(model);

	DBGPRINTF(DEBUG_INFO, "Model: %s\n\tMin: %f %f %f Max: %f %f %f\n\tCenter: %f %f %f Radius: %f\n", model->mesh[0].name, model->bBoxMin.x, model->bBoxMin.y, model->bBoxMin.z, model->bBoxMax.x, model->bBoxMax.y, model->bBoxMax.z, model->center.x, model->center.y, model->center.z, model->radius);

	// Match up material names with their meshes with an index number
	if(model->numMaterial)
	{
		for(uint32_t i=0;i<model->numMesh;i++)
		{
			for(uint32_t j=0;j<model->numMaterial;j++)
			{
				if(strcmp(model->mesh[i].materialName, model->material[j].name)==0)
					model->mesh[i].materialNumber=j;
			}
		}
	}

	return true;
}

// Free memory allocated for the model
void FreeBModel(BModel_t *model)
{
	Zone_Free(zone, model->vertex);
	Zone_Free(zone, model->UV);
	Zone_Free(zone, model->normal);
	Zone_Free(zone, model->tangent);
	Zone_Free(zone, model->binormal);

	if(model->numMesh)
	{
		/* Free mesh data */
		for(uint32_t i=0;i<model->numMesh;i++)
			Zone_Free(zone, model->mesh[i].face);

		Zone_Free(zone, model->mesh);
	}

	Zone_Free(zone, model->material);
}

void BuildMemoryBuffersBModel(VkuContext_t *context, BModel_t *model)
{
	VkCommandBuffer copyCommand=VK_NULL_HANDLE;
	VkuBuffer_t stagingBuffer;

	// Vertex data on device memory
	vkuCreateGPUBuffer(context, &model->vertexBuffer, sizeof(float)*20*model->numVertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Create staging buffer to transfer from host memory to device memory
	vkuCreateHostBuffer(context, &stagingBuffer, sizeof(float)*20*model->numVertex, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	if(!stagingBuffer.memory->mappedPointer)
		return;

	float *fPtr=(float *)stagingBuffer.memory->mappedPointer;

	for(uint32_t j=0;j<model->numVertex;j++)
	{
		*fPtr++=model->vertex[3*j+0];
		*fPtr++=model->vertex[3*j+1];
		*fPtr++=model->vertex[3*j+2];
		*fPtr++=1.0f;

		*fPtr++=model->UV[2*j+0];
		*fPtr++=1.0f-model->UV[2*j+1];
		*fPtr++=0.0f;
		*fPtr++=0.0f;

		*fPtr++=model->tangent[3*j+0];
		*fPtr++=model->tangent[3*j+1];
		*fPtr++=model->tangent[3*j+2];
		*fPtr++=0.0f;

		*fPtr++=model->binormal[3*j+0];
		*fPtr++=model->binormal[3*j+1];
		*fPtr++=model->binormal[3*j+2];
		*fPtr++=0.0f;

		*fPtr++=model->normal[3*j+0];
		*fPtr++=model->normal[3*j+1];
		*fPtr++=model->normal[3*j+2];
		*fPtr++=0.0f;
	}

	// Copy to device memory
	copyCommand=vkuOneShotCommandBufferBegin(context);
	vkCmdCopyBuffer(copyCommand, stagingBuffer.buffer, model->vertexBuffer.buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(float)*20*model->numVertex });
	vkuOneShotCommandBufferEnd(context, copyCommand);

	// Delete staging data
	vkuDestroyBuffer(context, &stagingBuffer);

	for(uint32_t i=0;i<model->numMesh;i++)
	{
		// Index data
		vkuCreateGPUBuffer(context, &model->mesh[i].indexBuffer, sizeof(uint32_t)*model->mesh[i].numFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		// Staging buffer
		vkuCreateHostBuffer(context, &stagingBuffer, sizeof(uint32_t)*model->mesh[i].numFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		if(!stagingBuffer.memory->mappedPointer)
			return;

		uint32_t *iPtr=(uint32_t *)stagingBuffer.memory->mappedPointer;

		for(uint32_t j=0;j<model->mesh[i].numFace;j++)
		{
			*iPtr++=model->mesh[i].face[3*j+0];
			*iPtr++=model->mesh[i].face[3*j+1];
			*iPtr++=model->mesh[i].face[3*j+2];
		}

		copyCommand=vkuOneShotCommandBufferBegin(context);
		vkCmdCopyBuffer(copyCommand, stagingBuffer.buffer, model->mesh[i].indexBuffer.buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(uint32_t)*model->mesh[i].numFace*3 });
		vkuOneShotCommandBufferEnd(context, copyCommand);

		// Delete staging data
		vkuDestroyBuffer(context, &stagingBuffer);
	}
}
