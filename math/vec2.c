#include "math.h"

void Vec2_Set(vec2 *a, const float x, const float y)
{
	a->value=_mm_set_ps(0, 0, y, x);
}

void Vec2_Setv(vec2 *a, const vec2 b)
{
	a->value=b.value;
}

void Vec2_Sets(vec2 *a, const float b)
{
	a->value=_mm_set_ps(0, 0, b, b);
}

void Vec2_Add(vec2 *a, const float x, const float y)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(0, 0, y, x));
}

void Vec2_Addv(vec2 *a, const vec2 b)
{
	a->value=_mm_add_ps(a->value, b.value);
}

void Vec2_Adds(vec2 *a, const float b)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(0, 0, b, b));
}

void Vec2_Sub(vec2 *a, const float x, const float y)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(0, 0, y, x));
}

void Vec2_Subv(vec2 *a, const vec2 b)
{
	a->value=_mm_sub_ps(a->value, b.value);
}

void Vec2_Subs(vec2 *a, const float b)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(0, 0, b, b));
}

void Vec2_Mul(vec2 *a, const float x, const float y)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(0, 0, y, x));
}

void Vec2_Mulv(vec2 *a, const vec2 b)
{
	a->value=_mm_mul_ps(a->value, b.value);
}

void Vec2_Muls(vec2 *a, const float b)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(0, 0, b, b));
}

float Vec2_Dot(const vec2 a, const vec2 b)
{
	__m128 x0=_mm_mul_ps(a.value, b.value);
	__m128 x1=_mm_hadd_ps(x0, x0);

	return _mm_cvtss_f32(_mm_hadd_ps(x1, x1));
//	return a.x*b.x+a.y*b.y;
}

float Vec2_Length(const vec2 Vector)
{
	return sqrtf(Vec2_Dot(Vector, Vector));
}

float Vec2_Distance(const vec2 Vector1, const vec2 Vector2)
{
	vec2 Vector;
	Vec2_Setv(&Vector, Vector2);
	Vec2_Subv(&Vector, Vector1);

	return Vec2_Length(Vector);
}

void Vec2_Reflect(const vec2 N, const vec2 I, vec2 *Result)
{
	if(Result)
	{
		float NdotI=2.0f*Vec2_Dot(N, I);

		Result->x=I.x-NdotI*N.x;
		Result->y=I.y-NdotI*N.y;
	}
}

float Vec2_Normalize(vec2 *v)
{
	if(v)
	{
		float length=Vec2_Length(*v);

		if(length)
		{
			float r=1.0f/length;

			Vec2_Muls(v, r);
		}

		return length;
	}

	return 0.0f;
}

void Vec2_Lerp(const vec2 a, const vec2 b, const float t, vec2 *out)
{
	if(out)
	{
		out->x=t*(b.x-a.x)+a.x;
		out->y=t*(b.y-a.y)+a.y;
	}
}
