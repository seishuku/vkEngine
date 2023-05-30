#include "math.h"

void Vec4_Set(vec4 *a, const float x, const float y, const float z, const float w)
{
	a->value=_mm_set_ps(w, z, y, x);
}

void Vec4_Setv(vec4 *a, const vec4 b)
{
	a->value=b.value;
}

void Vec4_Sets(vec4 *a, const float b)
{
	a->value=_mm_set_ps(b, b, b, b);
}

void Vec4_Add(vec4 *a, const float x, const float y, const float z, const float w)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(w, z, y, x));
}

void Vec4_Addv(vec4 *a, const vec4 b)
{
	a->value=_mm_add_ps(a->value, b.value);
}

void Vec4_Adds(vec4 *a, const float b)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(b, b, b, b));
}

void Vec4_Sub(vec4 *a, const float x, const float y, const float z, const float w)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(w, z, y, x));
}

void Vec4_Subv(vec4 *a, const vec4 b)
{
	a->value=_mm_sub_ps(a->value, b.value);
}

void Vec4_Subs(vec4 *a, const float b)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(b, b, b, b));
}

void Vec4_Mul(vec4 *a, const float x, const float y, const float z, const float w)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(w, z, y, x));
}

void Vec4_Mulv(vec4 *a, const vec4 b)
{
	a->value=_mm_mul_ps(a->value, b.value);
}

void Vec4_Muls(vec4 *a, const float b)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(b, b, b, b));
}

float Vec4_Dot(const vec4 a, const vec4 b)
{
	__m128 x0=_mm_mul_ps(a.value, b.value);
	__m128 x1=_mm_hadd_ps(x0, x0);

	return _mm_cvtss_f32(_mm_hadd_ps(x1, x1));
}

float Vec4_Length(const vec4 Vector)
{
	return sqrtf(Vec4_Dot(Vector, Vector));
}

float Vec4_Distance(const vec4 Vector1, const vec4 Vector2)
{
	vec4 Vector;
	Vec4_Setv(&Vector, Vector2);
	Vec4_Subv(&Vector, Vector1);

	return Vec4_Length(Vector);
}

float Vec4_Normalize(vec4 *v)
{
	if(v)
	{
		float length=Vec4_Length(*v);

		if(length)
		{
			float r=1.0f/length;

			Vec4_Muls(v, r);
		}

		return length;
	}

	return 0.0f;
}

void Vec4_Lerp(const vec4 a, const vec4 b, const float t, vec4 *out)
{
	if(out)
	{
		out->x=t*(b.x-a.x)+a.x;
		out->y=t*(b.y-a.y)+a.y;
		out->z=t*(b.z-a.z)+a.z;
		out->w=t*(b.w-a.w)+a.w;
	}
}
