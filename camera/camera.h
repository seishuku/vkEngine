#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../math/math.h"
#include "../physics/physics.h"

typedef enum
{
	CAMERA_MODE_FLIGHT=0,	// 6DOF flight camera
	CAMERA_MODE_CHARACTER	// Camera follows a body it doesn't own, first or third
} CameraMode_e;

typedef struct Camera_s
{
	RigidBody_t body;

	CameraMode_e mode;

	// Character mode: the view sits at (or orbits) a pivot above the followed body, along that body's up axis
	const RigidBody_t *followBody;
	float pivotHeight;		// Pivot distance from the body's center along its up
	float orbitDistance;	// 0 = first person
	float pitch;			// Look angle above the horizon (radians), clamped to the limits below
	float minPitch, maxPitch;
	vec3 heading;			// Unit vector perpendicular to the body's up, the direction the view is turned toward
	vec3 lastUp;			// The body's up last frame, used to carry heading along when up changes

	bool thirdPerson;
	float followDistance;
	float heightOffset;
	float trackSpeed;
	vec3 targetPosition;

	// Orientation vectors, updated by body orientation (read only)
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
	vec3 moveAxis;
	vec3 rotateAxis;
	bool shift;
} Camera_t;

bool CameraIsTargetInFOV(Camera_t camera, vec3 targetPos, float FOV);
void CameraSeekTargetCamera(Camera_t *camera, Camera_t cameraTarget, RigidBody_t *obstacles, size_t numObstacles);
void CameraInit(Camera_t *camera, const vec3 position, const vec3 up, const vec3 forward);
matrix CameraUpdate(Camera_t *camera, float dt);

// Switch to character mode
void CameraFollowBody(Camera_t *camera, const RigidBody_t *body, const float pivotHeight, const float orbitDistance);

// Back to the 6DOF flight camera
void CameraSetFlight(Camera_t *camera);

#endif
