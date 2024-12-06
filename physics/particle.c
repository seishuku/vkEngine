#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <threads.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../image/image.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../utils/pipeline.h"
#include "../camera/camera.h"
#include "../perframe.h"
#include "particle.h"

// External data from engine.c
extern VkuContext_t vkContext;
extern VkSampleCountFlags MSAA;
extern VkFormat ColorFormat, DepthFormat;

extern VkRenderPass renderPass;
extern VkuSwapchain_t swapchain;

extern Camera_t camera;
////////////////////////////

static Pipeline_t particlePipeline;

//static VkuImage_t particleTexture;

struct
{
	matrix mvp;
	vec4 Right;
	vec4 Up;
} particlePC;

inline static void emitterDefaultInit(Particle_t *particle)
{
	float seedRadius=30.0f;
	float theta=RandFloat()*2.0f*PI;
	float r=RandFloat()*seedRadius;

	// Set particle start position to emitter position
	particle->position=Vec3b(0.0f);
	particle->velocity=Vec3(r*sinf(theta), RandFloat()*100.0f, r*cosf(theta));

	particle->life=RandFloat()*0.999f+0.001f;
}

// Adds a particle emitter to the system
uint32_t ParticleSystem_AddEmitter(ParticleSystem_t *system, vec3 position, vec3 startColor, vec3 endColor, float particleSize, uint32_t numParticles, ParticleEmitterType_e type, ParticleInitCallback initCallback)
{
	if(system==NULL)
		return UINT32_MAX;

	mtx_lock(&system->mutex);

	// Pull the next ID from the global ID count
	uint32_t ID=system->baseID++;

	// Increment emitter count and resize emitter memory

	ParticleEmitter_t emitter;

	if(initCallback==NULL)
		emitter.initCallback=NULL;
	else
		emitter.initCallback=initCallback;

	// Set various flags/parameters
	emitter.type=type;
	emitter.ID=ID;
	emitter.startColor=startColor;
	emitter.endColor=endColor;
	emitter.particleSize=particleSize;

	// Set number of particles and allocate memory
	emitter.numParticles=numParticles;
	emitter.particles=(Particle_t *)Zone_Malloc(zone, numParticles*sizeof(Particle_t));

	if(emitter.particles==NULL)
	{
		mtx_unlock(&system->mutex);
		return UINT32_MAX;
	}

	memset(emitter.particles, 0, numParticles*sizeof(Particle_t));

	// Set emitter position (used when resetting/recycling particles when they die)
	emitter.position=position;

	// Set initial particle position and life to -1.0 (dead), unless it's a one-shot, then set it up with default or callback
	for(uint32_t i=0;i<emitter.numParticles;i++)
	{
		emitter.particles[i].ID=ID;
		emitter.particles[i].position=position;

		if(emitter.type==PARTICLE_EMITTER_ONCE)
		{
			if(emitter.initCallback)
				emitter.initCallback(i, emitter.numParticles, &emitter.particles[i]);
			else
				emitterDefaultInit(&emitter.particles[i]);

			// Add particle emitter position to the calculated position
			emitter.particles[i].position=Vec3_Addv(emitter.particles[i].position, emitter.position);
		}
		else
			emitter.particles[i].life=-1.0f;
	}

	List_Add(&system->emitters, &emitter);

	mtx_unlock(&system->mutex);

	return ID;
}

// Removes a particle emitter from the system
void ParticleSystem_DeleteEmitter(ParticleSystem_t *system, uint32_t ID)
{
	if(system==NULL||ID==UINT32_MAX)
		return;

	mtx_lock(&system->mutex);

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			Zone_Free(zone, emitter->particles);
			List_Del(&system->emitters, i);

			// Resize vertex buffers (both system memory and OpenGL buffer)
			// ParticleSystem_ResizeBuffer(system);
			break;
		}
	}

	mtx_unlock(&system->mutex);
}

// Resets the emitter to the initial parameters (mostly for a "burst" trigger)
void ParticleSystem_ResetEmitter(ParticleSystem_t *system, uint32_t ID)
{
	if(system==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			for(uint32_t j=0;j<emitter->numParticles;j++)
			{
				// Only reset dead particles, limit "total reset" weirdness
				if(emitter->particles[j].life<0.0f)
				{
					// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
					if(emitter->initCallback)
						emitter->initCallback(j, emitter->numParticles, &emitter->particles[j]);
					else
						emitterDefaultInit(&emitter->particles[j]);

					// Add particle emitter position to the calculated position
					emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, emitter->position);
				}
			}

			return;
		}
	}
}

void ParticleSystem_SetEmitterPosition(ParticleSystem_t *system, uint32_t ID, vec3 position)
{
	if(system==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t*)List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			emitter->position=position;
			return;
		}
	}
}

bool ParticleSystem_SetGravity(ParticleSystem_t *system, float x, float y, float z)
{
	if(system==NULL)
		return false;

	system->gravity=Vec3(x, y, z);

	return true;
}

bool ParticleSystem_SetGravityv(ParticleSystem_t *system, vec3 v)
{
	if(system==NULL)
		return false;

	system->gravity=v;

	return true;
}

bool ParticleSystem_Init(ParticleSystem_t *system)
{
	if(system==NULL)
		return false;

	if(mtx_init(&system->mutex, mtx_plain))
	{
		DBGPRINTF(DEBUG_ERROR, "ParticleSystem_Init: Unable to create mutex.\r\n");
		return false;
	}

	system->baseID=0;

	List_Init(&system->emitters, sizeof(ParticleEmitter_t), 10, NULL);

	system->count=0;

	// Default generic gravity
	system->gravity=Vec3(0.0f, -9.81f, 0.0f);

	if(!CreatePipeline(&vkContext, &particlePipeline, renderPass, "pipelines/particle.pipeline"))
		return false;

	// Pre-allocate minimal sized buffers
	for(uint32_t i=0;i<swapchain.numImages;i++)
		vkuCreateHostBuffer(&vkContext, &system->particleBuffer[i], sizeof(vec4)*2, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	return true;
}

void ParticleSystem_Step(ParticleSystem_t *system, float dt)
{
	if(system==NULL)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);
		bool isActive=false;

		for(uint32_t j=0;j<emitter->numParticles;j++)
		{
			if(emitter->particles[j].life>0.0f)
			{
				isActive=true;
				emitter->particles[j].velocity=Vec3_Addv(emitter->particles[j].velocity, Vec3_Muls(system->gravity, dt));
				emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, Vec3_Muls(emitter->particles[j].velocity, dt));
			}
			else if(emitter->type==PARTICLE_EMITTER_CONTINOUS)
			{
				// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
				if(emitter->initCallback)
					emitter->initCallback(j, emitter->numParticles, &emitter->particles[j]);
				else
					emitterDefaultInit(&emitter->particles[j]);

				// Add particle emitter position to the calculated position
				emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, emitter->position);
			}

			emitter->particles[j].life-=dt*0.75f;
		}

		if(!isActive&&emitter->type==PARTICLE_EMITTER_ONCE)
		{
			DBGPRINTF(DEBUG_WARNING, "REMOVING UNUSED EMITTER #%d\n", emitter->ID);
			ParticleSystem_DeleteEmitter(system, emitter->ID);
		}
	}
}

int compareParticles(const void *a, const void *b)
{
	vec3 *particleA=(vec3 *)a;
	vec3 *particleB=(vec3 *)b;

	float distA=Vec3_DistanceSq(*particleA, camera.body.position);
	float distB=Vec3_DistanceSq(*particleB, camera.body.position);

	if(distA>distB)
		return -1;

	if(distA<distB)
		return 1;

	return 0;
}

void ParticleSystem_Draw(ParticleSystem_t *system, VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye)
{
	if(system==NULL)
		return;

	mtx_lock(&system->mutex);

	uint32_t count=0;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);
		count+=emitter->numParticles;
	}

	// If the count isn't what the last count was, resize the buffer.
	if(count!=system->count)
	{
		// Set new total particle count
		system->count=count;
		system->systemBuffer=(float *)Zone_Realloc(zone, system->systemBuffer, sizeof(vec4)*2*count);
	}

	// Check current frame's vertex buffer size against the current particle count needs
	const size_t curSize=sizeof(vec4)*2*system->count;
	if(curSize>system->particleBuffer[index].memory->size)
	{
		// Reallocate if so
		vkuDestroyBuffer(&vkContext, &system->particleBuffer[index]);
		vkuCreateHostBuffer(&vkContext, &system->particleBuffer[index], sizeof(vec4)*2*count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	}

	// zero count and reuse it for final vertex data 
	count=0;
	float *array=system->systemBuffer;

	if(array==NULL)
	{
		mtx_unlock(&system->mutex);
		return;
	}

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		for(uint32_t j=0;j<emitter->numParticles;j++)
		{
			// Only draw ones that are alive still
			if(emitter->particles[j].life>0.0f)
			{
				*array++=emitter->particles[j].position.x;
				*array++=emitter->particles[j].position.y;
				*array++=emitter->particles[j].position.z;
				*array++=emitter->particleSize;
				vec3 color=Vec3_Lerp(emitter->startColor, emitter->endColor, emitter->particles[j].life);
				*array++=color.x;
				*array++=color.y;
				*array++=color.z;
				*array++=clampf(emitter->particles[j].life, 0.0f, 1.0f);

				count++;
			}
		}
	}

	qsort(system->systemBuffer, count, sizeof(vec4)*2, compareParticles);

	memcpy(system->particleBuffer[index].memory->mappedPointer, system->systemBuffer, sizeof(vec4)*2*count);

	mtx_unlock(&system->mutex);

	matrix modelview=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	particlePC.mvp=MatrixMult(modelview, perFrame[index].mainUBO[eye]->projection);
	particlePC.Right=Vec4(modelview.x.x, modelview.y.x, modelview.z.x, modelview.w.x);
	particlePC.Up=Vec4(modelview.x.y, modelview.y.y, modelview.z.y, modelview.w.y);

	//vkuDescriptorSet_UpdateBindingImageInfo(&particleDescriptorSet, 0, &particleTexture);
	//vkuAllocateUpdateDescriptorSet(&particleDescriptorSet, descriptorPool);

	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipelineLayout, 0, 1, &particleDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, particlePipeline.pipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(particlePC), &particlePC);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &system->particleBuffer[index].buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(commandBuffer, count, 1, 0, 0);
}

void ParticleSystem_Destroy(ParticleSystem_t *system)
{
	if(system==NULL)
		return;

	for(uint32_t i=0;i<swapchain.numImages;i++)
		vkuDestroyBuffer(&vkContext, &system->particleBuffer[i]);

	Zone_Free(zone, system->systemBuffer);

	//vkuDestroyImageBuffer(&Context, &particleTexture);

	DestroyPipeline(&vkContext, &particlePipeline);

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		Zone_Free(zone, emitter->particles);
	}

	List_Destroy(&system->emitters);
}
