// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

// Add a binding to the descriptor set layout
VkBool32 vkuDescriptorSet_AddBinding(VkuDescriptorSet_t *DescriptorSet, uint32_t Binding, VkDescriptorType Type, VkShaderStageFlags Stage)
{
	if(!DescriptorSet)
		return VK_FALSE;

	if(Binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding=
	{
		.binding=Binding,
		.descriptorType=Type,
		.descriptorCount=1,
		.stageFlags=Stage,
		.pImmutableSamplers=VK_NULL_HANDLE
	};

	DescriptorSet->Bindings[Binding]=DescriptorSetLayoutBinding;

	VkWriteDescriptorSet WriteDescriptorSet=
	{
		.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstBinding=Binding,
		.descriptorCount=1,
		.descriptorType=DescriptorSet->Bindings[Binding].descriptorType,
		.pBufferInfo=&DescriptorSet->BufferInfo[Binding],
		.pImageInfo=&DescriptorSet->ImageInfo[Binding],
	};

	DescriptorSet->WriteDescriptorSet[Binding]=WriteDescriptorSet;

	DescriptorSet->NumBindings++;

	return VK_TRUE;
}

VkBool32 vkuDescriptorSet_UpdateBindingImageInfo(VkuDescriptorSet_t *DescriptorSet, uint32_t Binding, VkuImage_t *Image)
{
	if(!DescriptorSet||!Image)
		return VK_FALSE;

	if(Binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	DescriptorSet->ImageInfo[Binding]=(VkDescriptorImageInfo) { Image->Sampler, Image->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

	return VK_TRUE;
}

VkBool32 vkuDescriptorSet_UpdateBindingBufferInfo(VkuDescriptorSet_t *DescriptorSet, uint32_t Binding, VkBuffer Buffer, VkDeviceSize Offset, VkDeviceSize Range)
{
	if(!DescriptorSet)
		return VK_FALSE;

	if(Binding>=VKU_MAX_DESCRIPTORSET_BINDINGS)
		return VK_FALSE;

	DescriptorSet->BufferInfo[Binding]=(VkDescriptorBufferInfo){ Buffer, Offset, Range };

	return VK_TRUE;
}

// Initialize the descriptor set layout structures
VkBool32 vkuInitDescriptorSet(VkuDescriptorSet_t *DescriptorSet, VkuContext_t *Context)
{
	if(!DescriptorSet||!Context)
		return VK_FALSE;

	DescriptorSet->Device=Context->Device;

	DescriptorSet->NumBindings=0;

	memset(DescriptorSet->Bindings, 0, sizeof(VkDescriptorSetLayout)*VKU_MAX_DESCRIPTORSET_BINDINGS);
	memset(DescriptorSet->WriteDescriptorSet, 0, sizeof(VkWriteDescriptorSet)*VKU_MAX_DESCRIPTORSET_BINDINGS);

	DescriptorSet->DescriptorSet=VK_NULL_HANDLE;

	return VK_TRUE;
}

// Creates the descriptor set layout
VkBool32 vkuAssembleDescriptorSetLayout(VkuDescriptorSet_t *DescriptorSet)
{
	if(!DescriptorSet)
		return VK_FALSE;

	VkDescriptorSetLayoutCreateInfo LayoutCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount=DescriptorSet->NumBindings,
		.pBindings=DescriptorSet->Bindings,
	};

	if(vkCreateDescriptorSetLayout(DescriptorSet->Device, &LayoutCreateInfo, NULL, &DescriptorSet->DescriptorSetLayout)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

VkBool32 vkuAllocateUpdateDescriptorSet(VkuDescriptorSet_t *DescriptorSet, VkDescriptorPool DescriptorPool)
{
	VkDescriptorSetAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool=DescriptorPool,
		.descriptorSetCount=1,
		.pSetLayouts=&DescriptorSet->DescriptorSetLayout,
	};

	//if(DescriptorSet->DescriptorSet==VK_NULL_HANDLE)
		vkAllocateDescriptorSets(DescriptorSet->Device, &AllocateInfo, &DescriptorSet->DescriptorSet);

	// Need to update destination set handle now that one has been allocated.
	for(uint32_t i=0;i<DescriptorSet->NumBindings;i++)
		DescriptorSet->WriteDescriptorSet[i].dstSet=DescriptorSet->DescriptorSet;

	vkUpdateDescriptorSets(DescriptorSet->Device, DescriptorSet->NumBindings, DescriptorSet->WriteDescriptorSet, 0, VK_NULL_HANDLE);

	return VK_TRUE;
}
