#ifndef __SPVPARSE_H__
#define __SPVPARSE_H__

// parseSpv: SPIRV = pointer to uint32 opcode data, size = code size in bytes
bool parseSpv(uint32_t *SPIRV, uint32_t size);

#endif