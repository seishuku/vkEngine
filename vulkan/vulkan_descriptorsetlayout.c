// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

// Add a binding to the descriptor set layout
VkBool32 vkuDescriptorSetLayout_AddBinding(VkuDescriptorSetLayout_t *DescriptorSetLayout, uint32_t Binding,
										   VkDescriptorType Type, uint32_t Count, VkShaderStageFlags Stage,
										   const VkSampler *ImmutableSamplers)
{
	if(!DescriptorSetLayout)
		return VK_FALSE;

	// Already at max bindings
	if(DescriptorSetLayout->NumBindings>=VKU_MAX_DESCRIPTORSETLAYOUT_BINDINGS)
		return VK_FALSE;

	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding=
	{
		.binding=Binding,
		.descriptorType=Type,
		.descriptorCount=Count,
		.stageFlags=Stage,
		.pImmutableSamplers=ImmutableSamplers
	};

	DescriptorSetLayout->Bindings[DescriptorSetLayout->NumBindings]=DescriptorSetLayoutBinding;
	DescriptorSetLayout->NumBindings++;

	return VK_TRUE;
}

// Initalize the descriptor set layout structures
VkBool32 vkuInitDescriptorSetLayout(VkuDescriptorSetLayout_t *DescriptorSetLayout, VkuContext_t *Context)
{
	if(!DescriptorSetLayout||!Context)
		return VK_FALSE;

	DescriptorSetLayout->Device=Context->Device;

	DescriptorSetLayout->NumBindings=0;
	memset(DescriptorSetLayout->Bindings, 0, sizeof(VkDescriptorSetLayout)*VKU_MAX_DESCRIPTORSETLAYOUT_BINDINGS);

	return VK_TRUE;
}

// Creates the descriptor set layout
VkBool32 vkuAssembleDescriptorSetLayout(VkuDescriptorSetLayout_t *DescriptorSetLayout)
{
	if(!DescriptorSetLayout)
		return VK_FALSE;

	VkResult Result=vkCreateDescriptorSetLayout(DescriptorSetLayout->Device, &(VkDescriptorSetLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//		.flags=VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
		.bindingCount=DescriptorSetLayout->NumBindings,
		.pBindings=DescriptorSetLayout->Bindings,
	}, NULL, &DescriptorSetLayout->DescriptorSetLayout);

	return Result==VK_SUCCESS?VK_TRUE:VK_FALSE;
}
