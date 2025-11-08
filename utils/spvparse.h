#ifndef __SPVPARSE_H__
#define __SPVPARSE_H__

#include "spirv.h"

#define SPV_REFLECT_MAX_BINDINGS 64
#define SPV_REFLECT_MAX_MEMBERS 64
#define SPV_REFLECT_MAX_NAME 32

typedef enum
{
	SPV_RESOURCE_UNIFORM_BUFFER,
	SPV_RESOURCE_STORAGE_BUFFER,
	SPV_RESOURCE_SAMPLED_IMAGE,
	SPV_RESOURCE_STORAGE_IMAGE,
	SPV_RESOURCE_SAMPLER,
	SPV_RESOURCE_PUSH_CONSTANT
} SpvResourceType_t;

typedef struct
{
	char name[SPV_REFLECT_MAX_NAME];
	uint32_t offset;
	uint32_t sizeBytes;
	SpvOp typeOp;
	uint32_t arrayCount;
} SpvStructMember_t;

typedef struct
{
	uint32_t set;
	uint32_t binding;
	SpvResourceType_t type;

	SpvOp typeOp;

	uint32_t sizeBytes;

	char name[SPV_REFLECT_MAX_NAME];

	uint32_t memberCount;
	SpvStructMember_t members[SPV_REFLECT_MAX_MEMBERS];
} SpvResourceBinding_t;

typedef struct
{
	uint32_t numBindings;
	SpvResourceBinding_t bindings[SPV_REFLECT_MAX_BINDINGS];
} SpvReflectionInfo_t;

// parseSpv: SPIRV = pointer to uint32 opcode data, size = code size in bytes, reflOut = reflection information (optional)
bool parseSpv(const uint32_t *opCodes, uint32_t codeSize, SpvReflectionInfo_t *reflOut);
void spvReflectDump(const SpvReflectionInfo_t *refl);

#endif
