#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../physics/physics.h"

RigidBody_t CameraGetRigidBody(const Camera_t camera)
{
	RigidBody_t body;

	body.position=camera.position;
	body.force=Vec3b(0.0f);

	// Transform camera space velocity to world space
	const matrix cameraOrientation=
	{
		.x=Vec4(camera.right.x, camera.up.x, camera.forward.x, 0.0f),
		.y=Vec4(camera.right.y, camera.up.y, camera.forward.y, 0.0f),
		.z=Vec4(camera.right.z, camera.up.z, camera.forward.z, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};
	body.velocity=Matrix3x3MultVec3(camera.velocity, MatrixTranspose(cameraOrientation));

	body.orientation=Vec4(0.0f, 0.0f, 0.0f, 1.0f);
	body.angularVelocity=Vec3b(0.0f);

	body.radius=camera.radius;

	body.mass=(1.0f/6000.0f)*(1.33333333f*PI*body.radius);
	body.invMass=1.0f/body.mass;

	body.inertia=0.4f*body.mass*(body.radius*body.radius);
	body.invInertia=1.0f/body.inertia;

	return body;
}

void CameraSetFromRigidBody(Camera_t *camera, const RigidBody_t body)
{
	camera->position=body.position;

	const matrix cameraOrientation=
	{
		.x=Vec4(camera->right.x, camera->up.x, camera->forward.x, 0.0f),
		.y=Vec4(camera->right.y, camera->up.y, camera->forward.y, 0.0f),
		.z=Vec4(camera->right.z, camera->up.z, camera->forward.z, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	camera->velocity=Matrix3x3MultVec3(body.velocity, cameraOrientation);
}

static vec4 calculatePlane(vec3 p, vec3 norm)
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
	frustumPlanes[0]=calculatePlane(Vec3_Addv(camera.position, Vec3_Muls(camera.forward, nearPlane)), camera.forward);
	frustumPlanes[1]=calculatePlane(Vec3_Addv(camera.position, forward_far), Vec3_Muls(camera.forward, -1.0f));

	frustumPlanes[2]=calculatePlane(camera.position, Vec3_Cross(camera.up, Vec3_Addv(forward_far, Vec3_Muls(camera.right, half_h))));
	frustumPlanes[3]=calculatePlane(camera.position, Vec3_Cross(Vec3_Subv(forward_far, Vec3_Muls(camera.right, half_h)), camera.up));
	frustumPlanes[4]=calculatePlane(camera.position, Vec3_Cross(camera.right, Vec3_Subv(forward_far, Vec3_Muls(camera.up, half_v))));
	frustumPlanes[5]=calculatePlane(camera.position, Vec3_Cross(Vec3_Addv(forward_far, Vec3_Muls(camera.up, half_v)), camera.right));
}

bool CameraIsTargetInFOV(const Camera_t camera, const vec3 targetPos, const float FOV)
{
	const float halfFOVAngle=FOV*0.5f;

	// Calculate the direction from the camera to the target
	vec3 directionToTarget=Vec3_Subv(targetPos, camera.position);
	Vec3_Normalize(&directionToTarget);

	// Calculate the angle between the camera's forward vector and the direction to the target
	// Check if the angle is within half of the FOV angle
	return acosf(Vec3_Dot(camera.forward, directionToTarget))<=halfFOVAngle;
}

// Move camera to targetPos while avoiding rigid body obstacles.
void CameraSeekTarget(Camera_t *camera, const vec3 targetPos, const float targetRadius, RigidBody_t *obstacles, size_t numObstacles)
{
	const float maxSpeed=1.0f;
	const float rotationDamping=0.01f;
	const float positionDamping=0.0005f;
	const float seekRadius=(camera->radius+targetRadius)*1.5f;

	// Find relative direction between camera and target
	vec3 directionWorld=Vec3_Subv(targetPos, camera->position);

	// Calculate a relative distance for later speed reduction
	const float relativeDistance=Vec3_Dot(directionWorld, directionWorld)-(seekRadius*seekRadius);

	Vec3_Normalize(&directionWorld);

	// Check for obstacles in the avoidance radius
	for(size_t i=0;i<numObstacles;i++)
	{
		const float avoidanceRadius=(camera->radius+obstacles[i].radius)*1.1f;
		const vec3 cameraToObstacle=Vec3_Subv(camera->position, obstacles[i].position);
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

	// Build 3x3 matrix and transform worldspace direction to camera space
	const matrix cameraOrientation=
	{
		.x=Vec4(camera->right.x, camera->up.x, camera->forward.x, 0.0f),
		.y=Vec4(camera->right.y, camera->up.y, camera->forward.y, 0.0f),
		.z=Vec4(camera->right.z, camera->up.z, camera->forward.z, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};
	const vec3 directionCamera=Matrix3x3MultVec3(directionWorld, cameraOrientation);

	// Aim pitch and yaw
	camera->yaw=atan2f(directionCamera.x, directionCamera.z)*rotationDamping;
	camera->pitch=asinf(directionCamera.y)*rotationDamping;

	// Slow down the speed as it gets closer
	const float speed=maxSpeed*relativeDistance;

	camera->velocity=Vec3_Addv(Vec3_Muls(camera->velocity, 1.0f-positionDamping), Vec3_Muls(directionCamera, speed*positionDamping));
}

void CameraSeekTargetCamera(Camera_t *camera, Camera_t cameraTarget, RigidBody_t *obstacles, size_t numObstacles)
{
	const float maxSpeed=1.0f;
	const float rotationDamping=0.01f;
	const float positionDamping=0.0005f;
	const float seekRadius=(camera->radius+cameraTarget.radius)*1.5f;

	// Find relative direction between camera and target
	vec3 directionWorld=Vec3_Subv(cameraTarget.position, camera->position);

	// Calculate a relative distance for later speed reduction
	const float relativeDistance=Vec3_Dot(directionWorld, directionWorld)-(seekRadius*seekRadius);

	Vec3_Normalize(&directionWorld);

	// Check for obstacles in the avoidance radius
	for(size_t i=0;i<numObstacles;i++)
	{
		const float avoidanceRadius=(camera->radius+obstacles[i].radius)*1.1f;
		const vec3 cameraToObstacle=Vec3_Subv(camera->position, obstacles[i].position);
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

	// Build 3x3 matrix and transform worldspace direction to camera space
	const matrix cameraOrientation=
	{
		.x=Vec4(camera->right.x, camera->up.x, camera->forward.x, 0.0f),
		.y=Vec4(camera->right.y, camera->up.y, camera->forward.y, 0.0f),
		.z=Vec4(camera->right.z, camera->up.z, camera->forward.z, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};
	const vec3 directionCamera=Matrix3x3MultVec3(directionWorld, cameraOrientation);

	// Aim pitch and yaw
	camera->yaw=atan2f(directionCamera.x, directionCamera.z)*rotationDamping;
	camera->pitch=asinf(directionCamera.y)*rotationDamping;

	// Slow down the speed as it gets closer
	const float speed=maxSpeed*relativeDistance;

	camera->velocity=Vec3_Addv(Vec3_Muls(camera->velocity, 1.0f-positionDamping), Vec3_Muls(directionCamera, speed*positionDamping));
}

// Camera collision stuff
static int32_t ClassifySphere(const vec3 center, const vec3 normal, const vec3 point, const float radius, float *distance)
{
	*distance=Vec3_Dot(normal, center)-Vec3_Dot(normal, point);

	if(fabsf(*distance)<radius)
		return 1;
	else
	{
		if(*distance>=radius)
			return 2;
	}

	return 0;
}

static bool InsidePolygon(const vec3 intersection, const vec3 triangle[3])
{
	float angle=0.0f;

	const vec3 A=Vec3_Subv(triangle[0], intersection);
	const vec3 B=Vec3_Subv(triangle[1], intersection);
	const vec3 C=Vec3_Subv(triangle[2], intersection);

	angle =Vec3_GetAngle(A, B);
	angle+=Vec3_GetAngle(B, C);
	angle+=Vec3_GetAngle(C, A);

	if(angle>=6.220353348f)
		return true;

	return false;
}

static vec3 ClosestPointOnLine(const vec3 A, const vec3 B, const vec3 point)
{
	const vec3 pointDir={ point.x-A.x, point.y-A.y, point.z-A.z };
	const vec3 slope={ B.x-A.y, B.y-A.y, B.z-A.z };
	const float d=Vec3_Dot(slope, slope);
	float recip_d=0.0f;

	if(d)
		recip_d=1.0f/d;

	const float t=fmaxf(0.0f, fminf(1.0f, Vec3_Dot(pointDir, slope)*recip_d));

	return Vec3_Addv(A, Vec3_Muls(slope, t));
}

int32_t EdgeSphereCollision(const vec3 center, const vec3 triangle[3], const float radius)
{
	for(uint32_t i=0;i<3;i++)
	{
		if(Vec3_Distance(ClosestPointOnLine(triangle[i], triangle[(i+1)%3], center), center)<radius)
			return 1;
	}

	return 0;
}

vec3 GetCollisionOffset(const vec3 normal, const float radius, const float distance)
{
	if(distance>0.0f)
		return Vec3_Muls(normal, radius-distance);
	else
		return Vec3_Muls(normal, -(radius+distance));
}

// Camera<->triangle mesh collision detection and response
void CameraCheckCollision(Camera_t *camera, float *vertex, uint32_t *face, int32_t numFace)
{
	float distance=0.0f;

	for(int32_t i=0;i<numFace;i++)
	{
		const vec3 triangle[3]=
		{
			{ vertex[3*face[3*i+0]], vertex[3*face[3*i+0]+1], vertex[3*face[3*i+0]+2] },
			{ vertex[3*face[3*i+1]], vertex[3*face[3*i+1]+1], vertex[3*face[3*i+1]+2] },
			{ vertex[3*face[3*i+2]], vertex[3*face[3*i+2]+1], vertex[3*face[3*i+2]+2] }
		};

		const vec3 v0=Vec3_Subv(triangle[1], triangle[0]);
		const vec3 v1=Vec3_Subv(triangle[2], triangle[0]);

		vec3 normal=Vec3_Cross(v0, v1);
		Vec3_Normalize(&normal);

		if(ClassifySphere(camera->position, normal, triangle[0], camera->radius, &distance)==1)
		{
			const vec3 intersection=Vec3_Subv(camera->position, Vec3_Muls(normal, distance));

			if(InsidePolygon(intersection, triangle)||EdgeSphereCollision(camera->position, triangle, camera->radius*0.5f))
				camera->position=Vec3_Addv(camera->position, GetCollisionOffset(normal, camera->radius, distance));
		}
	}
}

bool SphereBBOXIntersection(const vec3 center, const float radius, const vec3 bbMin, const vec3 bbMax)
{
	float dmin=0.0f;
	const float radiusSq=radius*radius;

	if(center.x<bbMin.x)
		dmin+=(center.x-bbMin.x)*(center.x-bbMin.x);
	else
	{
		if(center.x>bbMax.x)
			dmin+=(center.x-bbMax.x)*(center.x-bbMax.x);
	}

	if(center.y<bbMin.y)
		dmin+=(center.y-bbMin.y)*(center.y-bbMin.y);
	else
	{
		if(center.y>bbMax.y)
			dmin+=(center.y-bbMax.y)*(center.y-bbMax.y);
	}

	if(center.z<bbMin.z)
		dmin+=(center.z-bbMin.z)*(center.z-bbMin.z);
	else
	{
		if(center.z>bbMax.z)
			dmin+=(center.z-bbMax.z)*(center.z-bbMax.z);
	}

	if(dmin<=radiusSq)
		return true;

	return false;
}

// Actual camera stuff
void CameraInit(Camera_t *camera, const vec3 position, const vec3 right, const vec3 up, const vec3 forward)
{
	camera->position=position;
	camera->right=right;
	camera->up=up;
	camera->forward=forward;

	camera->pitch=0.0f;
	camera->yaw=0.0f;
	camera->roll=0.0f;

	camera->radius=10.0f;

	camera->velocity=Vec3b(0.0f);

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

static void CameraRotate(Camera_t *camera)
{
	const vec4 pitchYaw=QuatMultiply(QuatAnglev(-camera->pitch, camera->right), QuatAnglev(camera->yaw, camera->up));
	camera->forward=QuatRotate(pitchYaw, camera->forward);
	Vec3_Normalize(&camera->forward);

	camera->right=Vec3_Cross(camera->up, camera->forward);

	camera->right=QuatRotate(QuatAnglev(-camera->roll, camera->forward), camera->right);
	Vec3_Normalize(&camera->right);

	camera->up=Vec3_Cross(camera->forward, camera->right);
}

matrix CameraUpdate(Camera_t *camera, float dt)
{
	float speed=240.0f;
	float rotation=0.0625f;

	if(camera->shift)
		speed*=2.0f;

	if(camera->key_a)
		camera->velocity.x+=speed*dt;

	if(camera->key_d)
		camera->velocity.x-=speed*dt;

	if(camera->key_v)
		camera->velocity.y+=speed*dt;

	if(camera->key_c)
		camera->velocity.y-=speed*dt;

	if(camera->key_w)
		camera->velocity.z+=speed*dt;

	if(camera->key_s)
		camera->velocity.z-=speed*dt;

	if(camera->key_q)
		camera->roll+=rotation*dt;

	if(camera->key_e)
		camera->roll-=rotation*dt;

	if(camera->key_left)
		camera->yaw+=rotation*dt;

	if(camera->key_right)
		camera->yaw-=rotation*dt;

	if(camera->key_up)
		camera->pitch+=rotation*dt;

	if(camera->key_down)
		camera->pitch-=rotation*dt;

	const float maxVelocity=100.0f;
	const float magnitude=Vec3_Length(camera->velocity);

	// If velocity magnitude is higher than our max, normalize the velocity vector and scale by maximum speed
	if(magnitude>maxVelocity)
		camera->velocity=Vec3_Muls(camera->velocity, (1.0f/magnitude)*maxVelocity);

	// Dampen velocity
	const float damp=powf(0.9f, dt*60.0f);

	camera->velocity=Vec3_Muls(camera->velocity, damp);
	camera->pitch*=damp;
	camera->yaw*=damp;
	camera->roll*=damp;

	// Apply pitch/yaw/roll rotations
	CameraRotate(camera);

	// Combine the velocity with the 3 directional vectors to give overall directional velocity
	const matrix cameraOrientation=
	{
		.x=Vec4(camera->right.x, camera->up.x, camera->forward.x, 0.0f),
		.y=Vec4(camera->right.y, camera->up.y, camera->forward.y, 0.0f),
		.z=Vec4(camera->right.z, camera->up.z, camera->forward.z, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};
	const vec3 velocity=Matrix3x3MultVec3(camera->velocity, MatrixTranspose(cameraOrientation));

	// Integrate the velocity over time to give positional change
	camera->position=Vec3_Addv(camera->position, Vec3_Muls(velocity, dt));

	return MatrixLookAt(camera->position, Vec3_Addv(camera->position, camera->forward), camera->up);
}

// Camera path track stuff
static float blend(int32_t k, int32_t t, int32_t *knots, float v)
{
	float b;

	if(t==1)
	{
		if((knots[k]<=v)&&(v<knots[k+1]))
			b=1.0f;
		else
			b=0.0f;
	}
	else
	{
		if((knots[k+t-1]==knots[k])&&(knots[k+t]==knots[k+1]))
			b=0.0f;
		else
		{
			if(knots[k+t-1]==knots[k])
				b=(knots[k+t]-v)/(knots[k+t]-knots[k+1])*blend(k+1, t-1, knots, v);
			else
			{
				if(knots[k+t]==knots[k+1])
					b=(v-knots[k])/(knots[k+t-1]-knots[k])*blend(k, t-1, knots, v);
				else
					b=(v-knots[k])/(knots[k+t-1]-knots[k])*blend(k, t-1, knots, v)+(knots[k+t]-v)/(knots[k+t]-knots[k+1])*blend(k+1, t-1, knots, v);
			}
		}
	}

	return b;
}

static void CalculateKnots(int32_t *knots, int32_t n, int32_t t)
{
	int32_t i;

	for(i=0;i<=n+t;i++)
	{
		if(i<t)
			knots[i]=0;
		else
		{
			if((t<=i)&&(i<=n))
				knots[i]=i-t+1;
			else
			{
				if(i>n)
					knots[i]=n-t+2;
			}
		}
	}
}

static vec3 CalculatePoint(int32_t *knots, int32_t n, int32_t t, float v, float *control)
{
	int32_t k;
	float b;

	vec3 output=Vec3b(0.0f);

	for(k=0;k<=n;k++)
	{
		b=blend(k, t, knots, v);

		output.x+=control[3*k]*b;
		output.y+=control[3*k+1]*b;
		output.z+=control[3*k+2]*b;
	}

	return output;
}

int32_t CameraLoadPath(char *filename, CameraPath_t *path)
{
	FILE *stream;
	int32_t i;

	path->numPoints=0;

	if((stream=fopen(filename, "rt"))==NULL)
		return 0;

	if(fscanf(stream, "%d", &path->numPoints)!=1)
	{
		fclose(stream);
		return 0;
	}

	path->position=(float *)Zone_Malloc(zone, sizeof(float)*path->numPoints*3);

	if(path->position==NULL)
	{
		fclose(stream);
		return 0;
	}

	path->view=(float *)Zone_Malloc(zone, sizeof(float)*path->numPoints*3);

	if(path->view==NULL)
	{
		Zone_Free(zone, path->position);
		fclose(stream);

		return 0;
	}

	for(i=0;i<path->numPoints;i++)
	{
		if(fscanf(stream, "%f %f %f %f %f %f", &path->position[3*i], &path->position[3*i+1], &path->position[3*i+2], &path->view[3*i], &path->view[3*i+1], &path->view[3*i+2])!=6)
		{
			Zone_Free(zone, path->position);
			Zone_Free(zone, path->view);
			fclose(stream);

			return 0;
		}
	}

	fclose(stream);

	path->time=0.0f;
	path->endTime=(float)(path->numPoints-2);

	path->knots=(int32_t *)Zone_Malloc(zone, sizeof(int32_t)*path->numPoints*3);

	if(path->knots==NULL)
	{
		Zone_Free(zone, path->position);
		Zone_Free(zone, path->view);

		return 0;
	}

	CalculateKnots(path->knots, path->numPoints-1, 3);

	return 1;
}

matrix CameraInterpolatePath(CameraPath_t *path, float dt)
{
	path->time+=dt;

	if(path->time>path->endTime)
		path->time=0.0f;

	vec3 position=CalculatePoint(path->knots, path->numPoints-1, 3, path->time, path->position);
	vec3 view=CalculatePoint(path->knots, path->numPoints-1, 3, path->time, path->view);

	return MatrixLookAt(position, view, Vec3(0.0f, 1.0f, 0.0f));
}

void CameraDeletePath(CameraPath_t *path)
{
	Zone_Free(zone, path->position);
	Zone_Free(zone, path->view);
	Zone_Free(zone, path->knots);
}
