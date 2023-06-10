#include "math.h"

vec4 Vec4(const float x, const float y, const float z, const float w)
{
	return (vec4) { x, y, z, w };
}

vec4 Vec4b(const float b)
{
	return (vec4) { b, b, b, b };
}

vec4 Vec4_Add(const vec4 a, const float x, const float y, const float z, const float w)
{
	return (vec4) { a.x+x, a.y+y, a.z+z, a.w+w };
}

vec4 Vec4_Addv(const vec4 a, const vec4 b)
{
	return (vec4) { a.x+b.x, a.y+b.y, a.z+b.z, a.w+b.w };
}

vec4 Vec4_Adds(const vec4 a, const float b)
{
	return (vec4) { a.x+b, a.y+b, a.z+b, a.w+b };
}

vec4 Vec4_Sub(const vec4 a, const float x, const float y, const float z, const float w)
{
	return (vec4) { a.x-x, a.y-y, a.z-z, a.w-w };
}

vec4 Vec4_Subv(const vec4 a, const vec4 b)
{
	return (vec4) { a.x-b.x, a.y-b.y, a.z-b.z, a.w-b.w };
}

vec4 Vec4_Subs(const vec4 a, const float b)
{
	return (vec4) { a.x-b, a.y-b, a.z-b, a.w-b };
}

vec4 Vec4_Mul(const vec4 a, const float x, const float y, const float z, const float w)
{
	return (vec4) { a.x*x, a.y*y, a.z*z, a.w*w };
}

vec4 Vec4_Mulv(const vec4 a, const vec4 b)
{
	return (vec4) { a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w };
}

vec4 Vec4_Muls(const vec4 a, const float b)
{
	return (vec4) { a.x*b, a.y*b, a.z*b, a.w*b };
}

float Vec4_Dot(const vec4 a, const vec4 b)
{
	vec4 mult=Vec4_Mulv(a, b);
	return mult.x+mult.y+mult.z+mult.w;
}

float Vec4_Length(const vec4 Vector)
{
	return sqrtf(Vec4_Dot(Vector, Vector));
}

float Vec4_Distance(const vec4 Vector1, const vec4 Vector2)
{
	return Vec4_Length(Vec4_Subv(Vector2, Vector1));
}

vec4 Vec4_Reflect(const vec4 N, const vec4 I)
{
	return Vec4_Subv(I, Vec4_Muls(N, 2.0f*Vec4_Dot(N, I)));
}

float Vec4_Normalize(vec4 *v)
{
	if(v)
	{
		float length=rsqrtf(Vec4_Dot(*v, *v));

		if(length)
		{
			*v=Vec4_Muls(*v, length);
			return length;
		}
	}

	return 0.0f;
}

vec4 Vec4_Lerp(const vec4 a, const vec4 b, const float t)
{
	return Vec4_Addv(Vec4_Muls(Vec4_Subv(b, a), t), a);
}

vec4 Vec4_Clamp(const vec4 v, const float min, const float max)
{
	return (vec4)
	{
		min(max(v.x, min), max),
		min(max(v.y, min), max),
		min(max(v.z, min), max),
		min(max(v.w, min), max)
	};
}
