#ifndef __PARTICLE_H__
#define __PARTICLE_H__

typedef struct
{
	uint32_t ID;
	float life;
	vec3 pos, vel;
} Particle_t;

typedef void (*ParticleInitCallback)(uint32_t Index, uint32_t NumParticles, Particle_t *Particle);

typedef struct
{
	uint32_t ID;
	bool Burst;
	vec3 Position;
	vec3 StartColor, EndColor;
	float ParticleSize;
	uint32_t NumParticles;
	Particle_t *Particles;

	ParticleInitCallback InitCallback;
} ParticleEmitter_t;

typedef struct
{
	List_t Emitters;

	VkuBuffer_t ParticleBuffer;
	float *ParticleArray;
} ParticleSystem_t;

uint32_t ParticleSystem_AddEmitter(ParticleSystem_t *System, vec3 Position, vec3 StartColor, vec3 EndColor, float ParticleSize, uint32_t NumParticles, bool Burst, ParticleInitCallback InitCallback);
void ParticleSystem_DeleteEmitter(ParticleSystem_t *System, uint32_t ID);
void ParticleSystem_ResetEmitter(ParticleSystem_t *System, uint32_t ID);
void ParticleSystem_SetEmitterPosition(ParticleSystem_t *System, uint32_t ID, vec3 Position);

bool ParticleSystem_Init(ParticleSystem_t *System);
void ParticleSystem_Step(ParticleSystem_t *System, float dt);
void ParticleSystem_Draw(ParticleSystem_t *System, VkCommandBuffer CommandBuffer, VkDescriptorPool DescriptorPool, matrix Modelview, matrix Projection);
void ParticleSystem_Destroy(ParticleSystem_t *System);

#endif