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

extern VkRenderPass renderPass;
extern VkuSwapchain_t swapchain;

extern Camera_t camera;
////////////////////////////

static Pipeline_t particlePipeline;

//static VkuImage_t particleTexture;

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

// addParticle does a slow linear search for first available particle, however,
//   particles early in the list are more likely to die first, so it should exit fairly early.
static bool addParticle(ParticleSystem_t *system, Particle_t particle)
{
	// Check if there's enough space
	if(system->numParticles<system->maxParticles)
	{
		// Find first available particle and set it
		for(uint32_t i=0;i<system->maxParticles;i++)
		{
			if(system->particles[i].life<=0.0f)
			{
				system->particles[i]=particle;
				return true;
			}
		}
	}

	// No free particles available
	return false;
}

// Adds a particle emitter to the system
// Note: numParticles is total number of particles for burst/once types, but is particles-per-second for continous type.
uint32_t ParticleSystem_AddEmitter(ParticleSystem_t *system, vec3 position, vec3 startColor, vec3 endColor, float particleSize, uint32_t numParticles, ParticleEmitterType_e type, ParticleInitCallback initCallback)
{
	if(system==NULL)
		return UINT32_MAX;

	mtx_lock(&system->mutex);

	// Pull the next ID from the global ID count
	uint32_t ID=system->baseID++;

	// Set up the emitter structure
	ParticleEmitter_t emitter=
	{
		.ID=ID,
		.type=type,
		.position=position,
		.startColor=startColor,
		.endColor=endColor,
		.particleSize=particleSize,
		.emissionRate=(float)numParticles,
		.emissionInterval=1.0f/emitter.emissionRate,
		.emissionTime=0.0f,
		.initCallback=initCallback
	};

	// If it's a one time emitter, just dump a bunch of new particles,
	//   otherwise add the emitter to the list for later processing
	if(emitter.type==PARTICLE_EMITTER_ONCE)
	{
		// Spawn the particles
		for(uint32_t i=0;i<numParticles;i++)
		{
			Particle_t particle;

			if(emitter.initCallback)
				emitter.initCallback(i, numParticles, &particle);
			else
				emitterDefaultInit(&particle);

			particle.startColor=emitter.startColor;
			particle.endColor=emitter.endColor;
			particle.particleSize=emitter.particleSize;
			particle.position=Vec3_Addv(particle.position, emitter.position);

			addParticle(system, particle);
		}
	}
	else
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
			List_Del(&system->emitters, i);
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

	mtx_lock(&system->mutex);

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			if(emitter->type==PARTICLE_EMITTER_BURST)
			{
				// Spawn the particles
				for(uint32_t i=0;i<(uint32_t)emitter->emissionRate;i++)
				{
					Particle_t particle;

					if(emitter->initCallback)
						emitter->initCallback(i, (uint32_t)emitter->emissionRate, &particle);
					else
						emitterDefaultInit(&particle);

					particle.startColor=emitter->startColor;
					particle.endColor=emitter->endColor;
					particle.particleSize=emitter->particleSize;
					particle.position=Vec3_Addv(particle.position, emitter->position);

					addParticle(system, particle);
				}
			}
		}
	}

	mtx_unlock(&system->mutex);
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

	system->numParticles=0;
	system->maxParticles=100000;

	system->particles=(Particle_t *)Zone_Malloc(zone, sizeof(Particle_t)*system->maxParticles);

	if(system->particles==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "ParticleSystem_Init: Unable to allocate memory for particle pool.\r\n");
		return false;
	}
	else
	{
		for(uint32_t i=0;i<system->maxParticles;i++)
		{
			system->particles[i].position=Vec3b(0.0f);
			system->particles[i].velocity=Vec3b(0.0f);
			system->particles[i].startColor=Vec3b(0.0f);
			system->particles[i].endColor=Vec3b(0.0f);
			system->particles[i].particleSize=0.0f;
			system->particles[i].life=-1.0f;
		}
	}

	system->systemBuffer=(ParticleVertex_t *)Zone_Malloc(zone, sizeof(ParticleVertex_t)*system->maxParticles);

	if(system->systemBuffer==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "ParticleSystem_Init: Unable to allocate memory for system array.\r\n");
		return false;
	}

	// Default generic gravity
	system->gravity=Vec3(0.0f, -9.81f, 0.0f);

	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &particlePipeline, renderPass, "pipelines/particle.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	// Pre-allocate minimal sized buffers
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
		vkuCreateHostBuffer(&vkContext, &system->particleBuffer[i], sizeof(vec4)*2*system->maxParticles, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	return true;
}

void ParticleSystem_Step(ParticleSystem_t *system, float dt)
{
	if(system==NULL)
		return;

	mtx_lock(&system->mutex);

	// Run all alive particles
	for(uint32_t i=0;i<system->maxParticles;i++)
	{
		if(system->particles[i].life>0.0f)
		{
			system->particles[i].velocity=Vec3_Addv(system->particles[i].velocity, Vec3_Muls(system->gravity, dt));
			system->particles[i].position=Vec3_Addv(system->particles[i].position, Vec3_Muls(system->particles[i].velocity, dt));
		}

		system->particles[i].life-=dt;
	}

	// Run emitters and spawn particles based on emission rate
	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=(ParticleEmitter_t *)List_GetPointer(&system->emitters, i);

		if(emitter->type==PARTICLE_EMITTER_CONTINOUS)
		{
			uint32_t index=0;

			emitter->emissionTime+=dt;

			while(emitter->emissionTime>emitter->emissionInterval)
			{
				emitter->emissionTime-=emitter->emissionInterval;

				Particle_t particle;

				if(emitter->initCallback)
					emitter->initCallback(index++, (uint32_t)(emitter->emissionRate*dt), &particle);
				else
					emitterDefaultInit(&particle);

				particle.startColor=emitter->startColor;
				particle.endColor=emitter->endColor;
				particle.particleSize=emitter->particleSize;
				particle.position=Vec3_Addv(particle.position, emitter->position);

				addParticle(system, particle);
			}
		}
	}

	mtx_unlock(&system->mutex);
}

static void RadixSortParticles(ParticleVertex_t *particles, uint32_t count, const vec3 cameraPos)
{
	if(count<=1)
		return;

	// Single allocation for all temporary buffers
	size_t totalSize=sizeof(uint32_t)*count*3+sizeof(ParticleVertex_t)*count;

	void *tempBlock=Zone_Malloc(zone, totalSize);

	uint32_t *keys=(uint32_t *)tempBlock;
	uint32_t *indices=keys+count;
	uint32_t *dst=indices+count;

	ParticleVertex_t *tempParticles=(ParticleVertex_t *)(dst+count);

	for(uint32_t i=0;i<count;i++)
	{
		float distSq=Vec3_DistanceSq(Vec3(particles[i].posSize.x, particles[i].posSize.y, particles[i].posSize.z), cameraPos);

		// "float flip"
		memcpy(&keys[i], &distSq, sizeof(float));
		uint32_t mask=-(int32_t)(keys[i]>>31)|0x80000000;

		keys[i]^=mask;

		indices[i]=i;
	}

	uint32_t histogram[256];
	uint32_t *src=indices;

	for(int shift=0;shift<32;shift+=8)
	{
		memset(histogram, 0, sizeof(histogram));

		// Count byte frequencies
		for(uint32_t i=0;i<count;i++)
		{
			uint32_t key=keys[src[i]];
			uint32_t byte=(key>>shift)&0xFF;
			histogram[byte]++;
		}

		// Convert to prefix sums
		uint32_t sum=0;
		for(int i=0;i<256;i++)
		{
			uint32_t temp=histogram[i];
			histogram[i]=sum;
			sum+=temp;
		}

		// Distribute elements
		for(uint32_t i=0;i<count;i++)
		{
			uint32_t key=keys[src[i]];
			uint32_t byte=(key>>shift)&0xFF;
			dst[histogram[byte]++]=src[i];
		}

		// Swap buffers
		uint32_t *tmp=src;
		src=dst;
		dst=tmp;
	}

	// Reorder in descending order (farthest to nearest)
	for(uint32_t i=0;i<count;i++)
		tempParticles[i]=particles[src[count-1-i]];

	memcpy(particles, tempParticles, sizeof(ParticleVertex_t)*count);

	// Free the single temporary block
	Zone_Free(zone, tempBlock);
}

void ParticleSystem_Draw(ParticleSystem_t *system, VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye)
{
	if(system==NULL)
		return;

	mtx_lock(&system->mutex);

	system->numParticles=0;
	ParticleVertex_t *vertices=system->systemBuffer;

	for(uint32_t i=0;i<system->maxParticles;i++)
	{
		if(system->particles[i].life>0.0f)
		{
			vec3 color=Vec3_Lerp(system->particles[i].startColor, system->particles[i].endColor, system->particles[i].life);
			vertices->posSize=Vec4_Vec3(system->particles[i].position, system->particles[i].particleSize);
			vertices->colorLife=Vec4_Vec3(color, clampf(system->particles[i].life, 0.0f, 1.0f));
			vertices++;
			system->numParticles++;
		}
	}

	RadixSortParticles(system->systemBuffer, system->numParticles, camera.body.position);
	memcpy(system->particleBuffer[index].memory->mappedPointer, system->systemBuffer, sizeof(ParticleVertex_t)*system->numParticles);

	mtx_unlock(&system->mutex);

	struct
	{
		matrix modelview;
		matrix projection;
	} particlePC;
	
	particlePC.modelview=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	particlePC.projection=perFrame[index].mainUBO[eye]->projection;
	
	//vkuDescriptorSet_UpdateBindingImageInfo(&particleDescriptorSet, 0, &particleTexture);
	//vkuAllocateUpdateDescriptorSet(&particleDescriptorSet, descriptorPool);
	
	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipelineLayout, 0, 1, &particleDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, particlePipeline.pipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(particlePC), &particlePC);
	
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &system->particleBuffer[index].buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(commandBuffer, system->numParticles, 1, 0, 0);
}

void ParticleSystem_Destroy(ParticleSystem_t *system)
{
	if(system==NULL)
		return;

	DestroyPipeline(&vkContext, &particlePipeline);

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
		vkuDestroyBuffer(&vkContext, &system->particleBuffer[i]);

	//vkuDestroyImageBuffer(&Context, &particleTexture);

	Zone_Free(zone, system->systemBuffer);
	Zone_Free(zone, system->particles);

	List_Destroy(&system->emitters);
}
