#include "math.h"

#ifndef VEC_INLINE
const vec3 Vec3(const float x, const float y, const float z)
{
	return (vec3) { x, y, z };
}

const vec3 Vec3_Vec2(const vec2 a, const float z)
{
	return (vec3) { .x=a.x, .y=a.y, .z=z };
}

const vec3 Vec3b(const float b)
{
	return (vec3) { b, b, b };
}

vec3 Vec3_Add(const vec3 a, const float x, const float y, const float z)
{
	return (vec3) { a.x+x, a.y+y, a.z+z };
}

vec3 Vec3_Addv(const vec3 a, const vec3 b)
{
	return (vec3) { a.x+b.x, a.y+b.y, a.z+b.z };
}

vec3 Vec3_Adds(const vec3 a, const float b)
{
	return (vec3) { a.x+b, a.y+b, a.z+b };
}

vec3 Vec3_Sub(const vec3 a, const float x, const float y, const float z)
{
	return (vec3) { a.x+x, a.y+y, a.z+z };
}

vec3 Vec3_Subv(const vec3 a, const vec3 b)
{
	return (vec3) { a.x-b.x, a.y-b.y, a.z-b.z };
}

vec3 Vec3_Subs(const vec3 a, const float b)
{
	return (vec3) { a.x-b, a.y-b, a.z-b };
}

vec3 Vec3_Mul(const vec3 a, const float x, const float y, const float z)
{
	return (vec3) { a.x*x, a.y*y, a.z*z };
}

vec3 Vec3_Mulv(const vec3 a, const vec3 b)
{
	return (vec3) { a.x*b.x, a.y*b.y, a.z*b.z };
}

vec3 Vec3_Muls(const vec3 a, const float b)
{
	return (vec3) { a.x*b, a.y*b, a.z*b };
}

float Vec3_Dot(const vec3 a, const vec3 b)
{
	vec3 mult=Vec3_Mulv(a, b);
	return mult.x+mult.y+mult.z;
}

float Vec3_Length(const vec3 Vector)
{
	return sqrtf(Vec3_Dot(Vector, Vector));
}

float Vec3_LengthSq(const vec3 Vector)
{
	return Vec3_Dot(Vector, Vector);
}

float Vec3_Distance(const vec3 Vector1, const vec3 Vector2)
{
	return Vec3_Length(Vec3_Subv(Vector2, Vector1));
}

float Vec3_DistanceSq(const vec3 v0, const vec3 v1)
{
	return (v0.x-v1.x)*(v0.x-v1.x)+(v0.y-v1.y)*(v0.y-v1.y)+(v0.z-v1.z)*(v0.z-v1.z);
}

float Vec3_GetAngle(const vec3 Vector1, const vec3 Vector2)
{
	return acosf(Vec3_Dot(Vector1, Vector2)/(Vec3_Length(Vector1)*Vec3_Length(Vector2)));
}

vec3 Vec3_Reflect(const vec3 N, const vec3 I)
{
	return Vec3_Subv(I, Vec3_Muls(N, 2.0f*Vec3_Dot(N, I)));
}

vec3 Vec3_Cross(const vec3 v0, const vec3 v1)
{
	return (vec3)
	{
		v0.y*v1.z-v0.z*v1.y,
		v0.z*v1.x-v0.x*v1.z,
		v0.x*v1.y-v0.y*v1.x
	};
}

vec3 Vec3_Lerp(const vec3 a, const vec3 b, const float t)
{
	return Vec3_Addv(Vec3_Muls(Vec3_Subv(b, a), t), a);
}

vec3 Vec3_Clamp(const vec3 v, const float min, const float max)
{
	return (vec3)
	{
		fminf(fmaxf(v.x, min), max),
		fminf(fmaxf(v.y, min), max),
		fminf(fmaxf(v.z, min), max)
	};
}

vec3 Vec3_Clampv(const vec3 v, const vec3 min, const vec3 max)
{
	return (vec3)
	{
		fminf(fmaxf(v.x, min.x), max.x),
		fminf(fmaxf(v.y, min.y), max.y),
		fminf(fmaxf(v.z, min.z), max.z)
	};
}
#endif

float Vec3_Normalize(vec3 *v)
{
	if(v)
	{
		float length=Vec3_Length(*v);

		if(length)
		{
			*v=Vec3_Muls(*v, 1.0f/length);
			return length;
		}
	}

	return 0.0f;
}
