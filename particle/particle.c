#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../image/image.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../utils/genid.h"
#include "../camera/camera.h"
#include "particle.h"

// External data from engine.c
extern VkuContext_t Context;
extern VkRenderPass RenderPass;
extern VkSampleCountFlags MSAA;

extern VkuMemZone_t *VkZone;

typedef struct
{
	matrix HMD;
	matrix projection;
	matrix modelview;
	matrix light_mvp;
	vec4 light_color;
	vec4 light_direction;
} UBO_t;

extern UBO_t *Main_UBO[2];

////////////////////////////

float PartGrav[3]={ 0.0, -9.81f, 0.0 };

VkuDescriptorSet_t ParticleDescriptorSet;
VkPipelineLayout ParticlePipelineLayout;
VkuPipeline_t ParticlePipeline;

VkuImage_t ParticleTexture;

struct
{
	matrix mvp;
	vec4 Right;
	vec4 Up;
} ParticlePC;

// Resizes the OpenGL vertex buffer and system memory vertex buffer
bool ParticleSystem_ResizeBuffer(ParticleSystem_t *System)
{
	if(System==NULL)
		return false;

	uint32_t Count=0;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);
		Count+=Emitter->NumParticles;
	}

	// Resize vertex buffer
	vkDeviceWaitIdle(Context.Device);

	if(System->ParticleBuffer.Buffer)
	{
		vkUnmapMemory(Context.Device, System->ParticleBuffer.DeviceMemory);
		vkDestroyBuffer(Context.Device, System->ParticleBuffer.Buffer, VK_NULL_HANDLE);
		vkFreeMemory(Context.Device, System->ParticleBuffer.DeviceMemory, VK_NULL_HANDLE);
	}

	vkuCreateHostBuffer(&Context, &System->ParticleBuffer, sizeof(vec4)*2*Count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	vkMapMemory(Context.Device, System->ParticleBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&System->ParticleArray);

	return true;
}

// Adds a particle emitter to the system
uint32_t ParticleSystem_AddEmitter(ParticleSystem_t *System, vec3 Position, vec3 StartColor, vec3 EndColor, float ParticleSize, uint32_t NumParticles, bool Burst, ParticleInitCallback InitCallback)
{
	if(System==NULL)
		return false;

	// Pull the next ID from the global ID count
	uint32_t ID=GenID();

	// Increment emitter count and resize emitter memory

	ParticleEmitter_t Emitter;

	if(InitCallback==NULL)
		Emitter.InitCallback=NULL;
	else
		Emitter.InitCallback=InitCallback;

	// Set various flags/parameters
	Emitter.Burst=Burst;
	Emitter.ID=ID;
	Vec3_Setv(Emitter.StartColor, StartColor);
	Vec3_Setv(Emitter.EndColor, EndColor);
	Emitter.ParticleSize=ParticleSize;

	// Set number of particles and allocate memory
	Emitter.NumParticles=NumParticles;
	Emitter.Particles=Zone_Malloc(Zone, NumParticles*sizeof(Particle_t));

	if(Emitter.Particles==NULL)
		return UINT32_MAX;

	memset(Emitter.Particles, 0, NumParticles*sizeof(Particle_t));

	// Set emitter position (used when resetting/recycling particles when they die)
	Vec3_Setv(Emitter.Position, Position);

	// Set initial particle position and life to -1.0 (dead)
	for(uint32_t i=0;i<Emitter.NumParticles;i++)
	{
		Emitter.Particles[i].ID=ID;
		Vec3_Setv(Emitter.Particles[i].pos, Position);
		Emitter.Particles[i].life=-1.0f;
	}

	List_Add(&System->Emitters, &Emitter);

	// Resize vertex buffers (both system memory and OpenGL buffer)
	if(!ParticleSystem_ResizeBuffer(System))
		return UINT32_MAX;

	return ID;
}

// Removes a particle emitter from the system
void ParticleSystem_DeleteEmitter(ParticleSystem_t *System, uint32_t ID)
{
	if(System==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		if(Emitter->ID==ID)
		{
			Zone_Free(Zone, Emitter->Particles);
			List_Del(&System->Emitters, i);

			// Resize vertex buffers (both system memory and OpenGL buffer)
			ParticleSystem_ResizeBuffer(System);
			return;
		}
	}
}

// Resets the emitter to the initial parameters (mostly for a "burst" trigger)
void ParticleSystem_ResetEmitter(ParticleSystem_t *System, uint32_t ID)
{
	if(System==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		if(Emitter->ID==ID)
		{
			for(uint32_t j=0;j<Emitter->NumParticles;j++)
			{
				// Only reset dead particles, limit "total reset" weirdness
				if(Emitter->Particles[j].life<0.0f)
				{
					// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
					if(Emitter->InitCallback)
					{
						Emitter->InitCallback(j, Emitter->NumParticles, &Emitter->Particles[j]);

						// Add particle emitter position to the calculated position
						Vec3_Addv(Emitter->Particles[j].pos, Emitter->Position);
					}
					else
					{
						float SeedRadius=30.0f;
						float theta=((float)rand()/RAND_MAX)*2.0f*PI;
						float r=((float)rand()/RAND_MAX)*SeedRadius;

						// Set particle start position to emitter position
						Vec3_Setv(Emitter->Particles[j].pos, Emitter->Position);

						Emitter->Particles[j].vel[0]=r*sinf(theta);
						Emitter->Particles[j].vel[1]=((float)rand()/RAND_MAX)*100.0f;
						Emitter->Particles[j].vel[2]=r*cosf(theta);

						Emitter->Particles[j].life=((float)rand()/RAND_MAX)*0.999f+0.001f;
					}
				}
			}

			return;
		}
	}
}

void ParticleSystem_SetEmitterPosition(ParticleSystem_t *System, uint32_t ID, vec3 Position)
{
	if(System==NULL||ID==UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		if(Emitter->ID==ID)
		{
			Vec3_Setv(Emitter->Position, Position);
			return;
		}
	}
}

bool ParticleSystem_Init(ParticleSystem_t *System)
{
	if(System==NULL)
		return false;

	List_Init(&System->Emitters, sizeof(ParticleEmitter_t), 10, NULL);

	System->ParticleArray=NULL;

	if(!Image_Upload(&Context, &ParticleTexture, "./assets/particle.tga", IMAGE_BILINEAR|IMAGE_MIPMAP))
		return false;

	vkuInitDescriptorSet(&ParticleDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&ParticleDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&ParticleDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&ParticleDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.stageFlags=VK_SHADER_STAGE_GEOMETRY_BIT,
			.offset=0,
			.size=sizeof(ParticlePC)
		},
	}, 0, &ParticlePipelineLayout);

	vkuInitPipeline(&ParticlePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ParticlePipeline, ParticlePipelineLayout);
	vkuPipeline_SetRenderPass(&ParticlePipeline, RenderPass);

	ParticlePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	ParticlePipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	ParticlePipeline.DepthTest=VK_TRUE;
	ParticlePipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	ParticlePipeline.DepthWrite=VK_FALSE;
	ParticlePipeline.RasterizationSamples=MSAA;

	ParticlePipeline.Blend=VK_TRUE;
	ParticlePipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	ParticlePipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE;
	ParticlePipeline.ColorBlendOp=VK_BLEND_OP_ADD;
	ParticlePipeline.SrcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	ParticlePipeline.DstAlphaBlendFactor=VK_BLEND_FACTOR_ONE;
	ParticlePipeline.AlphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&ParticlePipeline, "./shaders/particle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&ParticlePipeline, "./shaders/particle.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT))
		return false;

	if(!vkuPipeline_AddStage(&ParticlePipeline, "./shaders/particle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&ParticlePipeline, 0, sizeof(vec4)*2, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&ParticlePipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
	vkuPipeline_AddVertexAttribute(&ParticlePipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4));

	if(!vkuAssemblePipeline(&ParticlePipeline))
		return false;

	return true;
}

void ParticleSystem_Step(ParticleSystem_t *System, float dt)
{
	if(System==NULL)
		return;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		for(uint32_t j=0;j<Emitter->NumParticles;j++)
		{
			Emitter->Particles[j].life-=dt*0.75f;

			// If the particle is dead and isn't a one shot (burst), restart it...
			// Otherwise run the math for the particle system motion.
			if(Emitter->Particles[j].life<0.0f&&!Emitter->Burst)
			{
				// If a velocity/life callback was set, use it... Otherwise use default "fountain" style
				if(Emitter->InitCallback)
				{
					Emitter->InitCallback(j, Emitter->NumParticles, &Emitter->Particles[j]);

					// Add particle emitter position to the calculated position
					Vec3_Addv(Emitter->Particles[j].pos, Emitter->Position);
				}
				else
				{
					float SeedRadius=30.0f;
					float theta=((float)rand()/RAND_MAX)*2.0f*PI;
					float r=((float)rand()/RAND_MAX)*SeedRadius;

					// Set particle start position to emitter position
					Vec3_Setv(Emitter->Particles[j].pos, Emitter->Position);

					Emitter->Particles[j].vel[0]=r*sinf(theta);
					Emitter->Particles[j].vel[1]=((float)rand()/RAND_MAX)*100.0f;
					Emitter->Particles[j].vel[2]=r*cosf(theta);

					Emitter->Particles[j].life=((float)rand()/RAND_MAX)*0.999f+0.001f;
				}
			}
			else
			{
				if(Emitter->Particles[j].life>0.0f)
				{
					vec3 temp;

					Vec3_Setv(temp, PartGrav);
					Vec3_Muls(temp, dt);
					Vec3_Addv(Emitter->Particles[j].vel, temp);

					Vec3_Setv(temp, Emitter->Particles[j].vel);
					Vec3_Muls(temp, dt);
					Vec3_Addv(Emitter->Particles[j].pos, temp);
				}
			}
		}
	}
}

void ParticleSystem_Draw(ParticleSystem_t *System, VkCommandBuffer CommandBuffer, VkDescriptorPool DescriptorPool, matrix Modelview, matrix Projection)
{
	if(System==NULL)
		return;

	float *Array=System->ParticleArray;

	if(Array==NULL)
		return;

	uint32_t Count=0;

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		for(uint32_t j=0;j<Emitter->NumParticles;j++)
		{
			// Only draw ones that are alive still
			if(Emitter->Particles[j].life>0.0f)
			{
				vec3 Color;

				*Array++=Emitter->Particles[j].pos[0];
				*Array++=Emitter->Particles[j].pos[1];
				*Array++=Emitter->Particles[j].pos[2];
				*Array++=Emitter->ParticleSize;
				Vec3_Lerp(Emitter->StartColor, Emitter->EndColor, Emitter->Particles[j].life, Color);
				*Array++=Color[0];
				*Array++=Color[1];
				*Array++=Color[2];
				*Array++=Emitter->Particles[j].life;

				Count++;
			}
		}
	}

	MatrixMult(Modelview, Projection, ParticlePC.mvp);
	Vec4_Set(ParticlePC.Right, Modelview[0], Modelview[4], Modelview[8], Modelview[12]);
	Vec4_Set(ParticlePC.Up, Modelview[1], Modelview[5], Modelview[9], Modelview[13]);

	vkuDescriptorSet_UpdateBindingImageInfo(&ParticleDescriptorSet, 0, &ParticleTexture);
	vkuAllocateUpdateDescriptorSet(&ParticleDescriptorSet, DescriptorPool);

	vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ParticlePipelineLayout, 0, 1, &ParticleDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ParticlePipeline.Pipeline);
	vkCmdPushConstants(CommandBuffer, ParticlePipelineLayout, VK_SHADER_STAGE_GEOMETRY_BIT, 0, sizeof(ParticlePC), &ParticlePC);

	vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &System->ParticleBuffer.Buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(CommandBuffer, Count, 1, 0, 0);
}

void ParticleSystem_Destroy(ParticleSystem_t *System)
{
	if(System==NULL)
		return;

	vkuDestroyBuffer(&Context, &System->ParticleBuffer);

	vkuDestroyImageBuffer(&Context, &ParticleTexture);

	vkDestroyPipeline(Context.Device, ParticlePipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ParticlePipelineLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorSetLayout(Context.Device, ParticleDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	for(uint32_t i=0;i<List_GetCount(&System->Emitters);i++)
	{
		ParticleEmitter_t *Emitter=List_GetPointer(&System->Emitters, i);

		Zone_Free(Zone, Emitter->Particles);
	}

	List_Destroy(&System->Emitters);
}