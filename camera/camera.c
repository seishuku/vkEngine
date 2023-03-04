#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include "../vulkan/vulkan.h"
#include "../system/system.h"
#include "../math/math.h"
#include "../camera/camera.h"

// Camera collision stuff
int32_t ClassifySphere(float Center[3], float Normal[3], float Point[3], float radius, float *distance)
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

int32_t InsidePolygon(float Intersection[3], float Tri[3][3])
{
	float Angle=0.0f;
	float A[3], B[3], C[3];

	A[0]=Tri[0][0]-Intersection[0];
	A[1]=Tri[0][1]-Intersection[1];
	A[2]=Tri[0][2]-Intersection[2];

	B[0]=Tri[1][0]-Intersection[0];
	B[1]=Tri[1][1]-Intersection[1];
	B[2]=Tri[1][2]-Intersection[2];

	C[0]=Tri[2][0]-Intersection[0];
	C[1]=Tri[2][1]-Intersection[1];
	C[2]=Tri[2][2]-Intersection[2];

	Angle+=Vec3_GetAngle(A, B);
	Angle+=Vec3_GetAngle(B, C);
	Angle+=Vec3_GetAngle(C, A);

	if(Angle>=6.220353348f)
		return 1;

	return 0;
}

void ClosestPointOnLine(vec3 A, vec3 B, vec3 Point, vec3 ClosestPoint)
{
	vec3 PointDir={ Point[0]-A[0], Point[1]-A[1], Point[2]-A[2] };
	vec3 Slope={ B[0]-A[0], B[1]-A[1], B[2]-A[2] };
	float d=Vec3_Dot(Slope, Slope), recip_d=0.0f;

	if(d)
		recip_d=1.0f/d;

	float t=fmaxf(0.0f, fminf(1.0f, Vec3_Dot(PointDir, Slope)*recip_d));

	ClosestPoint[0]=A[0]+t*Slope[0];
	ClosestPoint[1]=A[1]+t*Slope[1];
	ClosestPoint[2]=A[2]+t*Slope[2];
}

int32_t EdgeSphereCollision(vec3 Center, float Tri[3][3], float radius)
{
	int32_t i;
	vec3 Point;

	for(i=0;i<3;i++)
	{
		ClosestPointOnLine(Tri[i], Tri[(i+1)%3], Center, Point);

		float distance=Vec3_Distance(Point, Center);

		if(distance<radius)
			return 1;
	}

	return 0;
}

void GetCollisionOffset(vec3 Normal, float radius, float distance, vec3 Offset)
{
	if(distance>0.0f)
	{
		float distanceOver=radius-distance;

		Offset[0]=Normal[0]*distanceOver;
		Offset[1]=Normal[1]*distanceOver;
		Offset[2]=Normal[2]*distanceOver;
	}
	else
	{
		float distanceOver=radius+distance;

		Offset[0]=Normal[0]*-distanceOver;
		Offset[1]=Normal[1]*-distanceOver;
		Offset[2]=Normal[2]*-distanceOver;
	}
}

int32_t SphereBBOXIntersection(const vec3 Center, const float Radius, const vec3 BBMin, const vec3 BBMax)
{
	float dmin=0.0f;
	float R2=Radius*Radius;

	for(int32_t i=0;i<3;i++)
	{
		if(Center[i]<BBMin[i])
			dmin+=(Center[i]-BBMin[i])*(Center[i]-BBMin[i]);
		else
		{
			if(Center[i]>BBMax[i])
				dmin+=(Center[i]-BBMax[i])*(Center[i]-BBMax[i]);
		}
	}

	if(dmin<=R2)
		return 1;

	return 0;
}

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

		vec3 v0={ Tri[1][0]-Tri[0][0], Tri[1][1]-Tri[0][1], Tri[1][2]-Tri[0][2] };
		vec3 v1={ Tri[2][0]-Tri[0][0], Tri[2][1]-Tri[0][1], Tri[2][2]-Tri[0][2] };

		Cross(v0, v1, n);

		Vec3_Normalize(n);

		int32_t classification=ClassifySphere(Camera->Position, n, Tri[0], Camera->Radius, &distance);

		if(classification==1)
		{
			vec3 Offset={ n[0]*distance, n[1]*distance, n[2]*distance };
			vec3 Intersection={ Camera->Position[0]-Offset[0], Camera->Position[1]-Offset[1], Camera->Position[2]-Offset[2] };

			if(InsidePolygon(Intersection, Tri)||EdgeSphereCollision(Camera->Position, Tri, Camera->Radius*0.5f))
			{
				GetCollisionOffset(n, Camera->Radius, distance, Offset);

				Camera->Position[0]+=Offset[0];
				Camera->Position[1]+=Offset[1];
				Camera->Position[2]+=Offset[2];

				Camera->View[0]+=Offset[0];
				Camera->View[1]+=Offset[1];
				Camera->View[2]+=Offset[2];
			}
		}
	}
}

// Actual camera stuff
void CameraInit(Camera_t *Camera, const vec3 Position, const vec3 View, const vec3 Up)
{
	Camera->Position[0]=Position[0];
	Camera->Position[1]=Position[1];
	Camera->Position[2]=Position[2];

	Camera->View[0]=View[0];
	Camera->View[1]=View[1];
	Camera->View[2]=View[2];

	Camera->Up[0]=Up[0];
	Camera->Up[1]=Up[1];
	Camera->Up[2]=Up[2];

	Cross(Camera->View, Camera->Up, Camera->Forward);
	Cross(Camera->Forward, Camera->Up, Camera->Right);

	Camera->Pitch=0.0f;
	Camera->Yaw=0.0f;
	Camera->Roll=0.0f;

	Camera->Radius=10.0f;

	Camera->Velocity[0]=0.0f;
	Camera->Velocity[1]=0.0f;
	Camera->Velocity[2]=0.0f;

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

	QuatAnglev(Angle, Camera->Right, quat);
	QuatRotate(quat, Camera->Forward, Camera->Forward);
	Vec3_Normalize(Camera->Forward);

	Cross(Camera->Forward, Camera->Right, Camera->Up);
	Camera->Up[0]*=-1;
	Camera->Up[1]*=-1;
	Camera->Up[2]*=-1;
}

void CameraYaw(Camera_t *Camera, const float Angle)
{
	vec4 quat;

	QuatAnglev(Angle, Camera->Up, quat);
	QuatRotate(quat, Camera->Forward, Camera->Forward);
	Vec3_Normalize(Camera->Forward);

	Cross(Camera->Forward, Camera->Up, Camera->Right);
}

void CameraRoll(Camera_t *Camera, const float Angle)
{
	float quat[4];

	QuatAnglev(-Angle, Camera->Forward, quat);
	QuatRotate(quat, Camera->Right, Camera->Right);
	Vec3_Normalize(Camera->Right);

	Cross(Camera->Forward, Camera->Right, Camera->Up);
	Camera->Up[0]*=-1;
	Camera->Up[1]*=-1;
	Camera->Up[2]*=-1;
}

void CameraUpdate(Camera_t *Camera, float Time, matrix out)
{
	float speed=2.0f;
	float m[16];

	if(!out)
		return;

	if(Camera->shift)
		speed*=2.0f;

	if(Camera->key_d)
		Camera->Velocity[0]+=Time;

	if(Camera->key_a)
		Camera->Velocity[0]-=Time;

	if(Camera->key_v)
		Camera->Velocity[1]+=Time;

	if(Camera->key_c)
		Camera->Velocity[1]-=Time;

	if(Camera->key_w)
		Camera->Velocity[2]+=Time;

	if(Camera->key_s)
		Camera->Velocity[2]-=Time;

	if(Camera->key_q)
		Camera->Roll+=Time*0.125f;

	if(Camera->key_e)
		Camera->Roll-=Time*0.125f;

	if(Camera->key_left)
		Camera->Yaw+=Time*0.125f;

	if(Camera->key_right)
		Camera->Yaw-=Time*0.125f;

	if(Camera->key_up)
		Camera->Pitch+=Time*0.125f;

	if(Camera->key_down)
		Camera->Pitch-=Time*0.125f;

	const float Damp=0.91f;//Time*65.0f;
	Camera->Velocity[0]*=Damp;
	Camera->Velocity[1]*=Damp;
	Camera->Velocity[2]*=Damp;
	Camera->Pitch*=Damp;
	Camera->Yaw*=Damp;
	Camera->Roll*=Damp;

	CameraPitch(Camera, Camera->Pitch);
	CameraYaw(Camera, Camera->Yaw);
	CameraRoll(Camera, Camera->Roll);

	Camera->Position[0]+=Camera->Right[0]*speed*Camera->Velocity[0];
	Camera->Position[1]+=Camera->Right[1]*speed*Camera->Velocity[0];
	Camera->Position[2]+=Camera->Right[2]*speed*Camera->Velocity[0];

	Camera->Position[0]+=Camera->Up[0]*speed*Camera->Velocity[1];
	Camera->Position[1]+=Camera->Up[1]*speed*Camera->Velocity[1];
	Camera->Position[2]+=Camera->Up[2]*speed*Camera->Velocity[1];

	Camera->Position[0]+=Camera->Forward[0]*speed*Camera->Velocity[2];
	Camera->Position[1]+=Camera->Forward[1]*speed*Camera->Velocity[2];
	Camera->Position[2]+=Camera->Forward[2]*speed*Camera->Velocity[2];

	Camera->View[0]=Camera->Position[0]+Camera->Forward[0];
	Camera->View[1]=Camera->Position[1]+Camera->Forward[1];
	Camera->View[2]=Camera->Position[2]+Camera->Forward[2];

	MatrixLookAt(Camera->Position, Camera->View, Camera->Up, m);
	MatrixMult(m, out, out);
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

void CalculatePoint(int32_t *knots, int32_t n, int32_t t, float v, float *control, float *output)
{
	int32_t k;
	float b;

	output[0]=output[1]=output[2]=0.0f;

	for(k=0;k<=n;k++)
	{
		b=Blend(k, t, knots, v);

		output[0]+=control[3*k]*b;
		output[1]+=control[3*k+1]*b;
		output[2]+=control[3*k+2]*b;
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

	CalculatePoint(Path->Knots, Path->NumPoints-1, 3, Path->Time, Path->Position, Camera->Position);
	CalculatePoint(Path->Knots, Path->NumPoints-1, 3, Path->Time, Path->View, Camera->View);

	MatrixLookAt(Camera->Position, Camera->View, Camera->Up, m);
	MatrixMult(m, out, out);
}

void CameraDeletePath(CameraPath_t *Path)
{
	Zone_Free(Zone, Path->Position);
	Zone_Free(Zone, Path->View);
	Zone_Free(Zone, Path->Knots);
}
