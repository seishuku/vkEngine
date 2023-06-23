#ifndef __MATH_H__
#define __MATH_H__

#include <stdint.h>
#include <string.h>
#include <math.h>

#ifndef PI
#define PI 3.1415926f
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { vec4 x, y, z, w; } matrix;

// VecX - Component wise type initalizer (set vector to x, y, z, w)
// Vec*b - Broadcast wise type initalizer (set vector to b)
// VecX_X - Component wise type op (x, y, z, w math op on vector)
// VecX_Xv - Vector wise type op (vector math op on vector)
// VecX_Xs - Broadcast wise type op (b math op on vector)

vec2 Vec2(const float x, const float y);
vec2 Vec2b(const float b);
vec2 Vec2_Add(const vec2 a, const float x, const float y);
vec2 Vec2_Addv(const vec2 a, const vec2 b);
vec2 Vec2_Adds(const vec2 a, const float b);
vec2 Vec2_Sub(const vec2 a, const float x, const float y);
vec2 Vec2_Subv(const vec2 a, const vec2 b);
vec2 Vec2_Subs(const vec2 a, const float b);
vec2 Vec2_Mul(const vec2 a, const float x, const float y);
vec2 Vec2_Mulv(const vec2 a, const vec2 b);
vec2 Vec2_Muls(const vec2 a, const float b);
float Vec2_Dot(const vec2 a, const vec2 b);
float Vec2_Length(const vec2 Vector);
float Vec2_Distance(const vec2 Vector1, const vec2 Vector2);
vec2 Vec2_Reflect(const vec2 N, const vec2 I);
float Vec2_Normalize(vec2 *v);
vec2 Vec2_Lerp(const vec2 a, const vec2 b, const float t);
vec2 Vec2_Clamp(const vec2 v, const float min, const float max);

vec3 Vec3(const float x, const float y, const float z);
vec3 Vec3b(const float b);
vec3 Vec3_Add(const vec3 a, const float x, const float y, const float z);
vec3 Vec3_Addv(const vec3 a, const vec3 b);
vec3 Vec3_Adds(const vec3 a, const float b);
vec3 Vec3_Sub(const vec3 a, const float x, const float y, const float z);
vec3 Vec3_Subv(const vec3 a, const vec3 b);
vec3 Vec3_Subs(const vec3 a, const float b);
vec3 Vec3_Mul(const vec3 a, const float x, const float y, const float z);
vec3 Vec3_Mulv(const vec3 a, const vec3 b);
vec3 Vec3_Muls(const vec3 a, const float b);
float Vec3_Dot(const vec3 a, const vec3 b);
float Vec3_Length(const vec3 Vector);
float Vec3_Distance(const vec3 Vector1, const vec3 Vector2);
float Vec3_GetAngle(const vec3 Vector1, const vec3 Vector2);
vec3 Vec3_Reflect(const vec3 N, const vec3 I);
float Vec3_Normalize(vec3 *v);
vec3 Vec3_Cross(const vec3 v0, const vec3 v1);
vec3 Vec3_Lerp(const vec3 a, const vec3 b, const float t);
vec3 Vec3_Clamp(const vec3 v, const float min, const float max);

vec4 Vec4(const float x, const float y, const float z, const float w);
vec4 Vec4b(const float b);
vec4 Vec4_Add(const vec4 a, const float x, const float y, const float z, const float w);
vec4 Vec4_Addv(const vec4 a, const vec4 b);
vec4 Vec4_Adds(const vec4 a, const float b);
vec4 Vec4_Sub(const vec4 a, const float x, const float y, const float z, const float w);
vec4 Vec4_Subv(const vec4 a, const vec4 b);
vec4 Vec4_Subs(const vec4 a, const float b);
vec4 Vec4_Mul(const vec4 a, const float x, const float y, const float z, const float w);
vec4 Vec4_Mulv(const vec4 a, const vec4 b);
vec4 Vec4_Muls(const vec4 a, const float b);
float Vec4_Dot(const vec4 a, const vec4 b);
float Vec4_Length(const vec4 Vector);
float Vec4_Distance(const vec4 Vector1, const vec4 Vector2);
vec4 Vec4_Reflect(const vec4 N, const vec4 I);
float Vec4_Normalize(vec4 *v);
vec4 Vec4_Lerp(const vec4 a, const vec4 b, const float t);
vec4 Vec4_Clamp(const vec4 v, const float min, const float max);

float fsinf(const float v);
float fcosf(const float v);
float ftanf(const float x);
float rsqrtf(float x);

inline float deg2rad(const float x)
{
	return x*PI/180.0f;
}

inline float rad2deg(const float x)
{
	return 180.0f*x/PI;
}

float fact(const int32_t n);

void RandomSeed(uint32_t Seed);
uint32_t Random(void);
float RandFloat(void);
int32_t RandRange(int32_t min, int32_t max);
uint32_t IsPower2(uint32_t value);
uint32_t NextPower2(uint32_t value);
int32_t ComputeLog(uint32_t value);
float Lerp(const float a, const float b, const float t);

vec4 QuatAngle(const float angle, const float x, const float y, const float z);
vec4 QuatAnglev(const float angle, const vec3 v);
vec4 QuatEuler(const float roll, const float pitch, const float yaw);
vec4 QuatMultiply(const vec4 a, const vec4 b);
vec4 QuatInverse(const vec4 q);
vec3 QuatRotate(const vec4 q, const vec3 v);
vec4 QuatSlerp(const vec4 qa, const vec4 qb, const float t);
matrix QuatMatrix(const vec4 in);

matrix MatrixIdentity(void);
matrix MatrixMult(const matrix a, const matrix b);
matrix MatrixInverse(const matrix in);
matrix MatrixRotate(const float angle, const float x, const float y, const float z);
matrix MatrixRotatev(const float angle, const vec3 v);
matrix MatrixTranspose(const matrix in);
matrix MatrixTranslate(const float x, const float y, const float z);
matrix MatrixTranslatev(const vec3 v);
matrix MatrixScale(const float x, const float y, const float z);
matrix MatrixScalev(const vec3 v);
matrix MatrixAlignPoints(const vec3 start, const vec3 end, const vec3 up);
vec4 Matrix4x4MultVec4(const vec4 in, const matrix m);
vec3 Matrix4x4MultVec3(const vec3 in, const matrix m);
vec3 Matrix3x3MultVec3(const vec3 in, const matrix m);
matrix MatrixLookAt(const vec3 position, const vec3 forward, const vec3 up);
matrix MatrixInfPerspective(float fovy, float aspect, float zNear);
matrix MatrixPerspective(float fovy, float aspect, float zNear, float zFar);
matrix MatrixOrtho(float left, float right, float bottom, float top, float zNear, float zFar);

#endif
