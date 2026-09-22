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

static BAnim_t testAnim;

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

	if(!LoadBAnim(&testAnim, "assets/test.banim"))
		return false;

	return true;
}

void DestroyTest(void)
{
	FreeBAnim(&testAnim);
	vkuDestroyBuffer(&vkContext, &animatedWorldBuffer);

	DestroyPipeline(&vkContext, &testPipeline);
}

extern float fTimeStep;

static uint32_t frameCount=0;
static float frameTime=0.0f;

void DrawTest(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	BModel_t *model=&AssetManager_GetAsset(assets, MODEL_TEST)->model;

	frameTime+=fTimeStep;

	while(frameTime>=(1.0f/testAnim.frameRate))
	{
		frameTime-=(1.0f/testAnim.frameRate);
		frameCount++;

		if(frameCount>=testAnim.numFrame)
			frameCount=0;
	}

	const float t=frameTime*testAnim.frameRate;
	const uint32_t nextFrame=(frameCount+1>=testAnim.numFrame)?0:frameCount+1;

	const BAnim_BoneFrame_t *animframeA=&testAnim.frame[frameCount*testAnim.numBone];
	const BAnim_BoneFrame_t *animframeB=&testAnim.frame[nextFrame*testAnim.numBone];

	for(uint32_t i=0;i<model->numBone;i++)
	{
		vec4 orientation=QuatSlerp(animframeA[i].orientation, animframeB[i].orientation, t);
		vec3 position=Vec3_Lerp(animframeA[i].position, animframeB[i].position, t);

		matrix local=MatrixMult(QuatToMatrix(orientation), MatrixTranslatev(position));

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
