#include "math.h"

#ifndef VEC_INLINE
const vec2 Vec2(const float x, const float y)
{
	return (vec2) { x, y };
}

const vec2 Vec2b(const float b)
{
	return (vec2) { b, b };
}

vec2 Vec2_Add(const vec2 a, const float x, const float y)
{
	return (vec2) { a.x+x, a.y+y };
}

vec2 Vec2_Addv(const vec2 a, const vec2 b)
{
	return (vec2) { a.x+b.x, a.y+b.y };
}

vec2 Vec2_Adds(const vec2 a, const float b)
{
	return (vec2) { a.x+b, a.y+b };
}

vec2 Vec2_Sub(const vec2 a, const float x, const float y)
{
	return (vec2) { a.x-x, a.y-y };
}

vec2 Vec2_Subv(const vec2 a, const vec2 b)
{
	return (vec2) { a.x-b.x, a.y-b.y };
}

vec2 Vec2_Subs(const vec2 a, const float b)
{
	return (vec2) { a.x-b, a.y-b };
}

vec2 Vec2_Mul(const vec2 a, const float x, const float y)
{
	return (vec2) { a.x*x, a.y*y };
}

vec2 Vec2_Mulv(const vec2 a, const vec2 b)
{
	return (vec2) { a.x*b.x, a.y*b.y };
}

vec2 Vec2_Muls(const vec2 a, const float b)
{
	return (vec2) { a.x*b, a.y*b }; 
}

float Vec2_Dot(const vec2 a, const vec2 b)
{
	vec2 mult=Vec2_Mulv(a, b);
	return mult.x+mult.y;
}

float Vec2_Length(const vec2 Vector)
{
	return sqrtf(Vec2_Dot(Vector, Vector));
}

float Vec2_Distance(const vec2 Vector1, const vec2 Vector2)
{
	return Vec2_Length(Vec2_Subv(Vector2, Vector1));
}

float Vec2_DistanceSq(const vec2 v0, const vec2 v1)
{
	return (v0.x-v1.x)*(v0.x-v1.x)+(v0.y-v1.y)*(v0.y-v1.y);
}

vec2 Vec2_Reflect(const vec2 N, const vec2 I)
{
	return Vec2_Subv(I, Vec2_Muls(N, 2.0f*Vec2_Dot(N, I)));
}

vec2 Vec2_Lerp(const vec2 a, const vec2 b, const float t)
{
	return Vec2_Addv(Vec2_Muls(Vec2_Subv(b, a), t), a);
}

vec2 Vec2_Clamp(const vec2 v, const float min, const float max)
{
	return (vec2)
	{
		fminf(fmaxf(v.x, min), max),
		fminf(fmaxf(v.y, min), max)
	};
}

vec2 Vec2_Clampv(const vec2 v, const vec2 min, const vec2 max)
{
	return (vec2)
	{
		fminf(fmaxf(v.x, min.x), max.x),
		fminf(fmaxf(v.y, min.y), max.y)
	};
}
#endif

float Vec2_Normalize(vec2 *v)
{
	if(v)
	{
		float length=sqrtf(Vec2_Dot(*v, *v));

		if(length)
		{
			*v=Vec2_Muls(*v, 1.0f/length);
			return length;
		}
	}

	return 0.0f;
}
