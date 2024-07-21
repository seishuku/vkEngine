#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <stdint.h>
#include "../math/math.h"

static inline void Serialize_uint32(uint8_t **buffer, uint32_t val)
{
	*((uint32_t *)*buffer)=val;
	*buffer+=sizeof(uint32_t);
}

static inline uint32_t Deserialize_uint32(uint8_t **buffer)
{
	uint32_t val=*((uint32_t *)*buffer);
	*buffer+=sizeof(uint32_t);

	return val;
}

static inline void Serialize_float(uint8_t **buffer, float val)
{
	*((float *)*buffer)=val;
	*buffer+=sizeof(float);
}

static inline float Deserialize_float(uint8_t **buffer)
{
	float val=*((float *)*buffer);
	*buffer+=sizeof(float);

	return val;
}

static inline void Serialize_vec3(uint8_t **buffer, vec3 val)
{
	*((vec3 *)*buffer)=val;
	*buffer+=sizeof(vec3);
}

static inline vec3 Deserialize_vec3(uint8_t **buffer)
{
	vec3 val=*((vec3 *)*buffer);
	*buffer+=sizeof(vec3);

	return val;
}

static inline void Serialize_vec4(uint8_t **buffer, vec4 val)
{
	*((vec4 *)*buffer)=val;
	*buffer+=sizeof(vec4);
}

static inline vec4 Deserialize_vec4(uint8_t **buffer)
{
	vec4 val=*((vec4 *)*buffer);
	*buffer+=sizeof(vec4);

	return val;
}

#endif
