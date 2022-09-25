#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "obj.h"

void CalculateTangentOBJ(ModelOBJ_t *Model)
{
	if(Model->UV)
	{
		vec3 v0, v1, s, t, n;
		vec2 uv0, uv1;
		float r;

		Model->Tangent=(float *)Zone_Malloc(Zone, sizeof(float)*3*Model->NumVertex);

		if(Model->Tangent==NULL)
			return;

		memset(Model->Tangent, 0, sizeof(float)*3*Model->NumVertex);

		Model->Binormal=(float *)Zone_Malloc(Zone, sizeof(float)*3*Model->NumVertex);

		if(Model->Binormal==NULL)
			return;

		memset(Model->Binormal, 0, sizeof(float)*3*Model->NumVertex);

		Model->Normal=(float *)Zone_Malloc(Zone, sizeof(float)*3*Model->NumVertex);

		if(Model->Normal==NULL)
			return;

		memset(Model->Normal, 0, sizeof(float)*3*Model->NumVertex);

		for(uint32_t j=0;j<Model->NumMesh;j++)
		{
			for(uint32_t i=0;i<Model->Mesh[j].NumFace;i++)
			{
				uint32_t i1=Model->Mesh[j].Face[3*i+0];
				uint32_t i2=Model->Mesh[j].Face[3*i+1];
				uint32_t i3=Model->Mesh[j].Face[3*i+2];

				v0[0]=Model->Vertex[3*i2+0]-Model->Vertex[3*i1+0];
				v0[1]=Model->Vertex[3*i2+1]-Model->Vertex[3*i1+1];
				v0[2]=Model->Vertex[3*i2+2]-Model->Vertex[3*i1+2];

				uv0[0]=Model->UV[2*i2+0]-Model->UV[2*i1+0];
				uv0[1]=Model->UV[2*i2+1]-Model->UV[2*i1+1];

				v1[0]=Model->Vertex[3*i3+0]-Model->Vertex[3*i1+0];
				v1[1]=Model->Vertex[3*i3+1]-Model->Vertex[3*i1+1];
				v1[2]=Model->Vertex[3*i3+2]-Model->Vertex[3*i1+2];

				uv1[0]=Model->UV[2*i3+0]-Model->UV[2*i1+0];
				uv1[1]=Model->UV[2*i3+1]-Model->UV[2*i1+1];

				r=1.0f/(uv0[0]*uv1[1]-uv1[0]*uv0[1]);

				s[0]=(uv1[1]*v0[0]-uv0[1]*v1[0])*r;
				s[1]=(uv1[1]*v0[1]-uv0[1]*v1[1])*r;
				s[2]=(uv1[1]*v0[2]-uv0[1]*v1[2])*r;
				Vec3_Normalize(s);

				Model->Tangent[3*i1+0]+=s[0];	Model->Tangent[3*i1+1]+=s[1];	Model->Tangent[3*i1+2]+=s[2];
				Model->Tangent[3*i2+0]+=s[0];	Model->Tangent[3*i2+1]+=s[1];	Model->Tangent[3*i2+2]+=s[2];
				Model->Tangent[3*i3+0]+=s[0];	Model->Tangent[3*i3+1]+=s[1];	Model->Tangent[3*i3+2]+=s[2];

				t[0]=(uv0[0]*v1[0]-uv1[0]*v0[0])*r;
				t[1]=(uv0[0]*v1[1]-uv1[0]*v0[1])*r;
				t[2]=(uv0[0]*v1[2]-uv1[0]*v0[2])*r;
				Vec3_Normalize(t);

				Model->Binormal[3*i1+0]+=t[0];	Model->Binormal[3*i1+1]+=t[1];	Model->Binormal[3*i1+2]+=t[2];
				Model->Binormal[3*i2+0]+=t[0];	Model->Binormal[3*i2+1]+=t[1];	Model->Binormal[3*i2+2]+=t[2];
				Model->Binormal[3*i3+0]+=t[0];	Model->Binormal[3*i3+1]+=t[1];	Model->Binormal[3*i3+2]+=t[2];

				Cross(v0, v1, n);
				Vec3_Normalize(n);

				Model->Normal[3*i1+0]+=n[0];	Model->Normal[3*i1+1]+=n[1];	Model->Normal[3*i1+2]+=n[2];
				Model->Normal[3*i2+0]+=n[0];	Model->Normal[3*i2+1]+=n[1];	Model->Normal[3*i2+2]+=n[2];
				Model->Normal[3*i3+0]+=n[0];	Model->Normal[3*i3+1]+=n[1];	Model->Normal[3*i3+2]+=n[2];
			}
		}

		for(uint32_t i=0;i<Model->NumVertex;i++)
		{
			float *t=&Model->Tangent[3*i];
			float *b=&Model->Binormal[3*i];
			float *n=&Model->Normal[3*i];

			float d=Vec3_Dot(n, t);
			t[0]-=n[0]*d;
			t[1]-=n[1]*d;
			t[2]-=n[2]*d;
			Vec3_Normalize(t);
			Vec3_Normalize(b);
			Vec3_Normalize(n);

			vec3 NxT;
			Cross(n, t, NxT);

			if(Vec3_Dot(NxT, b)<0.0f)
				Vec3_Muls(t, -1.0f);

			Vec3_Setv(b, NxT);
		}
	}
}

bool LoadMTL(ModelOBJ_t *Model, const char *Filename)
{
	FILE *fp;
	char buff[512];

	if(!(fp=fopen(Filename, "r")))
		return false;

	// First pass, count materials for allocation
	while(!feof(fp))
	{
		// Read line
		fgets(buff, sizeof(buff), fp);

		if(strncmp(buff, "newmtl ", 7)==0)
			Model->NumMaterial++;
	}

	fseek(fp, 0, SEEK_SET);

	Model->Material=(MaterialOBJ_t *)Zone_Malloc(Zone, sizeof(MaterialOBJ_t)*Model->NumMaterial);

	if(Model->Material==NULL)
		return false;

	Model->NumMaterial=0;

	// Second pass, read materials
	while(!feof(fp))
	{
		// Read line
		fgets(buff, sizeof(buff), fp);

		if(strncmp(buff, "newmtl ", 7)==0)
		{
			Model->NumMaterial++;

			memset(&Model->Material[Model->NumMaterial-1], 0, sizeof(MaterialOBJ_t));

			if(sscanf(buff, "newmtl %s", Model->Material[Model->NumMaterial-1].Name)!=1)
				return false;
		}
		else if(strncmp(buff, "Ka ", 3)==0)
		{
			if(sscanf(buff, "Ka %f %f %f",
				&Model->Material[Model->NumMaterial-1].Ambient[0],
				&Model->Material[Model->NumMaterial-1].Ambient[1],
				&Model->Material[Model->NumMaterial-1].Ambient[2])!=3)
				return false;
		}
		else if(strncmp(buff, "Kd ", 3)==0)
		{
			if(sscanf(buff, "Kd %f %f %f",
				&Model->Material[Model->NumMaterial-1].Diffuse[0],
				&Model->Material[Model->NumMaterial-1].Diffuse[1],
				&Model->Material[Model->NumMaterial-1].Diffuse[2])!=3)
				return false;
		}
		else if(strncmp(buff, "Ks ", 3)==0)
		{
			if(sscanf(buff, "Ks %f %f %f",
				&Model->Material[Model->NumMaterial-1].Specular[0],
				&Model->Material[Model->NumMaterial-1].Specular[1],
				&Model->Material[Model->NumMaterial-1].Specular[2])!=3)
				return false;
		}
		else if(strncmp(buff, "Ke ", 3)==0)
		{
			if(sscanf(buff, "Ke %f %f %f",
				&Model->Material[Model->NumMaterial-1].Emission[0],
				&Model->Material[Model->NumMaterial-1].Emission[1],
				&Model->Material[Model->NumMaterial-1].Emission[2])!=3)
				return false;
		}
		else if(strncmp(buff, "Ns ", 3)==0)
		{
			if(sscanf(buff, "Ns %f",
				&Model->Material[Model->NumMaterial-1].Shininess)!=1)
				return false;
		}
		else if(strncmp(buff, "map_Kd ", 7)==0)
		{
			if(sscanf(buff, "map_Kd %s", Model->Material[Model->NumMaterial-1].Texture)!=1)
				return false;
		}
	}

	// Match up material names with their meshes with an index number
	if(Model->Material)
	{
		for(uint32_t i=0;i<Model->NumMesh;i++)
		{
			for(uint32_t j=0;j<Model->NumMaterial;j++)
			{
				if(strcmp(Model->Mesh[i].MaterialName, Model->Material[j].Name)==0)
					Model->Mesh[i].MaterialNumber=j;
			}
		}
	}

	return true;
}

// Apparently Windows CRT lib doesn't have this?
char *strrstr(const char *haystack, const char *needle)
{
	char *r=NULL;

	if(!needle[0])
		return (char *)haystack+strlen(haystack);
	while(1)
	{
		char *p=strstr(haystack, needle);
		if(!p)
			return r;
		r=p;
		haystack=p+1;
	}
}

bool LoadOBJ(ModelOBJ_t *Model, const char *Filename)
{
	FILE *fp;
	char buff[512];
	uint32_t NumUV=0;
	uint32_t vi[3]={ 0, 0, 0 };
	uint32_t ti[3]={ 0, 0, 0 };
	uint32_t ni[3]={ 0, 0, 0 };

	if(!(fp=fopen(Filename, "r")))
		return false;

	memset(Model, 0, sizeof(ModelOBJ_t));

	// Multiple passes, not a huge fan of this.

	// First pass, count number of meshes (objects) and vertices
	while(!feof(fp))
	{
		// Read line
		fgets(buff, sizeof(buff), fp);

		if(strncmp(buff, "o ", 2)==0)
			Model->NumMesh++;
		else if(strncmp(buff, "v ", 2)==0)
			Model->NumVertex++;
		else if(strncmp(buff, "vt ", 3)==0)
			NumUV++;
	}

	Model->Mesh=(MeshOBJ_t *)Zone_Malloc(Zone, sizeof(MeshOBJ_t)*Model->NumMesh);

	if(Model->Mesh==NULL)
		return false;

	Model->NumMesh=0;

	Model->Vertex=(float *)Zone_Malloc(Zone, sizeof(float)*3*Model->NumVertex);

	if(Model->Vertex==NULL)
		return false;

	Model->NumVertex=0;

	Model->UV=(float *)Zone_Malloc(Zone, sizeof(float)*2*NumUV);

	if(Model->UV==NULL)
		return false;

	NumUV=0;

	fseek(fp, 0, SEEK_SET);

	// Second pass, count faces, resetting on each object to split into different meshes, also read in object names and assigned material names
	while(!feof(fp))
	{
		// Read line
		fgets(buff, sizeof(buff), fp);

		if(strncmp(buff, "o ", 2)==0)
		{
			Model->NumMesh++;

			memset(&Model->Mesh[Model->NumMesh-1], 0, sizeof(MeshOBJ_t));

			if(sscanf(buff, "o %s", Model->Mesh[Model->NumMesh-1].Name)!=1)
				return false;
		}
		else if(strncmp(buff, "usemtl ", 7)==0)
		{
			if(Model->Mesh)
			{
				if(sscanf(buff, "usemtl %s", Model->Mesh[Model->NumMesh-1].MaterialName)!=1)
					return false;
			}
		}
		else if(strncmp(buff, "f ", 2)==0)
			Model->Mesh[Model->NumMesh-1].NumFace++;
	}

	Model->NumMesh=0;

	fseek(fp, 0, SEEK_SET);

	// Third pass, read faces, vertex data, and anything else remaining
	while(!feof(fp))
	{
		// Read line
		fgets(buff, sizeof(buff), fp);

		if(strncmp(buff, "o ", 2)==0)
		{
			Model->NumMesh++;

			Model->Mesh[Model->NumMesh-1].Face=(uint32_t *)Zone_Malloc(Zone, sizeof(uint32_t)*3*Model->Mesh[Model->NumMesh-1].NumFace);

			if(Model->Mesh[Model->NumMesh-1].Face==NULL)
				return false;

			Model->Mesh[Model->NumMesh-1].NumFace=0;
		}
		else if(strncmp(buff, "mtllib ", 7)==0)
		{
			if(sscanf(buff, "mtllib %s", Model->MaterialFilename)!=1)
				return false;
		}
		else if(strncmp(buff, "v ", 2)==0)
		{
			Model->NumVertex++;

			if(sscanf(buff, "v %f %f %f",
				&Model->Vertex[3*(Model->NumVertex-1)+0],
				&Model->Vertex[3*(Model->NumVertex-1)+1],
				&Model->Vertex[3*(Model->NumVertex-1)+2])!=3)
				return false;
		}
		else if(strncmp(buff, "vt ", 3)==0)
		{
			NumUV++;

			if(sscanf(buff, "vt %f %f",
				&Model->UV[2*(NumUV-1)+0],
				&Model->UV[2*(NumUV-1)+1])!=2)
				return false;
		}
		else if(strncmp(buff, "f ", 2)==0)
		{
			if(Model->Mesh&&Model->Mesh[Model->NumMesh-1].Face)
			{
				Model->Mesh[Model->NumMesh-1].NumFace++;

				// So... Alias Wavefront models allow a different index buffer for vertices, normals, and UV
				// But, I'm ignoring that and only using the vertex indices for everything and hope that
				// whatever exported the model used the same index for all attribs.
				//
				// Maybe add in a index unifier layer?
				//
				if(sscanf(buff, "f %d/%d/%d %d/%d/%d %d/%d/%d",
					&vi[0], &ti[0], &ni[0],
					&vi[1], &ti[1],	&ni[1],
					&vi[2], &ti[2], &ni[2])!=9)
				{
					if(sscanf(buff, "f %d/%d %d/%d %d/%d",
						&vi[0], &ti[0],
						&vi[1], &ti[1],
						&vi[2], &ti[2])!=6)
					{
						if(sscanf(buff, "f %d//%d %d//%d %d//%d",
							&vi[0], &ni[0],
							&vi[1], &ni[1],
							&vi[2], &ni[2])!=6)
						{
							if(sscanf(buff, "f %d %d %d",
								&vi[0],
								&vi[1],
								&vi[2])!=3)
								return false;
						}
					}
				}

				// Alias Wavefront models have face indices that start on 1, not 0
				Model->Mesh[Model->NumMesh-1].Face[3*(Model->Mesh[Model->NumMesh-1].NumFace-1)+0]=vi[0]-1;
				Model->Mesh[Model->NumMesh-1].Face[3*(Model->Mesh[Model->NumMesh-1].NumFace-1)+1]=vi[1]-1;
				Model->Mesh[Model->NumMesh-1].Face[3*(Model->Mesh[Model->NumMesh-1].NumFace-1)+2]=vi[2]-1;
			}
		}
	}

	fclose(fp);

	CalculateTangentOBJ(Model);

	char nameNoExt[256], *ptr;

	strncpy(nameNoExt, Filename, 256);
	ptr=strrstr(nameNoExt, ".");

	if(ptr==NULL)
		return false;

	ptr[0]='\0';

	strcat(nameNoExt, ".mtl");
	LoadMTL(Model, nameNoExt);

	return true;
}

// Free memory allocated for the model
void FreeOBJ(ModelOBJ_t *Model)
{
	Zone_Free(Zone, Model->Vertex);
	Zone_Free(Zone, Model->UV);
	Zone_Free(Zone, Model->Normal);
	Zone_Free(Zone, Model->Tangent);
	Zone_Free(Zone, Model->Binormal);

	if(Model->NumMesh)
	{
		/* Free mesh data */
		for(uint32_t i=0;i<Model->NumMesh;i++)
			Zone_Free(Zone, Model->Mesh[i].Face);

		Zone_Free(Zone, Model->Mesh);
	}

	Zone_Free(Zone, Model->Material);
}

void BuildMemoryBuffersOBJ(VkuContext_t *Context, ModelOBJ_t *Model)
{
	VkuBuffer_t stagingBuffer;
	void *Data=NULL;

	// Vertex data on device memory
	vkuCreateGPUBuffer(Context, &Model->VertexBuffer, sizeof(float)*20*Model->NumVertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Create staging buffer to transfer from host memory to device memory
	vkuCreateHostBuffer(Context, &stagingBuffer, sizeof(float)*20*Model->NumVertex, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	vkMapMemory(Context->Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	if(!Data)
		return;

	float *fPtr=Data;

	for(uint32_t j=0;j<Model->NumVertex;j++)
	{
		*fPtr++=Model->Vertex[3*j+0];
		*fPtr++=Model->Vertex[3*j+1];
		*fPtr++=Model->Vertex[3*j+2];
		*fPtr++=1.0f;

		*fPtr++=Model->UV[2*j+0];
		*fPtr++=1.0f-Model->UV[2*j+1];
		*fPtr++=0.0f;
		*fPtr++=0.0f;

		*fPtr++=Model->Tangent[3*j+0];
		*fPtr++=Model->Tangent[3*j+1];
		*fPtr++=Model->Tangent[3*j+2];
		*fPtr++=0.0f;

		*fPtr++=Model->Binormal[3*j+0];
		*fPtr++=Model->Binormal[3*j+1];
		*fPtr++=Model->Binormal[3*j+2];
		*fPtr++=0.0f;

		*fPtr++=Model->Normal[3*j+0];
		*fPtr++=Model->Normal[3*j+1];
		*fPtr++=Model->Normal[3*j+2];
		*fPtr++=0.0f;
	}

	vkUnmapMemory(Context->Device, stagingBuffer.DeviceMemory);

	// Copy to device memory
	vkuCopyBuffer(Context, stagingBuffer.Buffer, Model->VertexBuffer.Buffer, sizeof(float)*20*Model->NumVertex);

	// Delete staging data
	vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);

	for(uint32_t i=0;i<Model->NumMesh;i++)
	{
		// Index data
		vkuCreateGPUBuffer(Context, &Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		// Staging buffer
		vkuCreateHostBuffer(Context, &stagingBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		vkMapMemory(Context->Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(!Data)
			return;

		uint16_t *sPtr=Data;

		for(uint32_t j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*sPtr++=Model->Mesh[i].Face[3*j+0];
			*sPtr++=Model->Mesh[i].Face[3*j+1];
			*sPtr++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(Context->Device, stagingBuffer.DeviceMemory);

		vkuCopyBuffer(Context, stagingBuffer.Buffer, Model->Mesh[i].IndexBuffer.Buffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
	}
}
