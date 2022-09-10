#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../system/system.h"
#include "../utils/list.h"
#include "../utils/genid.h"
#include "lights.h"

extern VkuContext_t Context;

extern VulkanMemZone_t *VkZone;

extern VkFramebuffer ShadowFrameBuffer;
extern Image_t ShadowDepth;

extern VkBuffer shadow_ubo_buffer;
extern VkDeviceMemory shadow_ubo_memory;

void InitShadowCubeMap(uint32_t NumMaps);

uint32_t Lights_Add(Lights_t *Lights, vec3 Position, float Radius, vec4 Kd)
{
	if(Lights==NULL)
		return UINT32_MAX;

	// Pull the next ID from the global ID count
	uint32_t ID=GenID();

	Light_t Light;

	Light.ID=ID;
	Vec3_Setv(Light.Position, Position);
	Light.Radius=1.0f/Radius;
	Vec4_Setv(Light.Kd, Kd);

	Vec4_Sets(Light.SpotDirection, 0.0f);
	Light.SpotOuterCone=0.0f;
	Light.SpotInnerCone=0.0f;
	Light.SpotExponent=0.0f;

	List_Add(&Lights->Lights, (void *)&Light);

	return ID;
}

void Lights_Del(Lights_t *Lights, uint32_t ID)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			List_Del(&Lights->Lights, i);
			break;
		}
	}
}

void Lights_Update(Lights_t *Lights, uint32_t ID, vec3 Position, float Radius, vec4 Kd)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			Vec3_Setv(Light->Position, Position);
			Light->Radius=1.0f/Radius;
			Vec4_Setv(Light->Kd, Kd);

			return;
		}
	}
}

void Lights_UpdatePosition(Lights_t *Lights, uint32_t ID, vec3 Position)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			Vec3_Setv(Light->Position, Position);
			return;
		}
	}
}

void Lights_UpdateRadius(Lights_t *Lights, uint32_t ID, float Radius)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			Light->Radius=1.0f/Radius;
			return;
		}
	}
}

void Lights_UpdateKd(Lights_t *Lights, uint32_t ID, vec4 Kd)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			Vec4_Setv(Light->Kd, Kd);
			return;
		}
	}
}

void Lights_UpdateSpotlight(Lights_t *Lights, uint32_t ID, vec3 Direction, float OuterCone, float InnerCone, float Exponent)
{
	if(Lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&Lights->Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights->Lights, i);

		if(Light->ID==ID)
		{
			Vec3_Setv(Light->SpotDirection, Direction);
			Light->SpotOuterCone=OuterCone;
			Light->SpotInnerCone=InnerCone;
			Light->SpotExponent=Exponent;
			return;
		}
	}
}

void Lights_UpdateSSBO(Lights_t *Lights)
{
	static size_t oldSize=0;

	// If over allocated buffer size changed from last time, delete and recreate the buffer.
	if(oldSize!=Lights->Lights.bufSize)
	{
		oldSize=Lights->Lights.bufSize;

		if(Lights->StorageBuffer&&Lights->StorageMemory)
		{
			vkDeviceWaitIdle(Context.Device);

			vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);
			vkDestroySampler(Context.Device, ShadowDepth.Sampler, VK_NULL_HANDLE);
			vkDestroyImageView(Context.Device, ShadowDepth.View, VK_NULL_HANDLE);
//			vkFreeMemory(Context.Device, ShadowDepth.DeviceMemory, VK_NULL_HANDLE);
			VulkanMem_Free(VkZone, ShadowDepth.DeviceMemory);
			vkDestroyImage(Context.Device, ShadowDepth.Image, VK_NULL_HANDLE);

			vkFreeMemory(Context.Device, shadow_ubo_memory, VK_NULL_HANDLE);
			vkDestroyBuffer(Context.Device, shadow_ubo_buffer, VK_NULL_HANDLE);

			vkDestroyBuffer(Context.Device, Lights->StorageBuffer, VK_NULL_HANDLE);
			vkFreeMemory(Context.Device, Lights->StorageMemory, VK_NULL_HANDLE);

			vkuCreateBuffer(&Context, &Lights->StorageBuffer, &Lights->StorageMemory,
							(uint32_t)Lights->Lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			InitShadowCubeMap((uint32_t)(Lights->Lights.bufSize/Lights->Lights.Stride));
		}
	}

	// Update buffer
	if(Lights->StorageMemory)
	{
		void *Data=NULL;

		vkMapMemory(Context.Device, Lights->StorageMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(Data)
			memcpy(Data, Lights->Lights.Buffer, Lights->Lights.Size);

		vkUnmapMemory(Context.Device, Lights->StorageMemory);
	}
}

bool Lights_Init(Lights_t *Lights)
{
	List_Init(&Lights->Lights, sizeof(Light_t), 10, NULL);

	Lights->StorageBuffer=VK_NULL_HANDLE;
	Lights->StorageMemory=VK_NULL_HANDLE;

	vkuCreateBuffer(&Context, &Lights->StorageBuffer, &Lights->StorageMemory,
					(uint32_t)Lights->Lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	return true;
}

void Lights_Destroy(Lights_t *Lights)
{
	// Delete storage buffer and free memory
	vkDestroyBuffer(Context.Device, Lights->StorageBuffer, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, Lights->StorageMemory, VK_NULL_HANDLE);

	List_Destroy(&Lights->Lights);
}
