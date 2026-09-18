#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../model/banim.h"
#include "../utils/pipeline.h"
#include "../assetmanager.h"
#include "../entitylist.h"
#include "../perframe.h"
#include "shadow.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkRenderPass renderPass;

static Pipeline_t testPipeline;

#define MAX_BONE 128
static VkuBuffer_t animatedWorldBuffer;
static matrix animatedWorld[MAX_BONE];

bool CreateTestPipeline(void)
{
	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &testPipeline, renderPass, "pipelines/test.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	vkuCreateHostBuffer(&vkContext, &animatedWorldBuffer, sizeof(matrix)*MAX_BONE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

	return true;
}

void DestroyTest(void)
{
	DestroyPipeline(&vkContext, &testPipeline);
}

extern BAnim_t testAnim;
extern float fTimeStep;

static uint32_t frameCount=0;
static float frameTime=0.0f;

void DrawTest(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	BModel_t *model=&AssetManager_GetAsset(assets, MODEL_TEST)->model;

	frameTime+=fTimeStep;

	if(frameTime>=(1.0f/testAnim.frameRate))
	{
		frameTime=0.0f;
		frameCount++;
	}

	if(frameCount>=testAnim.numFrame)
		frameCount=0;

	const BAnim_BoneFrame_t *animframe=&testAnim.frame[frameCount*testAnim.numBone];

	for(uint32_t i=0;i<model->numBone;i++)
	{
		matrix local=MatrixMult(QuatToMatrix(animframe[i].orientation), MatrixTranslatev(animframe[i].position));

		if(model->bone[i].parent>=0)
			local=MatrixMult(local, animatedWorld[model->bone[i].parent]);

		animatedWorld[i]=local;
	}
 
	memcpy(animatedWorldBuffer.memory->mappedPointer, animatedWorld, sizeof(matrix)*model->numBone);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, testPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&testPipeline.descriptorSet, 0, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&testPipeline.descriptorSet, 1, model->boneBuffer.buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&testPipeline.descriptorSet, 2, animatedWorldBuffer.buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&testPipeline.descriptorSet, descriptorPool);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, testPipeline.pipelineLayout, 0, 1, &testPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model->vertexBuffer.buffer, &(VkDeviceSize){0});

	for(uint32_t m=0;m<model->numMesh;m++)
	{
		vkCmdBindIndexBuffer(commandBuffer, model->mesh[m].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, model->mesh[m].numFace*3, 1, 0, 0, 0);
	}
}
