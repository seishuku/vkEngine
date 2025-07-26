#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../physics/physics.h"

static vec4 CalculatePlane(vec3 p, vec3 norm)
{
	Vec3_Normalize(&norm);
	return Vec4(norm.x, norm.y, norm.z, Vec3_Dot(norm, p));
}

void CameraCalculateFrustumPlanes(const Camera_t camera, vec4 *frustumPlanes, const float aspect, const float fov, const float nearPlane, const float farPlane)
{
	const float half_v=farPlane*tanf(fov*0.5f);
	const float half_h=half_v*aspect;

	vec3 forward_far=Vec3_Muls(camera.forward, farPlane);

	// Top, bottom, right, left, far, near
	frustumPlanes[0]=CalculatePlane(Vec3_Addv(camera.body.position, Vec3_Muls(camera.forward, nearPlane)), camera.forward);
	frustumPlanes[1]=CalculatePlane(Vec3_Addv(camera.body.position, forward_far), Vec3_Muls(camera.forward, -1.0f));

	frustumPlanes[2]=CalculatePlane(camera.body.position, Vec3_Cross(camera.up, Vec3_Addv(forward_far, Vec3_Muls(camera.right, half_h))));
	frustumPlanes[3]=CalculatePlane(camera.body.position, Vec3_Cross(Vec3_Subv(forward_far, Vec3_Muls(camera.right, half_h)), camera.up));
	frustumPlanes[4]=CalculatePlane(camera.body.position, Vec3_Cross(camera.right, Vec3_Subv(forward_far, Vec3_Muls(camera.up, half_v))));
	frustumPlanes[5]=CalculatePlane(camera.body.position, Vec3_Cross(Vec3_Addv(forward_far, Vec3_Muls(camera.up, half_v)), camera.right));
}

bool CameraIsTargetInFOV(const Camera_t camera, const vec3 targetPos, const float FOV)
{
	const float halfFOVAngle=FOV*0.5f;

	// Calculate the direction from the camera to the target
	vec3 directionToTarget=Vec3_Subv(targetPos, camera.body.position);
	Vec3_Normalize(&directionToTarget);

	const float sqMag=Vec3_Dot(camera.forward, directionToTarget);
	const float angle=acosf(fminf(sqMag, 1.0f));

	// Calculate the angle between the camera's forward vector and the direction to the target
	// Check if the angle is within half of the FOV angle
	return angle<halfFOVAngle;
}

// Move camera to targetPos while avoiding rigid body obstacles.
void CameraSeekTargetCamera(Camera_t *camera, Camera_t cameraTarget, RigidBody_t *obstacles, size_t numObstacles)
{
	const float angularSpeed=0.05f;
	const float positionDamping=0.0005f;
	const float seekRadius=(camera->body.radius+cameraTarget.body.radius)*2.0f;

	// Find relative direction between camera and target
	vec3 directionWorld=Vec3_Subv(cameraTarget.body.position, camera->body.position);

	// Calculate a relative distance for later speed reduction
	const float relativeDistance=Vec3_Dot(directionWorld, directionWorld)-(seekRadius*seekRadius);

	Vec3_Normalize(&directionWorld);

	// Check for obstacles in the avoidance radius
	for(size_t i=0;i<numObstacles;i++)
	{
		const float avoidanceRadius=(camera->body.radius+obstacles[i].radius)*1.5f;
		const vec3 cameraToObstacle=Vec3_Subv(camera->body.position, obstacles[i].position);
		const float cameraToObstacleDistanceSq=Vec3_Dot(cameraToObstacle, cameraToObstacle);

		if(cameraToObstacleDistanceSq<=avoidanceRadius*avoidanceRadius)
		{
			if(cameraToObstacleDistanceSq>0.0f)
			{
				// Adjust the camera trajectory to avoid the obstacle
				const float rMag=1.0f/sqrtf(cameraToObstacleDistanceSq);
				const vec3 avoidanceDirection=Vec3_Muls(cameraToObstacle, rMag);

				directionWorld=Vec3_Addv(directionWorld, avoidanceDirection);
				Vec3_Normalize(&directionWorld);
			}
		}
	}

	// Apply the directional force to move the camera
	camera->body.force=Vec3_Addv(camera->body.force, Vec3_Muls(directionWorld, relativeDistance*positionDamping));

	// Aim:
	// Get the axis of rotation (an axis perpendicular to the direction between current and target),
	// then rotate that by the current (inverse) orientation, to get the correct orientated axis of rotation.
	vec3 rotationAxis=QuatRotate(QuatInverse(camera->body.orientation), Vec3_Cross(camera->forward, directionWorld));

	// Get the angle between direction vectors to get amount of rotation needed to aim at target.
	float cosTheta=clampf(Vec3_Dot(camera->forward, directionWorld), -1.0f, 1.0f);
	float theta=acosf(cosTheta);

	// Get a quat from the axis of rotation with the theta angle.
	vec4 rotation=QuatAnglev(theta, rotationAxis);

	// Apply that to the angular velocity, multiplied by an angular speed const to control how fast the aiming is.
	camera->body.angularVelocity=Vec3_Addv(camera->body.angularVelocity, Vec3_Muls(Vec3(rotation.x, rotation.y, rotation.z), angularSpeed));
}

// Third person camera matrix mod, basically sets up a camera to the camera
static matrix ThirdPersonMatrix(matrix baseView, Camera_t *targetCamera, float dt)
{
	// Desired position behind/above the target
	const vec3 forwardOffset=Vec3_Muls(targetCamera->forward, -targetCamera->followDistance);
	const vec3 upOffset=Vec3_Muls(targetCamera->up, targetCamera->heightOffset);

	const vec3 desiredPos=Vec3_Addv(Vec3_Addv(targetCamera->body.position, forwardOffset), upOffset);

	// Interpolation to desired position
	const vec3 delta=Vec3_Subv(desiredPos, targetCamera->targetPosition);
	targetCamera->targetPosition=Vec3_Addv(targetCamera->targetPosition, Vec3_Muls(delta, targetCamera->trackSpeed*dt));

	// Look-at matrix from 3rd person to the target
	return MatrixLookAt(targetCamera->targetPosition, targetCamera->body.position, targetCamera->up);
}

// Actual camera stuff
void CameraInit(Camera_t *camera, const vec3 position, const vec3 up, const vec3 forward)
{
	// Third person camera default parameters
	camera->thirdPerson=false;
	camera->targetPosition=position;
	camera->followDistance=12.0f;
	camera->heightOffset=2.0f;
	camera->trackSpeed=20.0f;

	camera->right=Vec3_Cross(up, forward);
	camera->up=up;
	camera->forward=forward;

	Vec3_Normalize(&camera->right);
	Vec3_Normalize(&camera->up);
	Vec3_Normalize(&camera->forward);

	const matrix cameraOrientation=
	{
		.x=Vec4_Vec3(camera->right, 0.0f),
		.y=Vec4_Vec3(camera->up, 0.0f),
		.z=Vec4_Vec3(camera->forward, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	camera->body.position=position;
	camera->body.velocity=Vec3b(0.0f);
	camera->body.force=Vec3b(0.0f);

	camera->body.orientation=MatrixToQuat(cameraOrientation);
	camera->body.angularVelocity=Vec3b(0.0f);

	camera->body.type=RIGIDBODY_SPHERE,
	camera->body.radius=10.0f;

	camera->body.mass=(1.0f/6000.0f)*(1.33333333f*PI*camera->body.radius);
	camera->body.invMass=1.0f/camera->body.mass;

	camera->body.inertia=0.9f*camera->body.mass*(camera->body.radius*camera->body.radius);
	camera->body.invInertia=1.0f/camera->body.inertia;

	camera->key_w=false;
	camera->key_s=false;
	camera->key_a=false;
	camera->key_d=false;
	camera->key_v=false;
	camera->key_c=false;
	camera->key_left=false;
	camera->key_right=false;
	camera->key_up=false;
	camera->key_down=false;
}

matrix CameraUpdate(Camera_t *camera, float dt)
{
	float speed=200.0f*dt;
	float rotation=5.0f*dt;

	if(camera->shift)
		speed*=2.0f;

	if(camera->key_a)
		camera->body.velocity=Vec3_Addv(camera->body.velocity, Vec3_Muls(camera->right, speed));

	if(camera->key_d)
		camera->body.velocity=Vec3_Subv(camera->body.velocity, Vec3_Muls(camera->right, speed));

	if(camera->key_v)
		camera->body.velocity=Vec3_Addv(camera->body.velocity, Vec3_Muls(camera->up, speed));

	if(camera->key_c)
		camera->body.velocity=Vec3_Subv(camera->body.velocity, Vec3_Muls(camera->up, speed));

	if(camera->key_w)
		camera->body.velocity=Vec3_Addv(camera->body.velocity, Vec3_Muls(camera->forward, speed));

	if(camera->key_s)
		camera->body.velocity=Vec3_Subv(camera->body.velocity, Vec3_Muls(camera->forward, speed));

	if(camera->key_up)
		camera->body.angularVelocity=Vec3_Subv(camera->body.angularVelocity, Vec3(rotation, 0.0f, 0.0f));

	if(camera->key_down)
		camera->body.angularVelocity=Vec3_Addv(camera->body.angularVelocity, Vec3(rotation, 0.0f, 0.0f));

	if(camera->key_left)
		camera->body.angularVelocity=Vec3_Addv(camera->body.angularVelocity, Vec3(0.0f, rotation, 0.0f));

	if(camera->key_right)
		camera->body.angularVelocity=Vec3_Subv(camera->body.angularVelocity, Vec3(0.0f, rotation, 0.0f));

	if(camera->key_q)
		camera->body.angularVelocity=Vec3_Subv(camera->body.angularVelocity, Vec3(0.0f, 0.0f, rotation));

	if(camera->key_e)
		camera->body.angularVelocity=Vec3_Addv(camera->body.angularVelocity, Vec3(0.0f, 0.0f, rotation));

	const float maxVelocity=200.0f;
	const float magnitude=Vec3_Length(camera->body.velocity);

	// If velocity magnitude is higher than our max, normalize the velocity vector and scale by maximum speed
	if(magnitude>maxVelocity)
		camera->body.velocity=Vec3_Muls(camera->body.velocity, (1.0f/magnitude)*maxVelocity);

	// Dampen velocity
	const float lambda=2.0f;
	const float decay=expf(-lambda*dt);

	camera->body.velocity=Vec3_Muls(camera->body.velocity, decay);
	camera->body.angularVelocity=Vec3_Muls(camera->body.angularVelocity, decay);

	// Get a matrix from the orientation quat to maintain directional vectors
	matrix orientation=QuatToMatrix(camera->body.orientation);
	camera->right  =Vec3(orientation.x.x, orientation.x.y, orientation.x.z);
	camera->up     =Vec3(orientation.y.x, orientation.y.y, orientation.y.z);
	camera->forward=Vec3(orientation.z.x, orientation.z.y, orientation.z.z);

	if(camera->thirdPerson)
		return ThirdPersonMatrix(MatrixLookAt(camera->body.position, Vec3_Addv(camera->body.position, camera->forward), camera->up), camera, dt);
	else
	{
		camera->targetPosition=camera->body.position;
		return MatrixLookAt(camera->body.position, Vec3_Addv(camera->body.position, camera->forward), camera->up);
	}
}
