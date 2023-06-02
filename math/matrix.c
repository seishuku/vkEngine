#include "math.h"

void MatrixIdentity(matrix out)
{
	if(out)
	{
		out[0]=1.0f;	out[1]=0.0f;	out[2]=0.0f;	out[3]=0.0f;
		out[4]=0.0f;	out[5]=1.0f;	out[6]=0.0f;	out[7]=0.0f;
		out[8]=0.0f;	out[9]=0.0f;	out[10]=1.0f;	out[11]=0.0f;
		out[12]=0.0f;	out[13]=0.0f;	out[14]=0.0f;	out[15]=1.0f;
	}
}

void MatrixMult(const matrix a, const matrix b, matrix out)
{
	matrix res;

	if(!out)
		return;

	res[ 0]=a[ 0]*b[ 0]+a[ 1]*b[ 4]+a[ 2]*b[ 8]+a[ 3]*b[12];
	res[ 1]=a[ 0]*b[ 1]+a[ 1]*b[ 5]+a[ 2]*b[ 9]+a[ 3]*b[13];
	res[ 2]=a[ 0]*b[ 2]+a[ 1]*b[ 6]+a[ 2]*b[10]+a[ 3]*b[14];
	res[ 3]=a[ 0]*b[ 3]+a[ 1]*b[ 7]+a[ 2]*b[11]+a[ 3]*b[15];
	res[ 4]=a[ 4]*b[ 0]+a[ 5]*b[ 4]+a[ 6]*b[ 8]+a[ 7]*b[12];
	res[ 5]=a[ 4]*b[ 1]+a[ 5]*b[ 5]+a[ 6]*b[ 9]+a[ 7]*b[13];
	res[ 6]=a[ 4]*b[ 2]+a[ 5]*b[ 6]+a[ 6]*b[10]+a[ 7]*b[14];
	res[ 7]=a[ 4]*b[ 3]+a[ 5]*b[ 7]+a[ 6]*b[11]+a[ 7]*b[15];
	res[ 8]=a[ 8]*b[ 0]+a[ 9]*b[ 4]+a[10]*b[ 8]+a[11]*b[12];
	res[ 9]=a[ 8]*b[ 1]+a[ 9]*b[ 5]+a[10]*b[ 9]+a[11]*b[13];
	res[10]=a[ 8]*b[ 2]+a[ 9]*b[ 6]+a[10]*b[10]+a[11]*b[14];
	res[11]=a[ 8]*b[ 3]+a[ 9]*b[ 7]+a[10]*b[11]+a[11]*b[15];
	res[12]=a[12]*b[ 0]+a[13]*b[ 4]+a[14]*b[ 8]+a[15]*b[12];
	res[13]=a[12]*b[ 1]+a[13]*b[ 5]+a[14]*b[ 9]+a[15]*b[13];
	res[14]=a[12]*b[ 2]+a[13]*b[ 6]+a[14]*b[10]+a[15]*b[14];
	res[15]=a[12]*b[ 3]+a[13]*b[ 7]+a[14]*b[11]+a[15]*b[15];

	memcpy(out, res, sizeof(matrix));
}

void MatrixInverse(const matrix in, matrix out)
{
	matrix res;

	if(!out)
		return;

	res[ 0]=in[ 0];
	res[ 1]=in[ 4];
	res[ 2]=in[ 8];
	res[ 3]=0.0f;
	res[ 4]=in[ 1];
	res[ 5]=in[ 5];
	res[ 6]=in[ 9];
	res[ 7]=0.0f;
	res[ 8]=in[ 2];
	res[ 9]=in[ 6];
	res[10]=in[10];
	res[11]=0.0f;
	res[12]=-(in[12]*in[ 0])-(in[13]*in[ 1])-(in[14]*in[ 2]);
	res[13]=-(in[12]*in[ 4])-(in[13]*in[ 5])-(in[14]*in[ 6]);
	res[14]=-(in[12]*in[ 8])-(in[13]*in[ 9])-(in[14]*in[10]);
	res[15]=1.0f;

	memcpy(out, res, sizeof(matrix));
}

void MatrixTranspose(const matrix in, matrix out)
{
	matrix res;

	if(!out)
		return;

	res[ 0]=in[ 0];
	res[ 1]=in[ 4];
	res[ 2]=in[ 8];
	res[ 3]=in[12];
	res[ 4]=in[ 1];
	res[ 5]=in[ 5];
	res[ 6]=in[ 9];
	res[ 7]=in[13];
	res[ 8]=in[ 2];
	res[ 9]=in[ 6];
	res[10]=in[10];
	res[11]=in[14];
	res[12]=in[ 3];
	res[13]=in[ 7];
	res[14]=in[11];
	res[15]=in[15];

	memcpy(out, res, sizeof(matrix));
}

void MatrixRotate(const float angle, const float x, const float y, const float z, matrix out)
{
	if(out)
	{
		matrix m;
		float c=cosf(angle);
		float s=sinf(angle);

		float temp[3]={ (1.0f-c)*x, (1.0f-c)*y, (1.0f-c)*z };

		m[0]=c+temp[0]*x;
		m[1]=temp[0]*y+s*z;
		m[2]=temp[0]*z-s*y;
		m[3]=0.0f;
		m[4]=temp[1]*x-s*z;
		m[5]=c+temp[1]*y;
		m[6]=temp[1]*z+s*x;
		m[7]=0.0f;
		m[8]=temp[2]*x+s*y;
		m[9]=temp[2]*y-s*x;
		m[10]=c+temp[2]*z;
		m[11]=0.0f;
		m[12]=0.0f;
		m[13]=0.0f;
		m[14]=0.0f;
		m[15]=1.0f;

		MatrixMult(m, out, out);
	}
}

void MatrixRotatev(const float angle, const vec3 v, matrix out)
{
	MatrixRotate(angle, v.x, v.y, v.z, out);
}

void MatrixTranslate(const float x, const float y, const float z, matrix out)
{
	if(out)
	{
		matrix m=
		{
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			   x,    y,    z, 1.0f
		};

		MatrixMult(m, out, out);
	}
}

void MatrixTranslatev(const vec3 v, matrix out)
{
	MatrixTranslate(v.x, v.y, v.z, out);
}

void MatrixScale(const float x, const float y, const float z, matrix out)
{
	if(out)
	{
		matrix m=
		{
			   x, 0.0f, 0.0f, 0.0f,
			0.0f,    y, 0.0f, 0.0f,
			0.0f, 0.0f,    z, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		};

		MatrixMult(m, out, out);
	}
}

void MatrixScalev(const vec3 v, matrix out)
{
	MatrixScale(v.x, v.y, v.z, out);
}

void MatrixAlignPoints(const vec3 start, const vec3 end, const vec3 up, matrix out)
{
	if(out)
	{
		// Find the direction of the start point and end point, then normalize it.
		vec3 direction=Vec3_Subv(end, start);
		Vec3_Normalize(&direction);

		// Get the cross product between the direction
		// and the object's current orientation, and normalize that.
		// That vector is the axis of rotation
		vec3 axis=Vec3_Cross(direction, up);
		Vec3_Normalize(&axis);

		// direction.orientation=cos(angle), so arccos to get angle between
		// the new direction and the static orientation.
		float angle=acosf(Vec3_Dot(direction, up));

		// Use that angle to build a rotation and translation matrix to reorient it.
		float s=sinf(angle);
		float c=cosf(angle);
		float c1=1.0f-c;

		matrix m=
		{
			c+axis.x*axis.x*c1,			axis.y*axis.x*c1+axis.z*s,	axis.z*axis.x*c1-axis.y*s,	0.0f,
			axis.x*axis.y*c1-axis.z*s,	c+axis.y*axis.y*c1,			axis.z*axis.y *c1+axis.x*s,	0.0f,
			axis.x*axis.z*c1+axis.y*s,	axis.y*axis.z*c1-axis.x*s,	c+axis.z*axis.z*c1,			0.0f,
			start.x,					start.y,					start.z,					1.0f
		};

		// Multiply that with the current set matrix
		MatrixMult(m, out, out);
	}
}

void Matrix4x4MultVec4(const vec4 in, const matrix m, vec4 *out)
{
	if(out)
	{
		vec4 res=
		{
			in.x*m[ 0]+in.y*m[ 4]+in.z*m[ 8]+in.w*m[12],
			in.x*m[ 1]+in.y*m[ 5]+in.z*m[ 9]+in.w*m[13],
			in.x*m[ 2]+in.y*m[ 6]+in.z*m[10]+in.w*m[14],
			in.x*m[ 3]+in.y*m[ 7]+in.z*m[11]+in.w*m[15]
		};

		memcpy(out, &res, sizeof(vec4));
	}
}

void Matrix4x4MultVec3(const vec3 in, const matrix m, vec3 *out)
{
	if(out)
	{
		vec3 res=
		{
			in.x*m[ 0]+in.y*m[ 4]+in.z*m[ 8]+m[12],
			in.x*m[ 1]+in.y*m[ 5]+in.z*m[ 9]+m[13],
			in.x*m[ 2]+in.y*m[ 6]+in.z*m[10]+m[14]
		};

		memcpy(out, &res, sizeof(vec3));
	}
}

void Matrix3x3MultVec3(const vec3 in, const matrix m, vec3 *out)
{
	if(out)
	{
		vec3 res=
		{
			in.x*m[ 0]+in.y*m[ 4]+in.z*m[ 8],
			in.x*m[ 1]+in.y*m[ 5]+in.z*m[ 9],
			in.x*m[ 2]+in.y*m[ 6]+in.z*m[10]
		};

		memcpy(out, &res, sizeof(vec3));
	}
}

// TODO?: Should this multiply with the supplied matrix like the other functions?
void MatrixLookAt(const vec3 position, const vec3 forward, const vec3 up, matrix out)
{
	if(out)
	{
		vec3 f=Vec3_Subv(forward, position);
		vec3 u=up, s;

		Vec3_Normalize(&u);
		Vec3_Normalize(&f);
		s=Vec3_Cross(f, u);
		Vec3_Normalize(&s);
		u=Vec3_Cross(s, f);

		out[0]=s.x;
		out[1]=u.x;
		out[2]=-f.x;
		out[3]=0.0f;
		out[4]=s.y;
		out[5]=u.y;
		out[6]=-f.y;
		out[7]=0.0f;
		out[8]=s.z;
		out[9]=u.z;
		out[10]=-f.z;
		out[11]=0.0f;
		out[12]=-Vec3_Dot(s, position);
		out[13]=-Vec3_Dot(u, position);
		out[14]=Vec3_Dot(f, position);
		out[15]=1.0f;
	}
}

// Projection matrix functions, these are set up for "z reverse" (depth cleared to 0.0, and greater than or equal depth test)
void MatrixInfPerspective(const float fovy, const float aspect, const float zNear, matrix out)
{
	if(out)
	{
		const float focal=tanf((fovy*PI/180.0f)*0.5f);
		matrix m=
		{
			1.0f/(aspect*focal),  0.0f,        0.0f,  0.0f,
			0.0f,                 -1.0f/focal, 0.0f,  0.0f,
			0.0f,                 0.0f,        0.0f,  -1.0f,
			0.0f,                 0.0f,        zNear, 0.0f
		};

		MatrixMult(m, out, out);
	}
}

void MatrixPerspective(const float fovy, const float aspect, const float zNear, const float zFar, matrix out)
{
	if(out)
	{
		const float focal=tanf((fovy*PI/180.0f)*0.5f);
		matrix m=
		{
			1.0f/(aspect*focal),  0.0f,        0.0f,                       0.0f,
			0.0f,                 -1.0f/focal, 0.0f,                       0.0f,
			0.0f,                 0.0f,        -2.0f/(zFar-zNear),         -1.0f,
			0.0f,                 0.0f,        -(zFar+zNear)/(zFar-zNear), 0.0f,
		};

		MatrixMult(m, out, out);
	}
}

// This is *not* set up for "z reverse"
void MatrixOrtho(const float left, const float right, const float bottom, const float top, const float zNear, const float zFar, matrix out)
{
	if(out)
	{
		matrix m=
		{
			2.0f/(right-left),          0.0f,                       0.0f,               0.0f,
			0.0f,                       2.0f/(bottom-top),          0.0f,               0.0f,
			0.0f,                       0.0f,                       1.0f/(zNear-zFar),  0.0f,
			-(right+left)/(right-left), -(bottom+top)/(bottom-top), zNear/(zNear-zFar), 1.0f
		};

		MatrixMult(m, out, out);
	}
}
