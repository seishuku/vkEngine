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
#include "../camera/camera.h"
#include "particle.h"

// External data from engine.c
extern VkuContext_t vkContext;
extern VkSampleCountFlags MSAA;
extern VkFormat ColorFormat, DepthFormat;

extern VkRenderPass renderPass;

extern VkuMemZone_t *vkZone;
////////////////////////////

//static VkuDescriptorSet_t particleDescriptorSet;
static VkPipelineLayout particlePipelineLayout;
static VkuPipeline_t particlePipeline;

static VkuImage_t particleTexture;

struct
{
	matrix mvp;
	vec4 Right;
	vec4 Up;
} particlePC;

// Resizes the OpenGL vertex buffer and system memory vertex buffer
bool ParticleSystem_ResizeBuffer(ParticleSystem_t *system)
{
	if(system==NULL)
		return false;

	mtx_lock(&system->mutex);

	uint32_t count=0;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);
		count+=emitter->numParticles;
	}

	// Resize vertex buffer
	vkDeviceWaitIdle(vkContext.device);

	if(system->particleBuffer.buffer)
	{
//		vkUnmapMemory(vkContext.device, system->particleBuffer.deviceMemory);
		vkDestroyBuffer(vkContext.device, system->particleBuffer.buffer, VK_NULL_HANDLE);
		vkFreeMemory(vkContext.device, system->particleBuffer.deviceMemory, VK_NULL_HANDLE);
	}

	vkuCreateHostBuffer(&vkContext, &system->particleBuffer, sizeof(vec4)*2*count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
//	vkMapMemory(vkContext.device, system->particleBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&system->particleArray);

	mtx_unlock(&system->mutex);

	return true;
}

// Adds a particle emitter to the system
uint32_t ParticleSystem_AddEmitter(ParticleSystem_t *system, vec3 position, vec3 startColor, vec3 endColor, float particleSize, uint32_t numParticles, bool burst, ParticleInitCallback initCallback)
{
	if(system==NULL)
		return UINT32_MAX;

	// Pull the next ID from the global ID count
	uint32_t ID=system->baseID++;

	// Increment emitter count and resize emitter memory

	ParticleEmitter_t emitter;

	if(initCallback==NULL)
		emitter.initCallback=NULL;
	else
		emitter.initCallback=initCallback;

	// Set various flags/parameters
	emitter.burst=burst;
	emitter.ID=ID;
	emitter.startColor=startColor;
	emitter.endColor=endColor;
	emitter.particleSize=particleSize;

	// Set number of particles and allocate memory
	emitter.numParticles=numParticles;
	emitter.particles=Zone_Malloc(zone, numParticles*sizeof(Particle_t));

	if(emitter.particles==NULL)
		return UINT32_MAX;

	memset(emitter.particles, 0, numParticles*sizeof(Particle_t));

	// Set emitter position (used when resetting/recycling particles when they die)
	emitter.position=position;

	// Set initial particle position and life to -1.0 (dead)
	for(uint32_t i=0;i<emitter.numParticles;i++)
	{
		emitter.particles[i].ID=ID;
		emitter.particles[i].position=position;
		emitter.particles[i].life=-1.0f;
	}

	List_Add(&system->emitters, &emitter);

	// Resize vertex buffers (both system memory and OpenGL buffer)
	if(!ParticleSystem_ResizeBuffer(system))
		return UINT32_MAX;

	return ID;
}

// Removes a particle emitter from the system
void ParticleSystem_DeleteEmitter(ParticleSystem_t *system, uint32_t ID)
{
	if(system==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			Zone_Free(zone, emitter->particles);
			List_Del(&system->emitters, i);

			// Resize vertex buffers (both system memory and OpenGL buffer)
			ParticleSystem_ResizeBuffer(system);
			return;
		}
	}
}

// Resets the emitter to the initial parameters (mostly for a "burst" trigger)
void ParticleSystem_ResetEmitter(ParticleSystem_t *system, uint32_t ID)
{
	if(system==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

		if(emitter->ID==ID)
		{
			for(uint32_t j=0;j<emitter->numParticles;j++)
			{
				// Only reset dead particles, limit "total reset" weirdness
				if(emitter->particles[j].life<0.0f)
				{
					// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
					if(emitter->initCallback)
					{
						emitter->initCallback(j, emitter->numParticles, &emitter->particles[j]);

						// Add particle emitter position to the calculated position
						emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, emitter->position);
					}
					else
					{
						float seedRadius=30.0f;
						float theta=RandFloat()*2.0f*PI;
						float r=RandFloat()*seedRadius;

						// Set particle start position to emitter position
						emitter->particles[j].position=emitter->position;
						emitter->particles[j].velocity=Vec3(r*sinf(theta), RandFloat()*100.0f, r*cosf(theta));

						emitter->particles[j].life=RandFloat()*0.999f+0.001f;
					}
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
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

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

	system->particleArray=NULL;

	// Default generic gravity
	system->gravity=Vec3(0.0f, -9.81f, 0.0f);

	//if(!Image_Upload(&Context, &ParticleTexture, "assets/particle.tga", IMAGE_BILINEAR|IMAGE_MIPMAP))
	//	return false;

	//vkuInitDescriptorSet(&ParticleDescriptorSet, &Context);
	//vkuDescriptorSet_AddBinding(&ParticleDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	//vkuAssembleDescriptorSetLayout(&ParticleDescriptorSet);

	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		//.setLayoutCount=1,
		//.pSetLayouts=&ParticleDescriptorSet.descriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.stageFlags=VK_SHADER_STAGE_GEOMETRY_BIT,
			.offset=0,
			.size=sizeof(particlePC)
		},
	}, 0, &particlePipelineLayout);

	vkuInitPipeline(&particlePipeline, &vkContext);

	vkuPipeline_SetPipelineLayout(&particlePipeline, particlePipelineLayout);
	vkuPipeline_SetRenderPass(&particlePipeline, renderPass);

	particlePipeline.topology=VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	particlePipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	particlePipeline.depthTest=VK_TRUE;
	particlePipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	particlePipeline.depthWrite=VK_FALSE;
	particlePipeline.rasterizationSamples=MSAA;

	particlePipeline.blend=VK_TRUE;
	particlePipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	particlePipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE;
	particlePipeline.colorBlendOp=VK_BLEND_OP_ADD;
	particlePipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	particlePipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
	particlePipeline.alphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&particlePipeline, "shaders/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&particlePipeline, "shaders/particle.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT))
		return false;

	if(!vkuPipeline_AddStage(&particlePipeline, "shaders/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&particlePipeline, 0, sizeof(vec4)*2, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&particlePipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
	vkuPipeline_AddVertexAttribute(&particlePipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4));

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&ColorFormat,
	//	.depthAttachmentFormat=DepthFormat,
	//};

	if(!vkuAssemblePipeline(&particlePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}

void ParticleSystem_Step(ParticleSystem_t *system, float dt)
{
	if(system==NULL)
		return;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

		for(uint32_t j=0;j<emitter->numParticles;j++)
		{
			emitter->particles[j].life-=dt*0.75f;

			// If the particle is dead and isn't a one shot (burst), restart it...
			// Otherwise run the math for the particle system motion.
			if(emitter->particles[j].life<0.0f&&!emitter->burst)
			{
				// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
				if(emitter->initCallback)
				{
					emitter->initCallback(j, emitter->numParticles, &emitter->particles[j]);

					// Add particle emitter position to the calculated position
					emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, emitter->position);
				}
				else
				{
					float seedRadius=30.0f;
					float theta=RandFloat()*2.0f*PI;
					float r=RandFloat()*seedRadius;

					// Set particle start position to emitter position
					emitter->particles[j].position=emitter->position;
					emitter->particles[j].velocity=Vec3(r*sinf(theta), RandFloat()*100.0f, r*cosf(theta));

					emitter->particles[j].life=RandFloat()*0.999f+0.001f;
				}
			}
			else
			{
				if(emitter->particles[j].life>0.0f)
				{
					emitter->particles[j].velocity=Vec3_Addv(emitter->particles[j].velocity, Vec3_Muls(system->gravity, dt));
					emitter->particles[j].position=Vec3_Addv(emitter->particles[j].position, Vec3_Muls(emitter->particles[j].velocity, dt));
				}
			}
		}
	}
}

void ParticleSystem_Draw(ParticleSystem_t *system, VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool, matrix modelview, matrix projection)
{
	if(system==NULL)
		return;

	mtx_lock(&system->mutex);

	float *array=NULL;//system->particleArray;
	vkMapMemory(vkContext.device, system->particleBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&array);

	if(array==NULL)
		return;

	uint32_t count=0;

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

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
				*array++=emitter->particles[j].life;

				count++;
			}
		}
	}

	vkUnmapMemory(vkContext.device, system->particleBuffer.deviceMemory);

	particlePC.mvp=MatrixMult(modelview, projection);
	particlePC.Right=Vec4(modelview.x.x, modelview.y.x, modelview.z.x, modelview.w.x);
	particlePC.Up=Vec4(modelview.x.y, modelview.y.y, modelview.z.y, modelview.w.y);

	//vkuDescriptorSet_UpdateBindingImageInfo(&particleDescriptorSet, 0, &particleTexture);
	//vkuAllocateUpdateDescriptorSet(&particleDescriptorSet, descriptorPool);

	//vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipelineLayout, 0, 1, &particleDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline.pipeline);
	vkCmdPushConstants(commandBuffer, particlePipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(particlePC), &particlePC);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &system->particleBuffer.buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(commandBuffer, count, 1, 0, 0);

	mtx_unlock(&system->mutex);
}

void ParticleSystem_Destroy(ParticleSystem_t *system)
{
	if(system==NULL)
		return;

//	vkUnmapMemory(vkContext.device, system->particleBuffer.deviceMemory);
	vkuDestroyBuffer(&vkContext, &system->particleBuffer);

	//vkuDestroyImageBuffer(&Context, &particleTexture);

	vkDestroyPipeline(vkContext.device, particlePipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.device, particlePipelineLayout, VK_NULL_HANDLE);
	//vkDestroyDescriptorSetLayout(Context.device, particleDescriptorSet.descriptorSetLayout, VK_NULL_HANDLE);

	for(uint32_t i=0;i<List_GetCount(&system->emitters);i++)
	{
		ParticleEmitter_t *emitter=List_GetPointer(&system->emitters, i);

		Zone_Free(zone, emitter->particles);
	}

	List_Destroy(&system->emitters);
}
