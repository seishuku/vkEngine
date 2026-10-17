#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "entitylist.h"
#include "precorder.h"

#define MAX_RECORD_ENTITIES 4096
#define MAX_RECORD_CONTACTS 8192

#define STDIO_BUFFER_SIZE (1u<<20) // 1MB

static FILE *recordFile=NULL;
static bool enabled=false;
static char stdioBuffer[STDIO_BUFFER_SIZE];

static uint32_t curFrameIndex=0;
static Entity_t scratchEntities[MAX_RECORD_ENTITIES];
static uint32_t scratchEntityCount=0;
static ContactPoint_t scratchContacts[MAX_RECORD_CONTACTS];
static uint32_t scratchContactCount=0;

static uint64_t *frameOffsets=NULL;
static uint32_t frameOffsetCount=0;
static uint32_t frameOffsetCapacity=0;

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
	WriteU32(3); // version
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

void PhysicsRecorder_LogEntity(Entity_t *entity)
{
	if(!PhysicsRecorder_IsEnabled()||scratchEntityCount>=MAX_RECORD_ENTITIES)
		return;

	Entity_t *e=&scratchEntities[scratchEntityCount++];
	*e=*entity;
}

void PhysicsRecorder_LogContact(ContactPoint_t *contact)
{
	if(!PhysicsRecorder_IsEnabled()||scratchContactCount>=MAX_RECORD_CONTACTS)
		return;

	ContactPoint_t *c=&scratchContacts[scratchContactCount++];
	*c=*contact;
}

void PhysicsRecorder_EndFrame(void)
{
	if(!PhysicsRecorder_IsEnabled())
		return;

	WriteU32(curFrameIndex);

	WriteU32(scratchEntityCount);
	for(uint32_t i=0;i<scratchEntityCount;i++)
	{
		Entity_t *e=&scratchEntities[i];
		WriteU32(e->ID);
		WriteU8(e->objectType);
		WriteU8(e->body->type);
		WriteVec3(e->body->position);
		WriteVec4(e->body->orientation);
		WriteVec3(e->body->size);
		WriteVec3(e->body->angularVelocity);
	}

	WriteU32(scratchContactCount);
	for(uint32_t i=0;i<scratchContactCount;i++)
	{
		ContactPoint_t *c=&scratchContacts[i];
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
