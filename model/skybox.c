#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../system/system.h"
#include "3ds.h"

extern VkuContext_t Context;

void BuildSkybox(Model3DS_t *Model)
{
	float scale=1.0f, w=0.0f, tex_scale=1.0f;
	vec4 SkyboxVerts[]=
	{
		{ +scale, -scale, -scale, w }, { +tex_scale, -tex_scale, -tex_scale, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, //0 Right
		{ +scale, -scale, +scale, w }, { +tex_scale, -tex_scale, +tex_scale, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, //1
		{ +scale, +scale, +scale, w }, { +tex_scale, +tex_scale, +tex_scale, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, //2
		{ +scale, +scale, -scale, w }, { +tex_scale, +tex_scale, -tex_scale, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, //3

		{ -scale, -scale, +scale, w }, { -tex_scale, -tex_scale, +tex_scale, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, //4 Left
		{ -scale, -scale, -scale, w }, { -tex_scale, -tex_scale, -tex_scale, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, //5
		{ -scale, +scale, -scale, w }, { -tex_scale, +tex_scale, -tex_scale, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, //6
		{ -scale, +scale, +scale, w }, { -tex_scale, +tex_scale, +tex_scale, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, //7

		{ -scale, +scale, -scale, w }, { -tex_scale, +tex_scale, -tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f,-1.0f, 0.0f, 0.0f }, //8 Top
		{ +scale, +scale, -scale, w }, { +tex_scale, +tex_scale, -tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f,-1.0f, 0.0f, 0.0f }, //9
		{ +scale, +scale, +scale, w }, { +tex_scale, +tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f,-1.0f, 0.0f, 0.0f }, //10
		{ -scale, +scale, +scale, w }, { -tex_scale, +tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, { 0.0f,-1.0f, 0.0f, 0.0f }, //11

		{ -scale, -scale, +scale, w }, { -tex_scale, -tex_scale, +tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, //12 Bottom
		{ +scale, -scale, +scale, w }, { +tex_scale, -tex_scale, +tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, //13
		{ +scale, -scale, -scale, w }, { +tex_scale, -tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, //14
		{ -scale, -scale, -scale, w }, { -tex_scale, -tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, //15

		{ +scale, -scale, +scale, w }, { +tex_scale, -tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, //16 Front
		{ -scale, -scale, +scale, w }, { -tex_scale, -tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, //17
		{ -scale, +scale, +scale, w }, { -tex_scale, +tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, //18
		{ +scale, +scale, +scale, w }, { +tex_scale, +tex_scale, +tex_scale, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f,-1.0f, 0.0f }, //19

		{ -scale, -scale, -scale, w }, { -tex_scale, -tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, //20 Back
		{ +scale, -scale, -scale, w }, { +tex_scale, -tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, //21
		{ +scale, +scale, -scale, w }, { +tex_scale, +tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }, //22
		{ -scale, +scale, -scale, w }, { -tex_scale, +tex_scale, -tex_scale, 0.0f }, {-1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f }  //23
	};
	uint16_t SkyboxTris[]=
	{
		 0,  1,  2,  3,  0, 2,	// Right
		 4,  5,  6,  7,  4, 6,	// Left
		 8,  9, 10, 11,  8, 10,	// Top
		12, 13, 14, 15, 12, 14,	// Bottom
		16, 17, 18, 19, 16, 18, // Front
		20, 21, 22, 23, 20, 22	// Back
	};

	Model->NumMesh=1;
	Model->Mesh=(Mesh3DS_t *)Zone_Malloc(Zone, sizeof(Mesh3DS_t));

	Model->Mesh->NumVertex=24;
	Model->Mesh->NumFace=12;

	VkuBuffer_t stagingBuffer;
	void *Data=NULL;

	// Vertex data on device memory
	vkuCreateGPUBuffer(&Context, &Model->Mesh->VertexBuffer, sizeof(float)*20*Model->Mesh->NumVertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Create staging buffer to transfer from host memory to device memory
	vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(float)*20*Model->Mesh->NumVertex, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	if(!Data)
		return;

	float *fPtr=Data;

	for(int32_t j=0;j<Model->Mesh->NumVertex;j++)
	{
		*fPtr++=SkyboxVerts[5*j+0][0];
		*fPtr++=SkyboxVerts[5*j+0][1];
		*fPtr++=SkyboxVerts[5*j+0][2];
		*fPtr++=SkyboxVerts[5*j+0][3];

		*fPtr++=SkyboxVerts[5*j+1][0];
		*fPtr++=SkyboxVerts[5*j+1][1];
		*fPtr++=SkyboxVerts[5*j+1][2];
		*fPtr++=0.0f;

		*fPtr++=SkyboxVerts[5*j+2][0];
		*fPtr++=SkyboxVerts[5*j+2][1];
		*fPtr++=SkyboxVerts[5*j+2][2];
		*fPtr++=0.0f;

		*fPtr++=SkyboxVerts[5*j+3][0];
		*fPtr++=SkyboxVerts[5*j+3][1];
		*fPtr++=SkyboxVerts[5*j+3][2];
		*fPtr++=0.0f;

		*fPtr++=SkyboxVerts[5*j+4][0];
		*fPtr++=SkyboxVerts[5*j+4][1];
		*fPtr++=SkyboxVerts[5*j+4][2];
		*fPtr++=0.0f;
	}

	vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

	// Copy to device memory
	vkuCopyBuffer(&Context, stagingBuffer.Buffer, Model->Mesh->VertexBuffer.Buffer, sizeof(float)*20*Model->Mesh->NumVertex);

	// Delete staging data
	vkFreeMemory(Context.Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, stagingBuffer.Buffer, VK_NULL_HANDLE);

	// Index data
	vkuCreateGPUBuffer(&Context, &Model->Mesh->IndexBuffer, sizeof(uint16_t)*Model->Mesh->NumFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Staging buffer
	vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(uint16_t)*Model->Mesh->NumFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	if(!Data)
		return;

	uint16_t *sPtr=Data;

	for(int32_t j=0;j<Model->Mesh->NumFace;j++)
	{
		*sPtr++=SkyboxTris[3*j+0];
		*sPtr++=SkyboxTris[3*j+1];
		*sPtr++=SkyboxTris[3*j+2];
	}

	vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

	vkuCopyBuffer(&Context, stagingBuffer.Buffer, Model->Mesh->IndexBuffer.Buffer, sizeof(uint16_t)*Model->Mesh->NumFace*3);

	// Delete staging data
	vkFreeMemory(Context.Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
}
