// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

// Add a binding to the descriptor set layout
VkBool32 vkuDescriptorSet_AddBinding(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage)
{
	if(!descriptorSet)
		return VK_FALSE;

	if(binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding=
	{
		.binding=binding,
		.descriptorType=type,
		.descriptorCount=1,
		.stageFlags=stage,
		.pImmutableSamplers=VK_NULL_HANDLE
	};

	descriptorSet->bindings[binding]=DescriptorSetLayoutBinding;

	VkWriteDescriptorSet writeDescriptorSet=
	{
		.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding=binding,
		.descriptorCount=1,
		.descriptorType=descriptorSet->bindings[binding].descriptorType,
		.pBufferInfo=&descriptorSet->bufferInfo[binding],
		.pImageInfo=&descriptorSet->imageInfo[binding],
	};

	descriptorSet->writeDescriptorSet[binding]=writeDescriptorSet;

	descriptorSet->numBindings++;

	return VK_TRUE;
}

VkBool32 vkuDescriptorSet_UpdateBindingImageInfo(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
	if(!descriptorSet)
		return VK_FALSE;

	if(binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	descriptorSet->imageInfo[binding]=(VkDescriptorImageInfo) { sampler, imageView, imageLayout };

	return VK_TRUE;
}

VkBool32 vkuDescriptorSet_UpdateBindingBufferInfo(VkuDescriptorSet_t *descriptorSet, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	if(!descriptorSet)
		return VK_FALSE;

	if(binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	descriptorSet->bufferInfo[binding]=(VkDescriptorBufferInfo){ buffer, offset, range };

	return VK_TRUE;
}

// Initialize the descriptor set layout structures
VkBool32 vkuInitDescriptorSet(VkuDescriptorSet_t *descriptorSet, VkDevice device)
{
	if(!descriptorSet)
		return VK_FALSE;

	descriptorSet->device=device;

	descriptorSet->numBindings=0;

	memset(descriptorSet->bindings, 0, sizeof(VkDescriptorSetLayout)*VKU_MAX_DESCRIPTORSET_BINDINGS);
	memset(descriptorSet->writeDescriptorSet, 0, sizeof(VkWriteDescriptorSet)*VKU_MAX_DESCRIPTORSET_BINDINGS);

	descriptorSet->descriptorSet=VK_NULL_HANDLE;

	return VK_TRUE;
}

// Creates the descriptor set layout
VkBool32 vkuAssembleDescriptorSetLayout(VkuDescriptorSet_t *descriptorSet)
{
	if(!descriptorSet)
		return VK_FALSE;

	VkDescriptorSetLayoutCreateInfo LayoutCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount=descriptorSet->numBindings,
		.pBindings=descriptorSet->bindings,
	};

	if(vkCreateDescriptorSetLayout(descriptorSet->device, &LayoutCreateInfo, NULL, &descriptorSet->descriptorSetLayout)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

VkBool32 vkuAllocateUpdateDescriptorSet(VkuDescriptorSet_t *descriptorSet, VkDescriptorPool descriptorPool)
{
	VkDescriptorSetAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool=descriptorPool,
		.descriptorSetCount=1,
		.pSetLayouts=&descriptorSet->descriptorSetLayout,
	};

	//if(descriptorSet->descriptorSet==VK_NULL_HANDLE)
		vkAllocateDescriptorSets(descriptorSet->device, &AllocateInfo, &descriptorSet->descriptorSet);

	// Need to update destination set handle now that one has been allocated.
	for(uint32_t i=0;i<descriptorSet->numBindings;i++)
		descriptorSet->writeDescriptorSet[i].dstSet=descriptorSet->descriptorSet;

	vkUpdateDescriptorSets(descriptorSet->device, descriptorSet->numBindings, descriptorSet->writeDescriptorSet, 0, VK_NULL_HANDLE);

	return VK_TRUE;
}
