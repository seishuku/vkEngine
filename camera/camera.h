#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"

typedef struct
{
	vec3 Position;
	vec3 Velocity;

	vec3 Right;
	vec3 Up;
	vec3 Forward;

	float Pitch;
	float Yaw;
	float Roll;

	float Radius;

	bool key_w, key_s;
	bool key_a, key_d;
	bool key_v, key_c;
	bool key_q, key_e;
	bool key_up, key_down;
	bool key_left, key_right;
	bool shift;
} Camera_t;

typedef struct
{
	float Time;
	float EndTime;
	float *Position;
	float *View;
	int32_t NumPoints;
	int32_t *Knots;
} CameraPath_t;

void CameraInit(Camera_t *Camera, const vec3 Position, const vec3 Right, const vec3 Up, const vec3 Forward);
matrix CameraUpdate(Camera_t *Camera, float Time);
void CameraCheckCollision(Camera_t *Camera, float *Vertex, uint32_t *Face, int32_t NumFace);
int32_t CameraLoadPath(char *filename, CameraPath_t *Path);
matrix CameraInterpolatePath(CameraPath_t *Path, float TimeStep);
void CameraDeletePath(CameraPath_t *Path);

#endif
