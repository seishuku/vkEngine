#ifndef __MATH_H__
#define __MATH_H__

#include <math.h>

float fsinf(float v);
float fcosf(float v);
float ftanf(float x);

float fact(int n);

unsigned int NextPower2(unsigned int value);
int ComputeLog(unsigned int value);

void Normalize(float *v);
void Cross(float v0[3], float v1[3], float *n);

void QuatAngle(float angle, float x, float y, float z, float *out);
void QuatEuler(float roll, float pitch, float yaw, float *out);
void QuatMultiply(float a[4], float b[4], float *out);
void QuatMatrix(float in[4], float *out);

void MatrixIdentity(float *out);
void MatrixMult(float a[16], float b[16], float *out);
void MatrixInverse(float in[16], float *out);
void MatrixTranspose(float in[16], float *out);
void MatrixTranslate(float x, float y, float z, float *out);
void MatrixScale(float x, float y, float z, float *out);
void Matrix4x4MultVec4(float in[4], float m[16], float *out);
void Matrix4x4MultVec3(float in[3], float m[16], float *out);
void Matrix3x3MultVec3(float in[3], float m[16], float *out);

void InfPerspective(float fovy, float aspect, float zNear, float *out);
void Perspective(float fovy, float aspect, float zNear, float zFar, float *out);
void Ortho(float left, float right, float bottom, float top, float zNear, float zFar, float *out);

#endif
