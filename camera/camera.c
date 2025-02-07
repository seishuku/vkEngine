#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../physics/physics.h"

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
	frustumPlanes[0]=calculatePlane(Vec3_Addv(camera.body.position, Vec3_Muls(camera.forward, nearPlane)), camera.forward);
	frustumPlanes[1]=calculatePlane(Vec3_Addv(camera.body.position, forward_far), Vec3_Muls(camera.forward, -1.0f));

	frustumPlanes[2]=calculatePlane(camera.body.position, Vec3_Cross(camera.up, Vec3_Addv(forward_far, Vec3_Muls(camera.right, half_h))));
	frustumPlanes[3]=calculatePlane(camera.body.position, Vec3_Cross(Vec3_Subv(forward_far, Vec3_Muls(camera.right, half_h)), camera.up));
	frustumPlanes[4]=calculatePlane(camera.body.position, Vec3_Cross(camera.right, Vec3_Subv(forward_far, Vec3_Muls(camera.up, half_v))));
	frustumPlanes[5]=calculatePlane(camera.body.position, Vec3_Cross(Vec3_Addv(forward_far, Vec3_Muls(camera.up, half_v)), camera.right));
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

		if(ClassifySphere(camera->body.position, normal, triangle[0], camera->body.radius, &distance)==1)
		{
			const vec3 intersection=Vec3_Subv(camera->body.position, Vec3_Muls(normal, distance));

			if(InsidePolygon(intersection, triangle)||EdgeSphereCollision(camera->body.position, triangle, camera->body.radius*0.5f))
				camera->body.position=Vec3_Addv(camera->body.position, GetCollisionOffset(normal, camera->body.radius, distance));
		}
	}
}

// Actual camera stuff
void CameraInit(Camera_t *camera, const vec3 position, const vec3 up, const vec3 forward)
{
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
	float speed=240.0f*dt;
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

	const float maxVelocity=100.0f;
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

	return MatrixLookAt(camera->body.position, Vec3_Addv(camera->body.position, camera->forward), camera->up);
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
