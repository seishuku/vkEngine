#ifndef __MATH_H__
#define __MATH_H__

#include <stdint.h>
#include <math.h>

#ifdef WIN32
#undef min
#undef max
#endif

inline static const int32_t min(const int32_t a, const int32_t b) { return (a<b)?a:b; }
inline static const int32_t max(const int32_t a, const int32_t b) { return (a>b)?a:b; }

#ifndef PI
#define PI 3.1415926f
#endif

inline static float clampf(const float value, const float min, const float max) { return fminf(fmaxf(value, min), max); }

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
typedef struct { vec4 x, y, z, w; } matrix;

#define VEC_INLINE

// VecX - Component wise type initializer (set vector to x, y, z, w)
// Vec*b - Broadcast wise type initializer (set vector to b)
// VecX_X - Component wise type op (x, y, z, w math op on vector)
// VecX_Xv - Vector wise type op (vector math op on vector)
// VecX_Xs - Broadcast wise type op (b math op on vector)

#ifdef VEC_INLINE
inline static const vec2 Vec2(const float x, const float y) { return (vec2) { .x=x, .y=y }; }
inline static const vec2 Vec2b(const float b) { return (vec2) { .x=b, .y=b }; }
inline static vec2 Vec2_Add(const vec2 a, const float x, const float y) { return (vec2) { .x=a.x+x, .y=a.y+y }; }
inline static vec2 Vec2_Addv(const vec2 a, const vec2 b) { return (vec2) { .x=a.x+b.x, .y=a.y+b.y }; }
inline static vec2 Vec2_Adds(const vec2 a, const float b) { return (vec2) { .x=a.x+b, .y=a.y+b }; }
inline static vec2 Vec2_Sub(const vec2 a, const float x, const float y) { return (vec2) { .x=a.x-x, .y=a.y-y }; }
inline static vec2 Vec2_Subv(const vec2 a, const vec2 b) { return (vec2) { .x=a.x-b.x, .y=a.y-b.y }; }
inline static vec2 Vec2_Subs(const vec2 a, const float b) { return (vec2) { .x=a.x-b, .y=a.y-b }; }
inline static vec2 Vec2_Mul(const vec2 a, const float x, const float y) { return (vec2) { .x=a.x*x, .y=a.y*y }; }
inline static vec2 Vec2_Mulv(const vec2 a, const vec2 b) { return (vec2) { .x=a.x*b.x, .y=a.y*b.y }; }
inline static vec2 Vec2_Muls(const vec2 a, const float b) { return (vec2) { .x=a.x*b, .y=a.y*b }; }
inline static float Vec2_Dot(const vec2 a, const vec2 b) { return a.x*b.x+a.y*b.y; }
inline static float Vec2_Length(const vec2 v) { return sqrtf(Vec2_Dot(v, v)); }
inline static float Vec2_Distance(const vec2 v0, const vec2 v1) { return Vec2_Length(Vec2_Subv(v1, v0)); }
inline static float Vec2_DistanceSq(const vec2 v0, const vec2 v1) { return (v0.x-v1.x)*(v0.x-v1.x)+(v0.y-v1.y)*(v0.y-v1.y); }
inline static vec2 Vec2_Reflect(const vec2 N, const vec2 I) { return Vec2_Subv(I, Vec2_Muls(N, 2.0f*Vec2_Dot(N, I))); }
inline static vec2 Vec2_Lerp(const vec2 a, const vec2 b, const float t) { return Vec2_Addv(Vec2_Muls(Vec2_Subv(b, a), t), a); }
inline static vec2 Vec2_Clamp(const vec2 v, const float min, const float max) { return (vec2) { .x=fminf(fmaxf(v.x, min), max), .y=fminf(fmaxf(v.y, min), max) }; }
inline static vec2 Vec2_Clampv(const vec2 v, const vec2 min, const vec2 max) { return (vec2) { .x=fminf(fmaxf(v.x, min.x), max.x), .y=fminf(fmaxf(v.y, min.y), max.y) }; }
#else
const vec2 Vec2(const float x, const float y);
const vec2 Vec2b(const float b);
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
float Vec2_Length(const vec2 v);
float Vec2_Distance(const vec2 v0, const vec2 v1);
float Vec2_DistanceSq(const vec2 v0, const vec2 v1);
vec2 Vec2_Reflect(const vec2 N, const vec2 I);
vec2 Vec2_Lerp(const vec2 a, const vec2 b, const float t);
vec2 Vec2_Clamp(const vec2 v, const float min, const float max);
vec2 Vec2_Clampv(const vec2 v, const vec2 min, const vec2 max);
#endif

float Vec2_Normalize(vec2 *v);

#ifdef VEC_INLINE
inline static const vec3 Vec3(const float x, const float y, const float z) { return (vec3) { .x=x, .y=y, .z=z }; }
inline static const vec3 Vec3_Vec2(const vec2 a, const float z) { return (vec3) { .x=a.x, .y=a.y, .z=z }; }
inline static const vec3 Vec3b(const float b) { return (vec3) { .x=b, .y=b, .z=b }; }
inline static vec3 Vec3_Add(const vec3 a, const float x, const float y, const float z) { return (vec3) { .x=a.x+x, .y=a.y+y, .z=a.z+z }; }
inline static vec3 Vec3_Addv(const vec3 a, const vec3 b) { return (vec3) { .x=a.x+b.x, .y=a.y+b.y, .z=a.z+b.z }; }
inline static vec3 Vec3_Adds(const vec3 a, const float b) { return (vec3) { .x=a.x+b, .y=a.y+b, .z=a.z+b }; }
inline static vec3 Vec3_Sub(const vec3 a, const float x, const float y, const float z) { return (vec3) { .x=a.x-x, .y=a.y-y, .z=a.z-z }; }
inline static vec3 Vec3_Subv(const vec3 a, const vec3 b) { return (vec3) { .x=a.x-b.x, .y=a.y-b.y, .z=a.z-b.z }; }
inline static vec3 Vec3_Subs(const vec3 a, const float b) { return (vec3) { .x=a.x-b, .y=a.y-b, .z=a.z-b }; }
inline static vec3 Vec3_Mul(const vec3 a, const float x, const float y, const float z) { return (vec3) { .x=a.x*x, .y=a.y*y, .z=a.z*z }; }
inline static vec3 Vec3_Mulv(const vec3 a, const vec3 b) { return (vec3) { .x=a.x*b.x, .y=a.y*b.y, .z=a.z*b.z }; }
inline static vec3 Vec3_Muls(const vec3 a, const float b) { return (vec3) { .x=a.x*b, .y=a.y*b, .z=a.z*b }; }
inline static float Vec3_Dot(const vec3 a, const vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline static float Vec3_Length(const vec3 v) { return sqrtf(Vec3_Dot(v, v)); }
inline static float Vec3_Distance(const vec3 v0, const vec3 v1) { return Vec3_Length(Vec3_Subv(v1, v0)); }
inline static float Vec3_DistanceSq(const vec3 v0, const vec3 v1) { return (v0.x-v1.x)*(v0.x-v1.x)+(v0.y-v1.y)*(v0.y-v1.y)+(v0.z-v1.z)*(v0.z-v1.z); }
inline static float Vec3_GetAngle(const vec3 v0, const vec3 v1) { return acosf(Vec3_Dot(v0, v1)/(Vec3_Length(v0)*Vec3_Length(v1))); }
inline static vec3 Vec3_Reflect(const vec3 N, const vec3 I) { return Vec3_Subv(I, Vec3_Muls(N, 2.0f*Vec3_Dot(N, I))); }
inline static vec3 Vec3_Cross(const vec3 v0, const vec3 v1) { return (vec3) { .x=v0.y*v1.z-v0.z*v1.y, .y=v0.z*v1.x-v0.x*v1.z, .z=v0.x*v1.y-v0.y*v1.x }; }
inline static vec3 Vec3_Lerp(const vec3 a, const vec3 b, const float t) { return Vec3_Addv(Vec3_Muls(Vec3_Subv(b, a), t), a); }
inline static vec3 Vec3_Clamp(const vec3 v, const float min, const float max) { return (vec3) { .x=fminf(fmaxf(v.x, min), max), .y=fminf(fmaxf(v.y, min), max), .z=fminf(fmaxf(v.z, min), max) }; }
inline static vec3 Vec3_Clampv(const vec3 v, const vec3 min, const vec3 max) { return (vec3) { .x=fminf(fmaxf(v.x, min.x), max.x), .y=fminf(fmaxf(v.y, min.y), max.y), .z=fminf(fmaxf(v.z, min.z), max.z) }; }
#else
const vec3 Vec3(const float x, const float y, const float z);
const vec3 Vec3b(const float b);
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
float Vec3_Length(const vec3 v);
float Vec3_Distance(const vec3 v0, const vec3 v1);
float Vec3_DistanceSq(const vec3 v0, const vec3 v1);
float Vec3_GetAngle(const vec3 v0, const vec3 v1);
vec3 Vec3_Reflect(const vec3 N, const vec3 I);
vec3 Vec3_Cross(const vec3 v0, const vec3 v1);
vec3 Vec3_Lerp(const vec3 a, const vec3 b, const float t);
vec3 Vec3_Clamp(const vec3 v, const float min, const float max);
vec3 Vec3_Clampv(const vec3 v, const vec3 min, const vec3 max);
#endif

float Vec3_Normalize(vec3 *v);

#ifdef VEC_INLINE
inline static const vec4 Vec4(const float x, const float y, const float z, const float w) { return (vec4) { .x=x, .y=y, .z=z, .w=w }; }
inline static const vec4 Vec4_Vec3(const vec3 a, const float w) { return (vec4) { .x=a.x, .y=a.y, .z=a.z, .w=w }; }
inline static const vec4 Vec4_Vec2(const vec2 a, const float z, const float w) { return (vec4) { .x=a.x, .y=a.y, .z=z, .w=w }; }
inline static const vec4 Vec4b(const float b) { return (vec4) { .x=b, .y=b, .z=b, .w=b }; }
inline static vec4 Vec4_Add(const vec4 a, const float x, const float y, const float z, const float w) { return (vec4) { .x=a.x+x, .y=a.y+y, .z=a.z+z, .w=a.w+w }; }
inline static vec4 Vec4_Addv(const vec4 a, const vec4 b) { return (vec4) { .x=a.x+b.x, .y=a.y+b.y, .z=a.z+b.z, .w=a.w+b.w }; }
inline static vec4 Vec4_Adds(const vec4 a, const float b) { return (vec4) { .x=a.x+b, .y=a.y+b, .z=a.z+b, .w=a.w+b }; }
inline static vec4 Vec4_Sub(const vec4 a, const float x, const float y, const float z, const float w) { return (vec4) { .x=a.x-x, .y=a.y-y, .z=a.z-z, .w=a.w-w }; }
inline static vec4 Vec4_Subv(const vec4 a, const vec4 b) { return (vec4) { .x=a.x-b.x, .y=a.y-b.y, .z=a.z-b.z, .w=a.w-b.w }; }
inline static vec4 Vec4_Subs(const vec4 a, const float b) { return (vec4) { .x=a.x-b, .y=a.y-b, .z=a.z-b, .w=a.w-b }; }
inline static vec4 Vec4_Mul(const vec4 a, const float x, const float y, const float z, const float w) { return (vec4) { .x=a.x*x, .y=a.y*y, .z=a.z*z, .w=a.w*w }; }
inline static vec4 Vec4_Mulv(const vec4 a, const vec4 b) { return (vec4) { .x=a.x*b.x, .y=a.y*b.y, .z=a.z*b.z, .w=a.w*b.w }; }
inline static vec4 Vec4_Muls(const vec4 a, const float b) { return (vec4) { .x=a.x*b, .y=a.y*b, .z=a.z*b, .w=a.w*b }; }
inline static float Vec4_Dot(const vec4 a, const vec4 b) { return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w; }
inline static float Vec4_Length(const vec4 v) { return sqrtf(Vec4_Dot(v, v)); }
inline static float Vec4_Distance(const vec4 v0, const vec4 v1) { return Vec4_Length(Vec4_Subv(v1, v0)); }
inline static float Vec4_DistanceSq(const vec4 v0, const vec4 v1) { return (v0.x-v1.x)*(v0.x-v1.x)+(v0.y-v1.y)*(v0.y-v1.y)+(v0.z-v1.z)*(v0.z-v1.z)+(v0.w-v1.w)*(v0.w-v1.w); }
inline static vec4 Vec4_Reflect(const vec4 N, const vec4 I) { return Vec4_Subv(I, Vec4_Muls(N, 2.0f*Vec4_Dot(N, I))); }
inline static vec4 Vec4_Lerp(const vec4 a, const vec4 b, const float t) { return Vec4_Addv(Vec4_Muls(Vec4_Subv(b, a), t), a); }
inline static vec4 Vec4_Clamp(const vec4 v, const float min, const float max) { return (vec4) { .x=fminf(fmaxf(v.x, min), max), .y=fminf(fmaxf(v.y, min), max), .z=fminf(fmaxf(v.z, min), max), .w=fminf(fmaxf(v.w, min), max) }; }
inline static vec4 Vec4_Clampv(const vec4 v, const vec4 min, const vec4 max) { return (vec4) { .x=fminf(fmaxf(v.x, min.x), max.x), .y=fminf(fmaxf(v.y, min.y), max.y), .z=fminf(fmaxf(v.z, min.z), max.z), .w=fminf(fmaxf(v.w, min.w), max.w) }; }
#else
const vec4 Vec4(const float x, const float y, const float z, const float w);
const vec4 Vec4_Vec3(const vec3 a, const float w);
const vec4 Vec4_Vec2(const vec2 a, const float z, const float w);
const vec4 Vec4b(const float b);
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
float Vec4_Length(const vec4 v);
float Vec4_Distance(const vec4 v0, const vec4 v1);
float Vec4_DistanceSq(const vec4 v0, const vec4 v1);
vec4 Vec4_Reflect(const vec4 N, const vec4 I);
vec4 Vec4_Lerp(const vec4 a, const vec4 b, const float t);
vec4 Vec4_Clamp(const vec4 v, const float min, const float max);
vec4 Vec4_Clampv(const vec4 v, const vec4 min, const vec4 max);
#endif

float Vec4_Normalize(vec4 *v);

float fsinf(const float v);
float fcosf(const float v);
float ftanf(const float x);
float rsqrtf(float x);

inline static float deg2rad(const float x)
{
	return x*PI/180.0f;
}

inline static float rad2deg(const float x)
{
	return 180.0f*x/PI;
}

float fact(const int32_t n);

void RandomSeed(uint32_t seed);
uint32_t Random(void);
int32_t RandRange(int32_t min, int32_t max);
float RandFloat(void);
float RandFloatRange(float min, float max);
uint32_t IsPower2(uint32_t value);
uint32_t NextPower2(uint32_t value);
int32_t ComputeLog(uint32_t value);
float Lerp(const float a, const float b, const float t);
float raySphereIntersect(vec3 rayOrigin, vec3 rayDirection, vec3 sphereCenter, float sphereRadius);

vec4 QuatAngle(const float angle, const float x, const float y, const float z);
vec4 QuatAnglev(const float angle, const vec3 v);
vec4 QuatEuler(const float roll, const float pitch, const float yaw);
vec4 QuatMultiply(const vec4 a, const vec4 b);
vec4 QuatInverse(const vec4 q);
vec3 QuatRotate(const vec4 q, const vec3 v);
vec4 QuatSlerp(const vec4 qa, const vec4 qb, const float t);
matrix QuatToMatrix(const vec4 in);
vec4 MatrixToQuat(const matrix m);

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
