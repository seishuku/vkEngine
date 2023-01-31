#ifndef __MATH_H__
#define __MATH_H__

#include <math.h>

#ifndef PI
#define PI 3.1415926f
#endif

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];
typedef float matrix[16];

inline void Vec2_Set(vec2 a, float x, float y) { a[0]=x; a[1]=y; }
inline void Vec2_Setv(vec2 a, vec2 b) { a[0]=b[0]; a[1]=b[1]; }
inline void Vec2_Sets(vec2 a, float b) { a[0]=b; a[1]=b; }
inline void  Vec3_Set(vec3 a, float x, float y, float z) { a[0]=(x); a[1]=(y); a[2]=(z); }
inline void Vec3_Setv(vec3 a, vec3 b) { a[0]=b[0]; a[1]=b[1]; a[2]=b[2]; }
inline void Vec3_Sets(vec3 a, float b) { a[0]=b; a[1]=b; a[2]=b; }
inline void  Vec4_Set(vec4 a, float x, float y, float z, float w) { a[0]=(x); a[1]=(y); a[2]=(z); a[3]=(w); }
inline void Vec4_Setv(vec4 a, vec4 b) { a[0]=b[0]; a[1]=b[1]; a[2]=b[2]; a[3]=b[3]; }
inline void Vec4_Sets(vec4 a, float b) { a[0]=b; a[1]=b; a[2]=b; a[3]=b; }

inline void  Vec2_Add(vec2 a, float x, float y) { a[0]+=x; a[1]+=y; }
inline void Vec2_Addv(vec2 a, vec2 b) { a[0]+=b[0]; a[1]+=b[1]; }
inline void Vec2_Adds(vec2 a, float b) { a[0]+=b; a[1]+=b; }
inline void  Vec3_Add(vec3 a, float x, float y, float z) { a[0]+=x; a[1]+=y; a[2]+=z; }
inline void Vec3_Addv(vec3 a, vec3 b) { a[0]+=b[0]; a[1]+=b[1]; a[2]+=b[2]; }
inline void Vec3_Adds(vec3 a, float b) { a[0]+=b; a[1]+=b; a[2]+=b; }
inline void  Vec4_Add(vec4 a, float x, float y, float z, float w) { a[0]+=x; a[1]+=y; a[2]+=z; a[3]+=w; }
inline void Vec4_Addv(vec4 a, vec4 b) { a[0]+=b[0]; a[1]+=b[1]; a[2]+=b[2]; a[3]+=b[3]; }
inline void Vec4_Adds(vec4 a, float b) { a[0]+=b; a[1]+=b; a[2]+=b; a[3]+=b; }

inline void  Vec2_Sub(vec2 a, float x, float y) { a[0]-=x; a[1]-=y; }
inline void Vec2_Subv(vec2 a, vec2 b) { a[0]-=b[0]; a[1]-=b[1]; }
inline void Vec2_Subs(vec2 a, float b) { a[0]-=b; a[1]-=b; }
inline void  Vec3_Sub(vec3 a, float x, float y, float z) { a[0]-=x; a[1]-=y; a[2]-=z; }
inline void Vec3_Subv(vec3 a, vec3 b) { a[0]-=b[0]; a[1]-=b[1]; a[2]-=b[2]; }
inline void Vec3_Subs(vec3 a, float b) { a[0]-=b; a[1]-=b; a[2]-=b; }
inline void  Vec4_Sub(vec4 a, float x, float y, float z, float w) { a[0]-=x; a[1]-=y; a[2]-=z; a[3]-=w; }
inline void Vec4_Subv(vec4 a, vec4 b) { a[0]-=b[0]; a[1]-=b[1]; a[2]-=b[2]; a[3]-=b[3]; }
inline void Vec4_Subs(vec4 a, float b) { a[0]-=b; a[1]-=b; a[2]-=b; a[3]-=b; }

inline void  Vec2_Mul(vec2 a, float x, float y) { a[0]*=x; a[1]*=y; }
inline void Vec2_Mulv(vec2 a, vec2 b) { a[0]*=b[0]; a[1]*=b[1]; }
inline void Vec2_Muls(vec2 a, float b) { a[0]*=b; a[1]*=b; }
inline void  Vec3_Mul(vec3 a, float x, float y, float z) { a[0]*=x; a[1]*=y; a[2]*=z; }
inline void Vec3_Mulv(vec3 a, vec3 b) { a[0]*=b[0]; a[1]*=b[1]; a[2]*=b[2]; }
inline void Vec3_Muls(vec3 a, float b) { a[0]*=b; a[1]*=b; a[2]*=b; }
inline void  Vec4_Mul(vec4 a, float x, float y, float z, float w) { a[0]*=x; a[1]*=y; a[2]*=z; a[3]*=w; }
inline void Vec4_Mulv(vec4 a, vec4 b) { a[0]*=b[0]; a[1]*=b[1]; a[2]*=b[2]; a[3]*=b[3]; }
inline void Vec4_Muls(vec4 a, float b) { a[0]*=b; a[1]*=b; a[2]*=b; a[3]*=b; }

float fsinf(const float v);
float fcosf(const float v);
float ftanf(const float x);

inline float deg2rad(float x)
{
	return x*PI/180.0f;
}

inline float rad2deg(float x)
{
	return 180.0f*x/PI;
}

float fact(const int32_t n);

uint32_t IsPower2(uint32_t value);
uint32_t NextPower2(uint32_t value);
int32_t ComputeLog(uint32_t value);

float Vec2_Dot(const vec2 a, const vec2 b);
float Vec3_Dot(const vec3 a, const vec3 b);
float Vec4_Dot(const vec4 a, const vec4 b);
float Vec2_Length(const vec2 Vector);
float Vec3_Length(const vec3 Vector);
float Vec4_Length(const vec4 Vector);
float Vec2_Distance(const vec2 Vector1, const vec2 Vector2);
float Vec3_Distance(const vec3 Vector1, const vec3 Vector2);
float Vec4_Distance(const vec4 Vector1, const vec4 Vector2);
float Vec3_GetAngle(const vec3 Vector1, const vec3 Vector2);
float Vec2_Normalize(vec2 v);
float Vec3_Normalize(vec3 v);
float Vec4_Normalize(vec4 v);
void Cross(const vec3 v0, const vec3 v1, vec3 n);
void Lerp(const float a, const float b, const float t, float *out);
void Vec2_Lerp(const vec2 a, const vec2 b, const float t, vec2 out);
void Vec3_Lerp(const vec3 a, const vec3 b, const float t, vec3 out);
void Vec4_Lerp(const vec4 a, const vec4 b, const float t, vec4 out);

void QuatAngle(const float angle, const float x, const float y, const float z, vec4 out);
void QuatAnglev(const float angle, const vec3 v, vec4 out);
void QuatEuler(const float roll, const float pitch, const float yaw, vec4 out);
void QuatMultiply(const vec4 a, const vec4 b, vec4 out);
void QuatInverse(vec4 q);
void QuatRotate(const vec4 q, const vec3 v, vec3 out);
void QuatSlerp(const vec4 qa, const vec4 qb, vec4 out, const float t);
void QuatMatrix(const vec4 in, matrix out);

void MatrixIdentity(matrix out);
void MatrixMult(const matrix a, const matrix b, matrix out);
void MatrixInverse(const matrix in, matrix out);
void MatrixRotate(const float angle, const float x, const float y, const float z, matrix out);
void MatrixRotatev(const float angle, const vec3 v, matrix out);
void MatrixTranspose(const matrix in, matrix out);
void MatrixTranslate(const float x, const float y, const float z, matrix out);
void MatrixTranslatev(const vec3 v, matrix out);
void MatrixScale(const float x, const float y, const float z, matrix out);
void MatrixScalev(const vec3 v, matrix out);
void MatrixAlignPoints(const vec3 start, const vec3 end, const vec3 up, matrix out);
void Matrix4x4MultVec4(const vec4 in, const matrix m, vec4 out);
void Matrix4x4MultVec3(const vec3 in, const matrix m, vec3 out);
void Matrix3x3MultVec3(const vec3 in, const matrix m, vec3 out);
void MatrixLookAt(const vec3 position, const vec3 forward, const vec3 up, matrix out);
void MatrixInfPerspective(float fovy, float aspect, float zNear, matrix out);
void MatrixPerspective(float fovy, float aspect, float zNear, float zFar, matrix out);
void MatrixOrtho(float left, float right, float bottom, float top, float zNear, float zFar, matrix out);

#endif
