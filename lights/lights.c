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

extern VkuMemZone_t *VkZone;

extern VkFramebuffer ShadowFrameBuffer;
extern VkuImage_t ShadowDepth;

extern VkuBuffer_t shadow_ubo_buffer;

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

		if(Lights->StorageBuffer.Buffer&&Lights->StorageBuffer.DeviceMemory)
		{
			vkDeviceWaitIdle(Context.Device);

			vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);
			vkDestroySampler(Context.Device, ShadowDepth.Sampler, VK_NULL_HANDLE);
			vkDestroyImageView(Context.Device, ShadowDepth.View, VK_NULL_HANDLE);
//			vkFreeMemory(Context.Device, ShadowDepth.DeviceMemory, VK_NULL_HANDLE);
			VkuMem_Free(VkZone, ShadowDepth.DeviceMemory);
			vkDestroyImage(Context.Device, ShadowDepth.Image, VK_NULL_HANDLE);

			vkFreeMemory(Context.Device, shadow_ubo_buffer.DeviceMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(Context.Device, shadow_ubo_buffer.Buffer, VK_NULL_HANDLE);

			vkDestroyBuffer(Context.Device, Lights->StorageBuffer.Buffer, VK_NULL_HANDLE);
			vkFreeMemory(Context.Device, Lights->StorageBuffer.DeviceMemory, VK_NULL_HANDLE);

			vkuCreateHostBuffer(&Context, &Lights->StorageBuffer, (uint32_t)Lights->Lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

			InitShadowCubeMap((uint32_t)(Lights->Lights.bufSize/Lights->Lights.Stride));
		}
	}

	// Update buffer
	if(Lights->StorageBuffer.DeviceMemory)
	{
		void *Data=NULL;

		vkMapMemory(Context.Device, Lights->StorageBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(Data)
			memcpy(Data, Lights->Lights.Buffer, Lights->Lights.Size);

		vkUnmapMemory(Context.Device, Lights->StorageBuffer.DeviceMemory);
	}
}

bool Lights_Init(Lights_t *Lights)
{
	List_Init(&Lights->Lights, sizeof(Light_t), 10, NULL);

	Lights->StorageBuffer.Buffer=VK_NULL_HANDLE;
	Lights->StorageBuffer.DeviceMemory=VK_NULL_HANDLE;

	vkuCreateHostBuffer(&Context, &Lights->StorageBuffer, (uint32_t)Lights->Lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	return true;
}

void Lights_Destroy(Lights_t *Lights)
{
	// Delete storage buffer and free memory
	vkDestroyBuffer(Context.Device, Lights->StorageBuffer.Buffer, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, Lights->StorageBuffer.DeviceMemory, VK_NULL_HANDLE);

	List_Destroy(&Lights->Lights);
}
