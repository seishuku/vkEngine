#include <string.h>
#include "math.h"

float fsinf(float v)
{
	float fx=v*0.1591549f+0.5f;
	float ix=fx-(float)floor(fx);
	float x=ix*6.2831852f-3.1415926f;
	float x2=x*x;
	float x3=x2*x;

	return x-x3/6.0f
			+x3*x2/120.0f
			-x3*x2*x2/5040.0f
			+x3*x2*x2*x2/362880.0f
			-x3*x2*x2*x2*x2/39916800.0f
			+x3*x2*x2*x2*x2*x2/6227020800.0f
			-x3*x2*x2*x2*x2*x2*x2/1307674279936.0f
			+x3*x2*x2*x2*x2*x2*x2*x2/355687414628352.0f;
}

float fcosf(float v)
{
	float fx=v*0.1591549f+0.5f;
	float ix=fx-(float)floor(fx);
	float x=ix*6.2831852f-3.1415926f;
	float x2=x*x;
	float x4=x2*x2;

	return 1-x2/2.0f
			+x4/24.0f
			-x4*x2/720.0f
			+x4*x4/40320.0f
			-x4*x4*x2/3628800.0f
			+x4*x4*x4/479001600.0f
			-x4*x4*x4*x2/87178289152.0f
			+x4*x4*x4*x4/20922788478976.0f;
}

float ftanf(float x)
{
	return fsinf(x)/fcosf(x);
}

// Misc functions
float fact(int n)
{
	int i;
	float j=1.0f;

	for(i=1;i<n;i++)
		j*=i;

	return j;
}

unsigned int NextPower2(unsigned int value)
{
	value--;
	value|=value>>1;
	value|=value>>2;
	value|=value>>4;
	value|=value>>8;
	value|=value>>16;
	value++;

	return value;
}

int ComputeLog(unsigned int value)
{
	int i=0;

	if(value==0)
		return -1;

	for(;;)
	{
		if(value&1)
		{
			if(value!=1)
				return -1;

			return i;
		}

		value>>=1;
		i++;
	}
}

// Vector functions
void Normalize(float *v)
{
	float mag;

	if(!v)
		return;

	mag=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);

	if(mag)
	{
		mag=1.0f/mag;

		v[0]*=mag;
		v[1]*=mag;
		v[2]*=mag;
	}
}

void Cross(float v0[3], float v1[3], float *n)
{
	if(!n)
		return;

	n[0]=v0[1]*v1[2]-v0[2]*v1[1];
	n[1]=v0[2]*v1[0]-v0[0]*v1[2];
	n[2]=v0[0]*v1[1]-v0[1]*v1[0];
}

// Quaternion functions
void QuatAngle(float angle, float x, float y, float z, float *out)
{
	float s, mag;

	if(!out)
		return;

	mag=1.0f/sqrtf(x*x+y*y+z*z);
	x*=mag;
	y*=mag;
	z*=mag;

	angle*=0.017453292f;
	angle*=0.5f;

	s=sinf(angle);

	out[0]=cosf(angle);
	out[1]=x*s;
	out[2]=y*s;
	out[3]=z*s;
}

void QuatEuler(float roll, float pitch, float yaw, float *out)
{
	float cr, cp, cy, sr, sp, sy, cpcy, spsy;

	if(!out)
		return;

	sr=sinf(roll*0.5f);
	cr=cosf(roll*0.5f);

	sp=sinf(pitch*0.5f);
	cp=cosf(pitch*0.5f);

	sy=sinf(yaw*0.5f);
	cy=cosf(yaw*0.5f);

	cpcy=cp*cy;
	spsy=sp*sy;

	out[0]=cr*cpcy+sr*spsy;
	out[1]=sr*cpcy-cr*spsy;
	out[2]=cr*sp*cy+sr*cp*sy;
	out[3]=cr*cp*sy-sr*sp*cy;
}

void QuatMultiply(float a[4], float b[4], float *out)
{
	if(!out)
		return;

	out[0]=b[0]*a[0]-b[1]*a[1]-b[2]*a[2]-b[3]*a[3];
	out[1]=b[0]*a[1]+b[1]*a[0]+b[2]*a[3]-b[3]*a[2];
	out[2]=b[0]*a[2]-b[1]*a[3]+b[2]*a[0]+b[3]*a[1];
	out[3]=b[0]*a[3]+b[1]*a[2]-b[2]*a[1]+b[3]*a[0];
}

void QuatMatrix(float in[4], float *out)
{
	float m[16];
	float xx, yy, zz, mag;

	if(!out)
		return;

	mag=1.0f/sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]+in[3]*in[3]);
	in[0]*=mag;
	in[1]*=mag;
	in[2]*=mag;
	in[3]*=mag;

	xx=in[1]*in[1];
	yy=in[2]*in[2];
	zz=in[3]*in[3];

	m[ 0]=1.0f-2.0f*(yy+zz);
	m[ 1]=2.0f*(in[1]*in[2]+in[0]*in[3]);
	m[ 2]=2.0f*(in[1]*in[3]-in[0]*in[2]);
	m[ 3]=0.0f;
	m[ 4]=2.0f*(in[1]*in[2]-in[0]*in[3]);
	m[ 5]=1.0f-2.0f*(xx+zz);
	m[ 6]=2.0f*(in[2]*in[3]+in[0]*in[1]);
	m[ 7]=0.0f;
	m[ 8]=2.0f*(in[1]*in[3]+in[0]*in[2]);
	m[ 9]=2.0f*(in[2]*in[3]-in[0]*in[1]);
	m[10]=1.0f-2.0f*(xx+yy);
	m[11]=0.0f;
	m[12]=0.0f;
	m[13]=0.0f;
	m[14]=0.0f;
	m[15]=1.0f;

	MatrixMult(m, out, out);
}

// Matrix functions
void MatrixIdentity(float *out)
{
	if(!out)
		return;

	out[ 0]=1.0f;	out[ 1]=0.0f;	out[ 2]=0.0f;	out[ 3]=0.0f;
	out[ 4]=0.0f;	out[ 5]=1.0f;	out[ 6]=0.0f;	out[ 7]=0.0f;
	out[ 8]=0.0f;	out[ 9]=0.0f;	out[10]=1.0f;	out[11]=0.0f;
	out[12]=0.0f;	out[13]=0.0f;	out[14]=0.0f;	out[15]=1.0f;
}

void MatrixMult(float a[16], float b[16], float *out)
{
	float res[16];

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

	memcpy(out, res, sizeof(float)*16);
}

void MatrixInverse(float in[16], float *out)
{
	float res[16];

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

	memcpy(out, res, sizeof(float)*16);
}

void MatrixTranspose(float in[16], float *out)
{
	float res[16];

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

	memcpy(out, res, sizeof(float)*16);
}

void MatrixTranslate(float x, float y, float z, float *out)
{
	float m[16];

	if(!out)
		return;

	m[ 0]=1.0f;	m[ 1]=0.0f;	m[ 2]=0.0f;	m[ 3]=0.0f;
	m[ 4]=0.0f;	m[ 5]=1.0f;	m[ 6]=0.0f;	m[ 7]=0.0f;
	m[ 8]=0.0f;	m[ 9]=0.0f;	m[10]=1.0f;	m[11]=0.0f;
	m[12]=x;	m[13]=y;	m[14]=z;	m[15]=1.0f;

	MatrixMult(m, out, out);
}

void MatrixScale(float x, float y, float z, float *out)
{
	float m[16];

	if(!out)
		return;

	m[ 0]=x;	m[ 1]=0.0f;	m[ 2]=0.0f;	m[ 3]=0.0f;
	m[ 4]=0.0f;	m[ 5]=y;	m[ 6]=0.0f;	m[ 7]=0.0f;
	m[ 8]=0.0f;	m[ 9]=0.0f;	m[10]=z;	m[11]=0.0f;
	m[12]=0.0f;	m[13]=0.0f;	m[14]=0.0f;	m[15]=1.0f;

	MatrixMult(m, out, out);
}

void Matrix4x4MultVec4(float in[4], float m[16], float *out)
{
	float res[4];

	if(!out)
		return;

	res[0]=in[0]*m[ 0]+in[1]*m[ 4]+in[2]*m[ 8]+in[3]*m[12];
	res[1]=in[0]*m[ 1]+in[1]*m[ 5]+in[2]*m[ 9]+in[3]*m[13];
	res[2]=in[0]*m[ 2]+in[1]*m[ 6]+in[2]*m[10]+in[3]*m[14];
	res[3]=in[0]*m[ 3]+in[1]*m[ 7]+in[2]*m[11]+in[3]*m[15];

	memcpy(out, res, sizeof(float)*4);
}

void Matrix4x4MultVec3(float in[3], float m[16], float *out)
{
	float res[3];

	if(!out)
		return;

	res[0]=in[0]*m[ 0]+in[1]*m[ 4]+in[2]*m[ 8]+m[12];
	res[1]=in[0]*m[ 1]+in[1]*m[ 5]+in[2]*m[ 9]+m[13];
	res[2]=in[0]*m[ 2]+in[1]*m[ 6]+in[2]*m[10]+m[14];

	memcpy(out, res, sizeof(float)*3);
}

void Matrix3x3MultVec3(float in[3], float m[16], float *out)
{
	float res[3];

	if(!out)
		return;

	res[0]=in[0]*m[ 0]+in[1]*m[ 4]+in[2]*m[ 8];
	res[1]=in[0]*m[ 1]+in[1]*m[ 5]+in[2]*m[ 9];
	res[2]=in[0]*m[ 2]+in[1]*m[ 6]+in[2]*m[10];

	memcpy(out, res, sizeof(float)*3);
}

// Projection matrix functions
void InfPerspective(float fovy, float aspect, float zNear, int flip, float *out)
{
	float y=tanf((fovy/2.0f)*3.14159f/180.0f)*zNear, x=aspect*y;
	float nudge=1.0f-(1.0f/(1<<16));
	float m[16];

	if(!out)
		return;

	m[0]=zNear/x;
	m[1]=0.0f;
	m[2]=0.0f;
	m[3]=0.0f;
	m[4]=0.0f;
	m[5]=flip?-zNear/y:zNear/y;
	m[6]=0.0f;
	m[7]=0.0f;
	m[8]=0.0f;
	m[9]=0.0f;
	m[10]=-1.0f*nudge;
	m[11]=-1.0f;
	m[12]=0.0f;
	m[13]=0.0f;
	m[14]=-2.0f*zNear*nudge;
	m[15]=0.0f;

	MatrixMult(m, out, out);
}

void Perspective(float fovy, float aspect, float zNear, float zFar, int flip, float *out)
{
	float y=tanf((fovy/2.0f)*3.14159f/180.0f)*zNear, x=aspect*y;
	float m[16];

	if(!out)
		return;

	m[0]=zNear/x;
	m[1]=0.0f;
	m[2]=0.0f;
	m[3]=0.0f;
	m[4]=0.0f;
	m[5]=flip?-zNear/y:zNear/y;
	m[6]=0.0f;
	m[7]=0.0f;
	m[8]=0.0f;
	m[9]=0.0f;
	m[10]=-(zFar+zNear)/(zFar-zNear);
	m[11]=-1.0f;
	m[12]=0.0f;
	m[13]=0.0f;
	m[14]=-(2.0f*zNear*zFar)/(zFar-zNear);
	m[15]=0.0f;

	MatrixMult(m, out, out);
}

void Ortho(float left, float right, float bottom, float top, float zNear, float zFar, float *out)
{
	float m[16];

	MatrixIdentity(m);

	m[0]=2/(right-left);
	m[5]=2/(top-bottom);	
	m[10]=-2/(zFar-zNear);
	m[12]=-(right+left)/(right-left);
	m[13]=-(top+bottom)/(top-bottom);
	m[14]=-(zFar+zNear)/(zFar-zNear);

	MatrixMult(m, out, out);
}
