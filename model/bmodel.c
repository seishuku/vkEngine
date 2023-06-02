#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "bmodel.h"

static void CalculateTangent(BModel_t *Model)
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

				v0.x=Model->Vertex[3*i2+0]-Model->Vertex[3*i1+0];
				v0.y=Model->Vertex[3*i2+1]-Model->Vertex[3*i1+1];
				v0.z=Model->Vertex[3*i2+2]-Model->Vertex[3*i1+2];

				uv0.x=Model->UV[2*i2+0]-Model->UV[2*i1+0];
				uv0.y=Model->UV[2*i2+1]-Model->UV[2*i1+1];

				v1.x=Model->Vertex[3*i3+0]-Model->Vertex[3*i1+0];
				v1.y=Model->Vertex[3*i3+1]-Model->Vertex[3*i1+1];
				v1.z=Model->Vertex[3*i3+2]-Model->Vertex[3*i1+2];

				uv1.x=Model->UV[2*i3+0]-Model->UV[2*i1+0];
				uv1.y=Model->UV[2*i3+1]-Model->UV[2*i1+1];

				r=1.0f/(uv0.x*uv1.y-uv1.x*uv0.y);

				s.x=(uv1.y*v0.x-uv0.y*v1.x)*r;
				s.y=(uv1.y*v0.y-uv0.y*v1.y)*r;
				s.z=(uv1.y*v0.z-uv0.y*v1.z)*r;
				Vec3_Normalize(&s);

				Model->Tangent[3*i1+0]+=s.x;	Model->Tangent[3*i1+1]+=s.y;	Model->Tangent[3*i1+2]+=s.z;
				Model->Tangent[3*i2+0]+=s.x;	Model->Tangent[3*i2+1]+=s.y;	Model->Tangent[3*i2+2]+=s.z;
				Model->Tangent[3*i3+0]+=s.x;	Model->Tangent[3*i3+1]+=s.y;	Model->Tangent[3*i3+2]+=s.z;

				t.x=(uv0.x*v1.x-uv1.x*v0.x)*r;
				t.y=(uv0.x*v1.y-uv1.x*v0.y)*r;
				t.z=(uv0.x*v1.z-uv1.x*v0.z)*r;
				Vec3_Normalize(&t);

				Model->Binormal[3*i1+0]+=t.x;	Model->Binormal[3*i1+1]+=t.y;	Model->Binormal[3*i1+2]+=t.z;
				Model->Binormal[3*i2+0]+=t.x;	Model->Binormal[3*i2+1]+=t.y;	Model->Binormal[3*i2+2]+=t.z;
				Model->Binormal[3*i3+0]+=t.x;	Model->Binormal[3*i3+1]+=t.y;	Model->Binormal[3*i3+2]+=t.z;

				n=Vec3_Cross(v0, v1);
				Vec3_Normalize(&n);

				Model->Normal[3*i1+0]+=n.x;	Model->Normal[3*i1+1]+=n.y;	Model->Normal[3*i1+2]+=n.z;
				Model->Normal[3*i2+0]+=n.x;	Model->Normal[3*i2+1]+=n.y;	Model->Normal[3*i2+2]+=n.z;
				Model->Normal[3*i3+0]+=n.x;	Model->Normal[3*i3+1]+=n.y;	Model->Normal[3*i3+2]+=n.z;
			}
		}

		for(uint32_t i=0;i<Model->NumVertex;i++)
		{
			vec3 t, b, n;

			t=Vec3_Set(Model->Tangent[3*i+0], Model->Tangent[3*i+1], Model->Tangent[3*i+2]);
			b=Vec3_Set(Model->Binormal[3*i+0], Model->Binormal[3*i+1], Model->Binormal[3*i+2]);
			n=Vec3_Set(Model->Normal[3*i+0], Model->Normal[3*i+1], Model->Normal[3*i+2]);

			t=Vec3_Subv(t, Vec3_Muls(n, Vec3_Dot(n, t)));

			Vec3_Normalize(&t);
			Vec3_Normalize(&b);
			Vec3_Normalize(&n);

			vec3 NxT=Vec3_Cross(n, t);

			if(Vec3_Dot(NxT, b)<0.0f)
				t=Vec3_Muls(t, -1.0f);

			b=Vec3_Setv(NxT);

			memcpy(&Model->Tangent[3*i], &t, sizeof(float)*3);
			memcpy(&Model->Binormal[3*i], &b, sizeof(float)*3);
			memcpy(&Model->Normal[3*i], &n, sizeof(float)*3);
		}
	}
}

static void ReadString(char *String, size_t StringLength, FILE *fp)
{
	uint32_t i=0;
	char ch=127;

	while(ch!='\0'&&i<StringLength)
	{
		ch=fgetc(fp);
		String[i++]=ch;
	}
}

bool LoadBModel(BModel_t *Model, const char *Filename)
{
	FILE *fp;

	if(!(fp=fopen(Filename, "rb")))
		return false;

	uint32_t Magic=0;
	fread(&Magic, sizeof(uint32_t), 1, fp);

	if(Magic!=BMDL_MAGIC)
		return false;

	memset(Model, 0, sizeof(BModel_t));

	/////// Read in materials

	// Get number of materials
	fread(&Model->NumMaterial, sizeof(uint32_t), 1, fp);

	// If there materials, allocate memory for them
	if(Model->NumMaterial)
	{
		Model->Material=(BModel_Material_t *)Zone_Malloc(Zone, sizeof(BModel_Material_t)*Model->NumMaterial);

		if(Model->Material==NULL)
			return false;
	}

	// Read them in
	for(uint32_t i=0;i<Model->NumMaterial;i++)
	{
		uint32_t Magic=0;
		fread(&Magic, sizeof(uint32_t), 1, fp);

		if(Magic!=MATL_MAGIC)
			return false;

		ReadString(Model->Material[i].Name, 256, fp);
		fread(&Model->Material[i].Ambient, sizeof(float), 3, fp);
		fread(&Model->Material[i].Diffuse, sizeof(float), 3, fp);
		fread(&Model->Material[i].Specular, sizeof(float), 3, fp);
		fread(&Model->Material[i].Emission, sizeof(float), 3, fp);
		fread(&Model->Material[i].Shininess, sizeof(float), 1, fp);
		ReadString(Model->Material[i].Texture, 256, fp);
	}
	///////

	/////// Read in meshes

	// Get number of meshes
	fread(&Model->NumMesh, sizeof(uint32_t), 1, fp);

	// If there meshes, allocate memory for them
	if(Model->NumMesh)
	{
		Model->Mesh=(BModel_Mesh_t *)Zone_Malloc(Zone, sizeof(BModel_Mesh_t)*Model->NumMesh);

		if(Model->Mesh==NULL)
			return false;
	}

	// Read them in
	for(uint32_t i=0;i<Model->NumMesh;i++)
	{
		uint32_t Magic=0;
		fread(&Magic, sizeof(uint32_t), 1, fp);

		if(Magic!=MESH_MAGIC)
			return false;

		ReadString(Model->Mesh[i].Name, 256, fp);
		ReadString(Model->Mesh[i].MaterialName, 256, fp);

		fread(&Model->Mesh[i].NumFace, sizeof(uint32_t), 1, fp);

		if(Model->Mesh[i].NumFace)
		{
			Model->Mesh[i].Face=(uint32_t *)Zone_Malloc(Zone, sizeof(uint32_t)*Model->Mesh[i].NumFace*3);

			if(Model->Mesh[i].Face==NULL)
				return false;

			fread(Model->Mesh[i].Face, sizeof(uint32_t), Model->Mesh[i].NumFace*3, fp);
		}
	}

	// Read in number of vertices
	fread(&Model->NumVertex, sizeof(uint32_t), 1, fp);

	// If there are vertices
	if(Model->NumVertex)
	{
		// Read out until end of file
		while(!feof(fp))
		{
			uint32_t Magic=0;

			// Read magic ID for chunk
			if(!fread(&Magic, sizeof(uint32_t), 1, fp))
				break;

			switch(Magic)
			{
				case VERT_MAGIC:
					Model->Vertex=(float *)Zone_Malloc(Zone, sizeof(float)*Model->NumVertex*3);

					if(Model->Vertex==NULL)
						return false;

					if(fread(Model->Vertex, sizeof(float)*3, Model->NumVertex, fp)!=Model->NumVertex)
					{
						Zone_Free(Zone, Model->Vertex);
						Model->Vertex=NULL;
					}
					break;

				case TEXC_MAGIC:
					Model->UV=(float *)Zone_Malloc(Zone, sizeof(float)*Model->NumVertex*2);

					if(Model->UV==NULL)
						return false;

					if(fread(Model->UV, sizeof(float)*2, Model->NumVertex, fp)!=Model->NumVertex)
					{
						Zone_Free(Zone, Model->UV);
						Model->UV=NULL;
					}
					break;

				case TANG_MAGIC:
					Model->Tangent=(float *)Zone_Malloc(Zone, sizeof(float)*Model->NumVertex*3);

					if(Model->Tangent==NULL)
						return false;

					if(fread(Model->Tangent, sizeof(float)*3, Model->NumVertex, fp)!=Model->NumVertex)
					{
						Zone_Free(Zone, Model->Tangent);
						Model->Tangent=NULL;
					}
					break;

				case BNRM_MAGIC:
					Model->Binormal=(float *)Zone_Malloc(Zone, sizeof(float)*Model->NumVertex*3);

					if(Model->Binormal==NULL)
						return false;

					if(fread(Model->Binormal, sizeof(float)*3, Model->NumVertex, fp)!=Model->NumVertex)
					{
						Zone_Free(Zone, Model->Binormal);
						Model->Binormal=NULL;
					}
					break;

				case NORM_MAGIC:
					Model->Normal=(float *)Zone_Malloc(Zone, sizeof(float)*Model->NumVertex*3);

					if(Model->Normal==NULL)
						return false;

					if(fread(Model->Normal, sizeof(float)*3, Model->NumVertex, fp)!=Model->NumVertex)
					{
						Zone_Free(Zone, Model->Normal);
						Model->Normal=NULL;
					}
					break;

				default:
					break;
			}
		}
	}

	if(!Model->Tangent&&!Model->Binormal&&!Model->Normal&&Model->UV)
		CalculateTangent(Model);

	// Match up material names with their meshes with an index number
	if(Model->NumMaterial)
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

// Free memory allocated for the model
void FreeBModel(BModel_t *Model)
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

void BuildMemoryBuffersBModel(VkuContext_t *Context, BModel_t *Model)
{
	VkCommandBuffer CopyCommand=VK_NULL_HANDLE;
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
	CopyCommand=vkuOneShotCommandBufferBegin(Context);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, Model->VertexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(float)*20*Model->NumVertex });
	vkuOneShotCommandBufferEnd(Context, CopyCommand);

	// Delete staging data
	vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
	vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);

	for(uint32_t i=0;i<Model->NumMesh;i++)
	{
		// Index data
		vkuCreateGPUBuffer(Context, &Model->Mesh[i].IndexBuffer, sizeof(uint32_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		// Staging buffer
		vkuCreateHostBuffer(Context, &stagingBuffer, sizeof(uint32_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		vkMapMemory(Context->Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(!Data)
			return;

		uint32_t *iPtr=Data;

		for(uint32_t j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*iPtr++=Model->Mesh[i].Face[3*j+0];
			*iPtr++=Model->Mesh[i].Face[3*j+1];
			*iPtr++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(Context->Device, stagingBuffer.DeviceMemory);

		CopyCommand=vkuOneShotCommandBufferBegin(Context);
		vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, Model->Mesh[i].IndexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(uint32_t)*Model->Mesh[i].NumFace*3 });
		vkuOneShotCommandBufferEnd(Context, CopyCommand);

		// Delete staging data
		vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
		vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	}
}
