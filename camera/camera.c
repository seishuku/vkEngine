#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../physics/physics.h"
#include "../camera/camera.h"

// Camera collision stuff
int32_t ClassifySphere(vec3 Center, vec3 Normal, vec3 Point, float radius, float *distance)
{
	*distance=Vec3_Dot(Normal, Center)-Vec3_Dot(Normal, Point);

	if(fabsf(*distance)<radius)
		return 1;
	else
	{
		if(*distance>=radius)
			return 2;
	}

	return 0;
}

static bool InsidePolygon(const vec3 Intersection, const vec3 Tri[3])
{
	float Angle=0.0f;
	vec3 A, B, C;

	A=Vec3_Subv(Tri[0], Intersection);
	B=Vec3_Subv(Tri[1], Intersection);
	C=Vec3_Subv(Tri[2], Intersection);

	Angle+=Vec3_GetAngle(A, B);
	Angle+=Vec3_GetAngle(B, C);
	Angle+=Vec3_GetAngle(C, A);

	if(Angle>=6.220353348f)
		return true;

	return false;
}

static void ClosestPointOnLine(vec3 A, vec3 B, vec3 Point, vec3 *ClosestPoint)
{
	vec3 PointDir={ Point.x-A.x, Point.y-A.y, Point.z-A.z };
	vec3 Slope={ B.x-A.y, B.y-A.y, B.z-A.z };
	float d=Vec3_Dot(Slope, Slope), recip_d=0.0f;

	if(d)
		recip_d=1.0f/d;

	float t=fmaxf(0.0f, fminf(1.0f, Vec3_Dot(PointDir, Slope)*recip_d));

	ClosestPoint->x=A.x+t*Slope.x;
	ClosestPoint->y=A.y+t*Slope.y;
	ClosestPoint->z=A.z+t*Slope.z;
}

int32_t EdgeSphereCollision(vec3 Center, vec3 Tri[3], float radius)
{
	int32_t i;
	vec3 Point;

	for(i=0;i<3;i++)
	{
		ClosestPointOnLine(Tri[i], Tri[(i+1)%3], Center, &Point);

		float distance=Vec3_Distance(Point, Center);

		if(distance<radius)
			return 1;
	}

	return 0;
}

vec3 GetCollisionOffset(vec3 Normal, float radius, float distance)
{
	if(distance>0.0f)
		return Vec3_Muls(Normal, radius-distance);
	else
		return Vec3_Muls(Normal, -(radius+distance));
}

bool SphereBBOXIntersection(const vec3 Center, const float Radius, const vec3 BBMin, const vec3 BBMax)
{
	float dmin=0.0f;
	float R2=Radius*Radius;

	if(Center.x<BBMin.x)
		dmin+=(Center.x-BBMin.x)*(Center.x-BBMin.x);
	else
	{
		if(Center.x>BBMax.x)
			dmin+=(Center.x-BBMax.x)*(Center.x-BBMax.x);
	}

	if(Center.y<BBMin.y)
		dmin+=(Center.y-BBMin.y)*(Center.y-BBMin.y);
	else
	{
		if(Center.y>BBMax.y)
			dmin+=(Center.y-BBMax.y)*(Center.y-BBMax.y);
	}

	if(Center.z<BBMin.z)
		dmin+=(Center.z-BBMin.z)*(Center.z-BBMin.z);
	else
	{
		if(Center.z>BBMax.z)
			dmin+=(Center.z-BBMax.z)*(Center.z-BBMax.z);
	}

	if(dmin<=R2)
		return true;

	return false;
}

// Camera<->triangle mesh collision detection and response
void CameraCheckCollision(Camera_t *Camera, float *Vertex, uint32_t *Face, int32_t NumFace)
{
	float distance=0.0f;
	vec3 n;

	for(int32_t i=0;i<NumFace;i++)
	{
		vec3 Tri[3]=
		{
			{ Vertex[3*Face[3*i+0]], Vertex[3*Face[3*i+0]+1], Vertex[3*Face[3*i+0]+2] },
			{ Vertex[3*Face[3*i+1]], Vertex[3*Face[3*i+1]+1], Vertex[3*Face[3*i+1]+2] },
			{ Vertex[3*Face[3*i+2]], Vertex[3*Face[3*i+2]+1], Vertex[3*Face[3*i+2]+2] }
		};

		vec3 v0={ Tri[1].x-Tri[0].x, Tri[1].y-Tri[0].y, Tri[1].z-Tri[0].z };
		vec3 v1={ Tri[2].x-Tri[0].x, Tri[2].y-Tri[0].y, Tri[2].z-Tri[0].z };

		n=Vec3_Cross(v0, v1);

		Vec3_Normalize(&n);

		int32_t classification=ClassifySphere(Camera->Position, n, Tri[0], Camera->Radius, &distance);

		if(classification==1)
		{
			vec3 Intersection=Vec3_Subv(Camera->Position, Vec3_Muls(n, distance));

			if(InsidePolygon(Intersection, Tri)||EdgeSphereCollision(Camera->Position, Tri, Camera->Radius*0.5f))
				Camera->Position=Vec3_Addv(Camera->Position, GetCollisionOffset(n, Camera->Radius, distance));
		}
	}
}

// Actual camera stuff
void CameraInit(Camera_t *Camera, const vec3 Position, const vec3 View, const vec3 Up)
{
	Camera->Position=Vec3_Setv(Position);
	Camera->View=Vec3_Setv(View);
	Camera->Up=Vec3_Setv(Up);

	Camera->Forward=Vec3_Cross(Camera->View, Camera->Up);
	Camera->Right=Vec3_Cross(Camera->Forward, Camera->Up);

	Camera->Pitch=0.0f;
	Camera->Yaw=0.0f;
	Camera->Roll=0.0f;

	Camera->Radius=10.0f;

	Camera->Velocity=Vec3_Sets(0.0f);

	Camera->key_w=0;
	Camera->key_s=0;
	Camera->key_a=0;
	Camera->key_d=0;
	Camera->key_v=0;
	Camera->key_c=0;
	Camera->key_left=0;
	Camera->key_right=0;
	Camera->key_up=0;
	Camera->key_down=0;
}

void CameraPitch(Camera_t *Camera, const float Angle)
{
	vec4 quat;

	QuatAnglev(Angle, Camera->Right, &quat);
	QuatRotate(quat, Camera->Forward, &Camera->Forward);
	Vec3_Normalize(&Camera->Forward);

	Camera->Up=Vec3_Cross(Camera->Forward, Camera->Right);
	Camera->Up=Vec3_Muls(Camera->Up, -1.0f);
}

void CameraYaw(Camera_t *Camera, const float Angle)
{
	vec4 quat;

	QuatAnglev(Angle, Camera->Up, &quat);
	QuatRotate(quat, Camera->Forward, &Camera->Forward);
	Vec3_Normalize(&Camera->Forward);

	Camera->Right=Vec3_Cross(Camera->Forward, Camera->Up);
}

void CameraRoll(Camera_t *Camera, const float Angle)
{
	vec4 quat;

	QuatAnglev(-Angle, Camera->Forward, &quat);
	QuatRotate(quat, Camera->Right, &Camera->Right);
	Vec3_Normalize(&Camera->Right);

	Camera->Up=Vec3_Cross(Camera->Forward, Camera->Right);
	Camera->Up=Vec3_Muls(Camera->Up, -1.0f);
}

void CameraUpdate(Camera_t *Camera, float dt, matrix out)
{
	float speed=2.0f;

	if(!out)
		return;

	if(Camera->shift)
		speed*=2.0f;

	if(Camera->key_d)
		Camera->Velocity.x+=speed;

	if(Camera->key_a)
		Camera->Velocity.x-=speed;

	if(Camera->key_v)
		Camera->Velocity.y+=speed;

	if(Camera->key_c)
		Camera->Velocity.y-=speed;

	if(Camera->key_w)
		Camera->Velocity.z+=speed;

	if(Camera->key_s)
		Camera->Velocity.z-=speed;

	if(Camera->key_q)
		Camera->Roll+=0.125f;

	if(Camera->key_e)
		Camera->Roll-=0.125f;

	if(Camera->key_left)
		Camera->Yaw+=0.125f;

	if(Camera->key_right)
		Camera->Yaw-=0.125f;

	if(Camera->key_up)
		Camera->Pitch+=0.125f;

	if(Camera->key_down)
		Camera->Pitch-=0.125f;

	const float Damp=0.92f;
	Camera->Velocity=Vec3_Muls(Camera->Velocity, Damp);
	Camera->Pitch*=Damp;
	Camera->Yaw*=Damp;
	Camera->Roll*=Damp;

	CameraPitch(Camera, Camera->Pitch*dt);
	CameraYaw(Camera, Camera->Yaw*dt);
	CameraRoll(Camera, Camera->Roll*dt);

	Camera->Position.x+=Camera->Right.x*Camera->Velocity.x*dt;
	Camera->Position.y+=Camera->Right.y*Camera->Velocity.x*dt;
	Camera->Position.z+=Camera->Right.z*Camera->Velocity.x*dt;

	Camera->Position.x+=Camera->Up.x*Camera->Velocity.y*dt;
	Camera->Position.y+=Camera->Up.y*Camera->Velocity.y*dt;
	Camera->Position.z+=Camera->Up.z*Camera->Velocity.y*dt;

	Camera->Position.x+=Camera->Forward.x*Camera->Velocity.z*dt;
	Camera->Position.y+=Camera->Forward.y*Camera->Velocity.z*dt;
	Camera->Position.z+=Camera->Forward.z*Camera->Velocity.z*dt;

	Camera->View.x=Camera->Position.x+Camera->Forward.x;
	Camera->View.y=Camera->Position.y+Camera->Forward.y;
	Camera->View.z=Camera->Position.z+Camera->Forward.z;

	MatrixLookAt(Camera->Position, Camera->View, Camera->Up, out);
}

// Camera path track stuff
float Blend(int32_t k, int32_t t, int32_t *knots, float v)
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
				b=(knots[k+t]-v)/(knots[k+t]-knots[k+1])*Blend(k+1, t-1, knots, v);
			else
			{
				if(knots[k+t]==knots[k+1])
					b=(v-knots[k])/(knots[k+t-1]-knots[k])*Blend(k, t-1, knots, v);
				else
					b=(v-knots[k])/(knots[k+t-1]-knots[k])*Blend(k, t-1, knots, v)+(knots[k+t]-v)/(knots[k+t]-knots[k+1])*Blend(k+1, t-1, knots, v);
			}
		}
	}

	return b;
}

void CalculateKnots(int32_t *knots, int32_t n, int32_t t)
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

void CalculatePoint(int32_t *knots, int32_t n, int32_t t, float v, float *control, vec3 *output)
{
	int32_t k;
	float b;

	*output=Vec3_Sets(0.0f);

	for(k=0;k<=n;k++)
	{
		b=Blend(k, t, knots, v);

		output->x+=control[3*k]*b;
		output->y+=control[3*k+1]*b;
		output->z+=control[3*k+2]*b;
	}
}

int32_t CameraLoadPath(char *filename, CameraPath_t *Path)
{
	FILE *stream;
	int32_t i;

	Path->NumPoints=0;

	if((stream=fopen(filename, "rt"))==NULL)
		return 0;

	if(fscanf(stream, "%d", &Path->NumPoints)!=1)
	{
		fclose(stream);
		return 0;
	}

	Path->Position=(float *)Zone_Malloc(Zone, sizeof(float)*Path->NumPoints*3);

	if(Path->Position==NULL)
	{
		fclose(stream);

		return 0;
	}

	Path->View=(float *)Zone_Malloc(Zone, sizeof(float)*Path->NumPoints*3);

	if(Path->View==NULL)
	{
		Zone_Free(Zone, Path->Position);
		fclose(stream);

		return 0;
	}

	for(i=0;i<Path->NumPoints;i++)
	{
		if(fscanf(stream, "%f %f %f %f %f %f", &Path->Position[3*i], &Path->Position[3*i+1], &Path->Position[3*i+2], &Path->View[3*i], &Path->View[3*i+1], &Path->View[3*i+2])!=6)
		{
			Zone_Free(Zone, Path->Position);
			Zone_Free(Zone, Path->View);
			fclose(stream);

			return 0;
		}
	}

	fclose(stream);

	Path->Time=0.0f;
	Path->EndTime=(float)(Path->NumPoints-2);

	Path->Knots=(int32_t *)Zone_Malloc(Zone, sizeof(int32_t)*Path->NumPoints*3);

	if(Path->Knots==NULL)
	{
		Zone_Free(Zone, Path->Position);
		Zone_Free(Zone, Path->View);
		fclose(stream);

		return 0;
	}

	CalculateKnots(Path->Knots, Path->NumPoints-1, 3);

	return 1;
}

void CameraInterpolatePath(CameraPath_t *Path, Camera_t *Camera, float TimeStep, matrix out)
{
	matrix m;

	if(!out)
		return;

	Path->Time+=TimeStep;

	if(Path->Time>Path->EndTime)
		Path->Time=0.0f;

	CalculatePoint(Path->Knots, Path->NumPoints-1, 3, Path->Time, Path->Position, &Camera->Position);
	CalculatePoint(Path->Knots, Path->NumPoints-1, 3, Path->Time, Path->View, &Camera->View);

	MatrixLookAt(Camera->Position, Camera->View, Camera->Up, m);
	MatrixMult(m, out, out);
}

void CameraDeletePath(CameraPath_t *Path)
{
	Zone_Free(Zone, Path->Position);
	Zone_Free(Zone, Path->View);
	Zone_Free(Zone, Path->Knots);
}
