#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../system/system.h"
#include "3ds.h"

void BuildSkybox(VkuContext_t *Context, VkuBuffer_t *VertexBuffer, VkuBuffer_t *IndexBuffer)
{
	float vertices[3*12];

	float phiaa=26.56505f, r=1.0f;
	float phia=PI*phiaa/180.0f;
	float theb=PI*36.0f/180.0f;
	float the72=PI*72.0f/180.0f;

	vertices[3*0+0]=0.0f;
	vertices[3*0+1]=0.0f;
	vertices[3*0+2]=r;

	vertices[3*11+0]=0.0f;
	vertices[3*11+1]=0.0f;
	vertices[3*11+2]=-r;

	float the=0.0f;
	for(uint32_t i=1;i<6;i++)
	{
		vertices[3*i+0]=r*cosf(the)*cosf(phia);
		vertices[3*i+1]=r*sinf(the)*cosf(phia);
		vertices[3*i+2]=r*sinf(phia);
		the=the+the72;
	}

	the=theb;
	for(uint32_t i=6;i<11;i++)
	{
		vertices[3*i+0]=r*cosf(the)*cosf(-phia);
		vertices[3*i+1]=r*sinf(the)*cosf(-phia);
		vertices[3*i+2]=r*sinf(-phia);
		the=the+the72;
	}

	uint16_t faces[3*20]=
	{
		0, 2, 1,
		0, 3, 2,
		0, 4, 3,
		0, 5, 4,
		0, 1, 5,
		11, 6, 7,
		11, 7, 8,
		11, 8, 9,
		11, 9, 10,
		11, 10, 6,
		1, 2, 6,
		2, 3, 7,
		3, 4, 8,
		4, 5, 9,
		5, 1, 10,
		6, 2, 7,
		7, 3, 8,
		8, 4, 9,
		9, 5, 10,
		10, 1, 6
	};

	uint32_t NumVertex=12;
	uint32_t NumFace=20;

	VkuBuffer_t stagingBuffer;
	void *Data=NULL;

	// Vertex data on device memory
	vkuCreateGPUBuffer(Context, VertexBuffer, sizeof(float)*4*NumVertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Create staging buffer to transfer from host memory to device memory
	vkuCreateHostBuffer(Context, &stagingBuffer, sizeof(float)*4*NumVertex, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	vkMapMemory(Context->Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	if(!Data)
		return;

	float *fPtr=Data;

	for(uint32_t j=0;j<NumVertex;j++)
	{
		*fPtr++=vertices[3*j+0];
		*fPtr++=vertices[3*j+1];
		*fPtr++=vertices[3*j+2];
		*fPtr++=0.0f;
	}

	vkUnmapMemory(Context->Device, stagingBuffer.DeviceMemory);

	// Copy to device memory
	vkuCopyBuffer(Context, stagingBuffer.Buffer, VertexBuffer->Buffer, sizeof(float)*4*NumVertex);

	// Delete staging data
	vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);

	// Index data
	vkuCreateGPUBuffer(Context, IndexBuffer, sizeof(uint16_t)*NumFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

	// Staging buffer
	vkuCreateHostBuffer(Context, &stagingBuffer, sizeof(uint16_t)*NumFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	vkMapMemory(Context->Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	if(!Data)
		return;

	uint16_t *sPtr=Data;

	for(uint32_t j=0;j<NumFace;j++)
	{
		*sPtr++=faces[3*j+0];
		*sPtr++=faces[3*j+1];
		*sPtr++=faces[3*j+2];
	}

	vkUnmapMemory(Context->Device, stagingBuffer.DeviceMemory);

	vkuCopyBuffer(Context, stagingBuffer.Buffer, IndexBuffer->Buffer, sizeof(uint16_t)*NumFace*3);

	// Delete staging data
	vkFreeMemory(Context->Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context->Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
}
