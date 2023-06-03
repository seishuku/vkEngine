#ifndef __MATH_H__
#define __MATH_H__

#include <stdint.h>
#include <string.h>
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

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef float matrix[16];

vec2 Vec2_Set(const float x, const float y);
vec2 Vec2_Setv(const vec2 b);
vec2 Vec2_Sets(const float b);
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

vec3 Vec3_Set(const float x, const float y, const float z);
vec3 Vec3_Setv(const vec3 b);
vec3 Vec3_Sets(const float b);
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

vec4 Vec4_Set(const float x, const float y, const float z, const float w);
vec4 Vec4_Setv(const vec4 b);
vec4 Vec4_Sets(const float b);
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

float RandFloat(void);
int32_t RandRange(int32_t min, int32_t max);
uint32_t IsPower2(uint32_t value);
uint32_t NextPower2(uint32_t value);
int32_t ComputeLog(uint32_t value);
float Lerp(const float a, const float b, const float t);

void QuatAngle(const float angle, const float x, const float y, const float z, vec4 *out);
void QuatAnglev(const float angle, const vec3 v, vec4 *out);
void QuatEuler(const float roll, const float pitch, const float yaw, vec4 *out);
void QuatMultiply(const vec4 a, const vec4 b, vec4 *out);
void QuatInverse(vec4 *q);
void QuatRotate(const vec4 q, const vec3 v, vec3 *out);
void QuatSlerp(const vec4 qa, const vec4 qb, const float t, vec4 *out);
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
void Matrix4x4MultVec4(const vec4 in, const matrix m, vec4 *out);
void Matrix4x4MultVec3(const vec3 in, const matrix m, vec3 *out);
void Matrix3x3MultVec3(const vec3 in, const matrix m, vec3 *out);
void MatrixLookAt(const vec3 position, const vec3 forward, const vec3 up, matrix out);
void MatrixInfPerspective(float fovy, float aspect, float zNear, matrix out);
void MatrixPerspective(float fovy, float aspect, float zNear, float zFar, matrix out);
void MatrixOrtho(float left, float right, float bottom, float top, float zNear, float zFar, matrix out);

#endif
