#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../physics/physics.h"

typedef struct Camera_s
{
	RigidBody_t body;

	bool thirdPerson;
	float followDistance;
	float heightOffset;
	float trackSpeed;
	vec3 targetPosition;

	// Orientation vectors, updated by body orientation
	union
	{
		struct
		{
			vec3 right;
			vec3 up;
			vec3 forward;
		};
		vec3 axes[3];
	};

	// Input states
	bool moveForward, moveBackward;
	bool moveLeft, moveRight;
	bool moveUp, moveDown;
	bool rollLeft, rollRight;
	bool pitchUp, pitchDown;
	bool yawLeft, yawRight;
	bool shift;
} Camera_t;

bool CameraIsTargetInFOV(Camera_t camera, vec3 targetPos, float FOV);
void CameraSeekTargetCamera(Camera_t *camera, Camera_t cameraTarget, RigidBody_t *obstacles, size_t numObstacles);
void CameraInit(Camera_t *camera, const vec3 position, const vec3 up, const vec3 forward);
matrix CameraUpdate(Camera_t *camera, float dt);

#endif
