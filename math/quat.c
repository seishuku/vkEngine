#include "math.h"

vec4 QuatAngle(const float angle, const float x, const float y, const float z)
{
	vec3 v={ x, y, z };
	Vec3_Normalize(&v);

	v=Vec3_Muls(v, sinf(angle*0.5f));

	return Vec4(v.x, v.y, v.z, cosf(angle*0.5f));
}

vec4 QuatAnglev(const float angle, const vec3 v)
{
	return QuatAngle(angle, v.x, v.y, v.z);
}

vec4 QuatEuler(const float roll, const float pitch, const float yaw)
{
	float sr=sinf(roll*0.5f);
	float cr=cosf(roll*0.5f);

	float sp=sinf(pitch*0.5f);
	float cp=cosf(pitch*0.5f);

	float sy=sinf(yaw*0.5f);
	float cy=cosf(yaw*0.5f);

	return Vec4(
		cy*sr*cp-sy*cr*sp,
		cy*cr*sp+sy*sr*cp,
		sy*cr*cp-cy*sr*sp,
		cy*cr*cp+sy*sr*sp
	);
}

vec4 QuatMultiply(const vec4 a, const vec4 b)
{
	return Vec4(
		a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
		a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
		a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
		a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z
	);
}

vec4 QuatInverse(const vec4 q)
{
	float invNorm=1.0f/Vec4_Dot(q, q);

	return Vec4(
		q.x*-invNorm,
		q.y*-invNorm,
		q.z*-invNorm,
		q.w*invNorm
	);
}

vec3 QuatRotate(const vec4 q, const vec3 v)
{
	vec4 p=q;
	Vec4_Normalize(&p);

	vec3 u=Vec3(p.x, p.y, p.z);

	return Vec3_Addv(
		Vec3_Muls(Vec3_Cross(u, v), 2.0f*p.w),
		Vec3_Addv(
			Vec3_Muls(u, 2.0f*Vec3_Dot(u, v)),
			Vec3_Muls(v, p.w*p.w-Vec3_Dot(u, u))
		)
	);
}

vec4 QuatSlerp(const vec4 qa, const vec4 qb, const float t)
{
	// Check for out-of range parameter and return edge points if so
	if(t<=0.0)
		return qa;

	if(t>=1.0)
		return qb;

	// Compute "cosine of angle between quaternions" using dot product
	float cosOmega=Vec4_Dot(qa, qb);

	// If negative dot, use -q1.  Two quaternions q and -q represent the same rotation, but may produce different slerp.
	// We chose q or -q to rotate using the acute angle.
	vec4 q1=qb;

	if(cosOmega<0.0f)
	{
		q1.x=-q1.x;
		q1.y=-q1.y;
		q1.z=-q1.z;
		q1.w=-q1.w;
		cosOmega=-cosOmega;
	}

	// Compute interpolation fraction, checking for quaternions almost exactly the same
	float k0, k1;

	if(cosOmega>0.9999f)
	{
		// Very close - just use linear interpolation, which will protect against a divide by zero
		k0=1.0f-t;
		k1=t;
	}
	else
	{
		// Compute the sin of the angle using the trig identity sin^2(omega) + cos^2(omega) = 1
		float sinOmega=sqrtf(1.0f-(cosOmega*cosOmega));

		// Compute the angle from its sine and cosine
		float omega=atan2f(sinOmega, cosOmega);

		// Compute inverse of denominator, so we only have to divide once
		float oneOverSinOmega=1.0f/sinOmega;

		// Compute interpolation parameters
		k0=sinf((1.0f-t)*omega)*oneOverSinOmega;
		k1=sinf(t*omega)*oneOverSinOmega;
	}

	// Interpolate and return new quaternion
	return Vec4_Addv(Vec4_Muls(qa, k0), Vec4_Muls(q1, k1));
}

void QuatAxes(vec4 q, vec3 *axes)
{
	float xx=q.x*q.x;
	float yy=q.y*q.y;
	float zz=q.z*q.z;
	float xy=q.x*q.y;
	float xz=q.x*q.z;
	float yz=q.y*q.z;
	float wx=q.w*q.x;
	float wy=q.w*q.y;
	float wz=q.w*q.z;

	// (column-major order)
	axes[0]=Vec3(1.0f-2.0f*(yy+zz), 2.0f*(xy+wz), 2.0f*(xz-wy));
	axes[1]=Vec3(2.0f*(xy-wz), 1.0f-2.0f*(xx+zz), 2.0f*(yz+wx));
	axes[2]=Vec3(2.0f*(xz+wy), 2.0f*(yz-wx), 1.0f-2.0f*(xx+yy));
}

matrix QuatToMatrix(const vec4 q)
{
	float norm=sqrtf(Vec4_Dot(q, q)), s=0.0f;

	if(norm>0.0f)
		s=2.0f/norm;

	float xx=s*q.x*q.x;
	float xy=s*q.x*q.y;
	float xz=s*q.x*q.z;
	float yy=s*q.y*q.y;
	float yz=s*q.y*q.z;
	float zz=s*q.z*q.z;
	float wx=s*q.w*q.x;
	float wy=s*q.w*q.y;
	float wz=s*q.w*q.z;

	return (matrix)
	{
		{ 1.0f-yy-zz, xy+wz, xz-wy, 0.0f },
		{ xy-wz, 1.0f-xx-zz, yz+wx, 0.0f },
		{ xz+wy, yz-wx, 1.0f-xx-yy, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
}

vec4 MatrixToQuat(const matrix m)
{
	vec4 q;

	float tr=m.x.x+m.y.y+m.z.z;

	if(tr>0.0f)
	{
		float S=sqrtf(tr+1.0f)*2.0f;

		q.x=(m.z.y-m.y.z)/S;
		q.y=(m.x.z-m.z.x)/S;
		q.z=(m.y.x-m.x.y)/S;
		q.w=0.25f*S;
	}
	else if((m.x.x>m.y.y)&(m.x.x>m.z.z))
	{
		float S=sqrtf(1.0f+m.x.x-m.y.y-m.z.z)*2.0f;

		q.x=0.25f*S;
		q.y=(m.x.y+m.y.x)/S;
		q.z=(m.x.z+m.z.x)/S;
		q.w=(m.z.y-m.y.z)/S;
	}
	else if(m.y.y>m.z.z)
	{
		float S=sqrtf(1.0f+m.y.y-m.x.x-m.z.z)*2.0f;

		q.x=(m.x.y+m.y.x)/S;
		q.y=0.25f*S;
		q.z=(m.y.z+m.z.y)/S;
		q.w=(m.x.z-m.z.x)/S;
	}
	else
	{
		float S=sqrtf(1.0f+m.z.z-m.x.x-m.y.y)*2.0f;

		q.x=(m.x.z+m.z.x)/S;
		q.y=(m.y.z+m.z.y)/S;
		q.z=0.25f*S;
		q.w=(m.y.x-m.x.y)/S;
	}

	return q;
}
