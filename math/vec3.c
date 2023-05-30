#include "math.h"

void Vec3_Set(vec3 *a, const float x, const float y, const float z)
{
	a->value=_mm_set_ps(0, z, y, x);
}

void Vec3_Setv(vec3 *a, const vec3 b)
{
	a->value=b.value;
}

void Vec3_Sets(vec3 *a, const float b)
{
	a->value=_mm_set_ps(0, b, b, b);
}

void Vec3_Add(vec3 *a, const float x, const float y, const float z)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(0, z, y, x));
}

void Vec3_Addv(vec3 *a, const vec3 b)
{
	a->value=_mm_add_ps(a->value, b.value);
}

void Vec3_Adds(vec3 *a, const float b)
{
	a->value=_mm_add_ps(a->value, _mm_set_ps(0, b, b, b));
}

void Vec3_Sub(vec3 *a, const float x, const float y, const float z)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(0, z, y, x));
}

void Vec3_Subv(vec3 *a, const vec3 b)
{
	a->value=_mm_sub_ps(a->value, b.value);
}

void Vec3_Subs(vec3 *a, const float b)
{
	a->value=_mm_sub_ps(a->value, _mm_set_ps(0, b, b, b));
}

void Vec3_Mul(vec3 *a, const float x, const float y, const float z)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(0, z, y, x));
}

void Vec3_Mulv(vec3 *a, const vec3 b)
{
	a->value=_mm_mul_ps(a->value, b.value);
}

void Vec3_Muls(vec3 *a, const float b)
{
	a->value=_mm_mul_ps(a->value, _mm_set_ps(0, b, b, b));
}

float Vec3_Dot(const vec3 a, const vec3 b)
{
	__m128 x0=_mm_mul_ps(a.value, b.value);
	__m128 x1=_mm_hadd_ps(x0, x0);

	return _mm_cvtss_f32(_mm_hadd_ps(x1, x1));
//	return a.x*b.x+a.y*b.y+a.z*b.z;
}

float Vec3_Length(const vec3 Vector)
{
	return sqrtf(Vec3_Dot(Vector, Vector));
}

float Vec3_Distance(const vec3 Vector1, const vec3 Vector2)
{
	vec3 Vector;
	Vec3_Setv(&Vector, Vector2);
	Vec3_Subv(&Vector, Vector1);

	return Vec3_Length(Vector);
}

float Vec3_GetAngle(const vec3 Vector1, const vec3 Vector2)
{
	return acosf(Vec3_Dot(Vector1, Vector2)/(Vec3_Length(Vector1)*Vec3_Length(Vector2)));
}

void Vec3_Reflect(const vec3 N, const vec3 I, vec3 *Result)
{
	if(Result)
	{
		float NdotI=2.0f*Vec3_Dot(N, I);

		Result->x=I.x-NdotI*N.x;
		Result->y=I.y-NdotI*N.y;
		Result->z=I.z-NdotI*N.z;
	}
}

float Vec3_Normalize(vec3 *v)
{
	if(v)
	{
		float length=Vec3_Length(*v);

		if(length)
		{
			float r=1.0f/length;

			Vec3_Muls(v, r);
		}
	}

	return 0.0f;
}

void Vec3_Cross(const vec3 v0, const vec3 v1, vec3 *n)
{
	if(!n)
		return;

	__m128 tmp0, tmp1, tmp2;

	tmp0=_mm_shuffle_ps(v1.value, v1.value, _MM_SHUFFLE(3, 0, 2, 1));
	tmp1=_mm_shuffle_ps(v0.value, v0.value, _MM_SHUFFLE(3, 0, 2, 1));

	tmp0=_mm_mul_ps(tmp0, v0.value);
	tmp1=_mm_mul_ps(tmp1, v1.value);
	tmp2=_mm_sub_ps(tmp0, tmp1);

	n->value=_mm_shuffle_ps(tmp2, tmp2, _MM_SHUFFLE(3, 0, 2, 1));
	//n->x=v0.y*v1.z-v0.z*v1.y;
	//n->y=v0.z*v1.x-v0.x*v1.z;
	//n->z=v0.x*v1.y-v0.y*v1.x;
}

void Vec3_Lerp(const vec3 a, const vec3 b, const float t, vec3 *out)
{
	if(out)
	{
		out->x=t*(b.x-a.x)+a.x;
		out->y=t*(b.y-a.y)+a.y;
		out->z=t*(b.z-a.z)+a.z;
	}
}
