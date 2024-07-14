#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../physics/physics.h"

typedef struct Camera_s
{
	RigidBody_t body;

	vec3 right;
	vec3 up;
	vec3 forward;

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
	float time;
	float endTime;
	float *position;
	float *view;
	int32_t numPoints;
	int32_t *knots;
} CameraPath_t;

bool CameraIsTargetInFOV(Camera_t camera, vec3 targetPos, float FOV);
void CameraSeekTargetCamera(Camera_t *camera, Camera_t cameraTarget, RigidBody_t *obstacles, size_t numObstacles);
void CameraInit(Camera_t *camera, const vec3 position, const vec3 right, const vec3 up, const vec3 forward);
matrix CameraUpdate(Camera_t *camera, float dt);
void CameraCheckCollision(Camera_t *camera, float *vertex, uint32_t *face, int32_t numFace);
int32_t CameraLoadPath(char *filename, CameraPath_t *path);
matrix CameraInterpolatePath(CameraPath_t *path, float dt);
void CameraDeletePath(CameraPath_t *path);

#endif
