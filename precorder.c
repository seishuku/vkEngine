#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "precorder.h"

// Adjust these to comfortably cover your MAX_ENTITY / MAX_MANIFOLDS*MAX_CONTACTS_PER_MANIFOLD
// bounds. Kept independent of those macros so this file doesn't need to pull in entitylist.h.
#define MAX_RECORD_ENTITIES 4096
#define MAX_RECORD_CONTACTS 8192

// A large stdio buffer means most EndFrame calls just memcpy into libc's own buffer rather
// than hitting the OS. If profiling ever shows the physics thread stalling on I/O during a
// flush, the next step is a lock-free queue handing frames to a dedicated writer thread —
// not needed until you can actually measure it costing something.
#define STDIO_BUFFER_SIZE (1u<<20) // 1MB

typedef struct
{
	uint32_t id;
	uint8_t objType;
	uint8_t type;
	vec3 position;
	vec4 orientation;
	vec3 dims;
	vec3 angularVelocity;
} ScratchEntity_t;

typedef struct
{
	uint32_t idA, idB;
	vec3 position;
	vec3 normal;
	float penetration;
} ScratchContact_t;

static FILE *recordFile=NULL;
static bool enabled=false;
static char stdioBuffer[STDIO_BUFFER_SIZE];

static uint32_t curFrameIndex=0;
static ScratchEntity_t scratchEntities[MAX_RECORD_ENTITIES];
static uint32_t scratchEntityCount=0;
static ScratchContact_t scratchContacts[MAX_RECORD_CONTACTS];
static uint32_t scratchContactCount=0;

static uint64_t *frameOffsets=NULL;
static uint32_t frameOffsetCount=0;
static uint32_t frameOffsetCapacity=0;

// --- low-level explicit-field writers (avoids struct padding/ABI issues entirely) ---

static inline void WriteU8(uint8_t v)   { fwrite(&v, sizeof(v), 1, recordFile); }
static inline void WriteU32(uint32_t v) { fwrite(&v, sizeof(v), 1, recordFile); }
static inline void WriteU64(uint64_t v) { fwrite(&v, sizeof(v), 1, recordFile); }
static inline void WriteF32(float v)    { fwrite(&v, sizeof(v), 1, recordFile); }

static inline void WriteVec3(vec3 v) { WriteF32(v.x); WriteF32(v.y); WriteF32(v.z); }
static inline void WriteVec4(vec4 v) { WriteF32(v.x); WriteF32(v.y); WriteF32(v.z); WriteF32(v.w); }

bool PhysicsRecorder_Init(const char *path, float fixedTimestep)
{
	PhysicsRecorder_Shutdown(); // in case a previous recording was left open

	recordFile=fopen(path, "wb");

	if(!recordFile)
		return false;

	setvbuf(recordFile, stdioBuffer, _IOFBF, STDIO_BUFFER_SIZE);

	fwrite("PREC", 1, 4, recordFile);
	WriteU32(2); // version
	WriteF32(fixedTimestep);

	frameOffsetCount=0;
	enabled=true;

	return true;
}

void PhysicsRecorder_SetEnabled(bool e) { enabled=e; }
bool PhysicsRecorder_IsEnabled(void)    { return enabled&&recordFile!=NULL; }

void PhysicsRecorder_BeginFrame(uint32_t frameIndex)
{
	if(!PhysicsRecorder_IsEnabled())
		return;

	curFrameIndex=frameIndex;
	scratchEntityCount=0;
	scratchContactCount=0;

	// Record where this frame will start in the file so playback can seek directly to it.
	if(frameOffsetCount>=frameOffsetCapacity)
	{
		frameOffsetCapacity=frameOffsetCapacity?frameOffsetCapacity*2:1024;
		frameOffsets=realloc(frameOffsets, frameOffsetCapacity*sizeof(uint64_t));
	}

	frameOffsets[frameOffsetCount++]=(uint64_t)ftell(recordFile);
}

void PhysicsRecorder_LogEntity(uint32_t id, uint8_t objType, RigidBodyType_e type, vec3 position, vec4 orientation, vec3 dims, vec3 angularVelocity)
{
	if(!PhysicsRecorder_IsEnabled()||scratchEntityCount>=MAX_RECORD_ENTITIES)
		return;

	ScratchEntity_t *e=&scratchEntities[scratchEntityCount++];
	e->id=id;
	e->objType=objType;
	e->type=(uint8_t)type;
	e->position=position;
	e->orientation=orientation;
	e->dims=dims;
	e->angularVelocity=angularVelocity;
}

void PhysicsRecorder_LogContact(uint32_t idA, uint32_t idB, vec3 position, vec3 normal, float penetration)
{
	if(!PhysicsRecorder_IsEnabled()||scratchContactCount>=MAX_RECORD_CONTACTS)
		return;

	ScratchContact_t *c=&scratchContacts[scratchContactCount++];
	c->idA=idA;
	c->idB=idB;
	c->position=position;
	c->normal=normal;
	c->penetration=penetration;
}

void PhysicsRecorder_EndFrame(void)
{
	if(!PhysicsRecorder_IsEnabled())
		return;

	WriteU32(curFrameIndex);

	WriteU32(scratchEntityCount);
	for(uint32_t i=0;i<scratchEntityCount;i++)
	{
		ScratchEntity_t *e=&scratchEntities[i];
		WriteU32(e->id);
		WriteU8(e->objType);
		WriteU8(e->type);
		WriteVec3(e->position);
		WriteVec4(e->orientation);
		WriteVec3(e->dims);
		WriteVec3(e->angularVelocity);
	}

	WriteU32(scratchContactCount);
	for(uint32_t i=0;i<scratchContactCount;i++)
	{
		ScratchContact_t *c=&scratchContacts[i];
		WriteU32(c->idA);
		WriteU32(c->idB);
		WriteVec3(c->position);
		WriteVec3(c->normal);
		WriteF32(c->penetration);
	}
}

void PhysicsRecorder_Shutdown(void)
{
	if(!recordFile)
		return;

	uint64_t indexOffset=(uint64_t)ftell(recordFile);

	for(uint32_t i=0;i<frameOffsetCount;i++)
		WriteU64(frameOffsets[i]);

	WriteU64(indexOffset);
	WriteU32(frameOffsetCount);

	fclose(recordFile);
	recordFile=NULL;
	enabled=false;

	free(frameOffsets);
	frameOffsets=NULL;
	frameOffsetCount=frameOffsetCapacity=0;
}
