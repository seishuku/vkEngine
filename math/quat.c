#include "math.h"

void QuatAngle(const float angle, const float x, const float y, const float z, vec4 *out)
{
	if(out)
	{
		vec3 v={ x, y, z };
		float s=sinf(angle*0.5f);

		Vec3_Normalize(&v);

		out->x=s*v.x;
		out->y=s*v.y;
		out->z=s*v.z;
		out->w=cosf(angle*0.5f);
	}
}

void QuatAnglev(const float angle, const vec3 v, vec4 *out)
{
	QuatAngle(angle, v.x, v.y, v.z, out);
}

void QuatEuler(const float roll, const float pitch, const float yaw, vec4 *out)
{
	if(out)
	{
		float sr=sinf(roll*0.5f);
		float cr=cosf(roll*0.5f);

		float sp=sinf(pitch*0.5f);
		float cp=cosf(pitch*0.5f);

		float sy=sinf(yaw*0.5f);
		float cy=cosf(yaw*0.5f);

		out->x=cy*sr*cp-sy*cr*sp;
		out->y=cy*cr*sp+sy*sr*cp;
		out->z=sy*cr*cp-cy*sr*sp;
		out->w=cy*cr*cp+sy*sr*sp;
	}
}

void QuatMultiply(const vec4 a, const vec4 b, vec4 *out)
{
	if(out)
	{
		vec4 res=
		{
			a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
			a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
			a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
			a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z
		};

		memcpy(out, &res, sizeof(vec4));
	}
}

void QuatInverse(vec4 *q)
{
	if(q)
	{
		float invNorm=1.0f/Vec4_Dot(*q, *q);

		q->x*=-invNorm;
		q->y*=-invNorm;
		q->z*=-invNorm;
		q->w*=invNorm;
	}
}

void QuatRotate(const vec4 q, const vec3 v, vec3 *out)
{
	if(out)
	{
		vec4 p={ q.x, q.y, q.z, q.w };
		vec3 v2;

		Vec4_Normalize(&p);

		vec3 u={ p.x, p.y, p.z };
		float s=p.w;

		vec3 v1=
		{
			u.x*2.0f*Vec3_Dot(u, v)+v.x*s*s-Vec3_Dot(u, u),
			u.y*2.0f*Vec3_Dot(u, v)+v.y*s*s-Vec3_Dot(u, u),
			u.z*2.0f*Vec3_Dot(u, v)+v.z*s*s-Vec3_Dot(u, u)
		};

		Vec3_Cross(u, v, &v2);

		out->x=v2.x*2.0f*s+v1.x;
		out->y=v2.y*2.0f*s+v1.y;
		out->z=v2.z*2.0f*s+v1.z;
	}
}

void QuatSlerp(const vec4 qa, const vec4 qb, const float t, vec4 *out)
{
	if(out)
	{
		// Check for out-of range parameter and return edge points if so
		if(t<=0.0)
		{
			memcpy(out, &qa, sizeof(vec4));
			return;
		}

		if(t>=1.0)
		{
			memcpy(out, &qb, sizeof(vec4));
			return;
		}

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
			// Very close - just use linear interpolation, which will protect againt a divide by zero

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
		out->x=(k0*qa.x)+(k1*q1.x);
		out->y=(k0*qa.y)+(k1*q1.y);
		out->z=(k0*qa.z)+(k1*q1.z);
		out->w=(k0*qa.w)+(k1*q1.w);
	}
}

void QuatMatrix(const vec4 q, matrix out)
{
	if(out)
	{
		matrix m;
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

		m[0]=1.0f-yy-zz;
		m[1]=xy+wz;
		m[2]=xz-wy;
		m[3]=0.0f;
		m[4]=xy-wz;
		m[5]=1.0f-xx-zz;
		m[6]=yz+wx;
		m[7]=0.0f;
		m[8]=xz+wy;
		m[9]=yz-wx;
		m[10]=1.0f-xx-yy;
		m[11]=0.0f;
		m[12]=0.0f;
		m[13]=0.0f;
		m[14]=0.0f;
		m[15]=1.0f;

		MatrixMult(m, out, out);
	}
}
