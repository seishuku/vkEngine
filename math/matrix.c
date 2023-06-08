#include "math.h"

// I'm not a huge fan of this layout, so here's a...
// Matrix cheat sheet
// .x.x=[ 0] .x.y=[ 1] .x.z=[ 2] .x.w=[ 3]
// .y.x=[ 4] .y.y=[ 5] .y.z=[ 6] .y.w=[ 7]
// .z.x=[ 8] .z.y=[ 9] .z.z=[10] .z.w=[11]
// .w.x=[12] .w.y=[13] .w.z=[14] .w.w=[15]

matrix MatrixIdentity(void)
{
	return (matrix)
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
}

matrix MatrixMult(const matrix a, const matrix b)
{
	return (matrix)
	{
		{
			a.x.x *b.x.x+a.x.y*b.y.x+a.x.z*b.z.x+a.x.w*b.w.x,
			a.x.x *b.x.y+a.x.y*b.y.y+a.x.z*b.z.y+a.x.w*b.w.y,
			a.x.x *b.x.z+a.x.y*b.y.z+a.x.z*b.z.z+a.x.w*b.w.z,
			a.x.x *b.x.w+a.x.y*b.y.w+a.x.z*b.z.w+a.x.w*b.w.w
		},
		{
			a.y.x*b.x.x+a.y.y*b.y.x+a.y.z*b.z.x+a.y.w*b.w.x,
			a.y.x*b.x.y+a.y.y*b.y.y+a.y.z*b.z.y+a.y.w*b.w.y,
			a.y.x*b.x.z+a.y.y*b.y.z+a.y.z*b.z.z+a.y.w*b.w.z,
			a.y.x*b.x.w+a.y.y*b.y.w+a.y.z*b.z.w+a.y.w*b.w.w
		},
		{
			a.z.x*b.x.x+a.z.y*b.y.x+a.z.z*b.z.x+a.z.w*b.w.x,
			a.z.x*b.x.y+a.z.y*b.y.y+a.z.z*b.z.y+a.z.w*b.w.y,
			a.z.x*b.x.z+a.z.y*b.y.z+a.z.z*b.z.z+a.z.w*b.w.z,
			a.z.x*b.x.w+a.z.y*b.y.w+a.z.z*b.z.w+a.z.w*b.w.w
		},
		{
			a.w.x*b.x.x+a.w.y*b.y.x+a.w.z*b.z.x+a.w.w*b.w.x,
			a.w.x*b.x.y+a.w.y*b.y.y+a.w.z*b.z.y+a.w.w*b.w.y,
			a.w.x*b.x.z+a.w.y*b.y.z+a.w.z*b.z.z+a.w.w*b.w.z,
			a.w.x*b.x.w+a.w.y*b.y.w+a.w.z*b.z.w+a.w.w*b.w.w
		}
	};
}

matrix MatrixInverse(const matrix in)
{
	return (matrix)
	{
		{ in.x.x, in.y.x, in.z.x, 0.0f },
		{ in.x.y, in.y.y, in.z.y, 0.0f },
		{ in.x.z, in.y.z, in.z.z, 0.0f },
		{
			-(in.w.x*in.x.x)-(in.w.y*in.x.y)-(in.w.z*in.x.z),
			-(in.w.x*in.y.x)-(in.w.y*in.y.y)-(in.w.z*in.y.z),
			-(in.w.x*in.z.x)-(in.w.y*in.z.y)-(in.w.z*in.z.z),
			1.0f
		}
	};
}

matrix MatrixTranspose(const matrix in)
{
	return (matrix)
	{
		{ in.x.x, in.y.x, in.z.x, in.w.x },
		{ in.x.y, in.y.y, in.z.y, in.w.y },
		{ in.x.z, in.y.z, in.z.z, in.w.z },
		{ in.x.w, in.y.w, in.z.w, in.w.w }
	};
}

matrix MatrixRotate(const float angle, const float x, const float y, const float z)
{
	float c=cosf(angle);
	float s=sinf(angle);

	float temp[3]={ (1.0f-c)*x, (1.0f-c)*y, (1.0f-c)*z };

	return (matrix)
	{
		{ c+temp[0]*x, temp[0]*y+s*z, temp[0]*z-s*y, 0.0f },
		{ temp[1]*x-s*z, c+temp[1]*y, temp[1]*z+s*x, 0.0f },
		{ temp[2]*x+s*y, temp[2]*y-s*x, c+temp[2]*z, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
}

matrix MatrixRotatev(const float angle, const vec3 v)
{
	return MatrixRotate(angle, v.x, v.y, v.z);
}

matrix MatrixTranslate(const float x, const float y, const float z)
{
	return (matrix)
	{
		{ 1.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 1.0f, 0.0f },
		{    x,    y,    z, 1.0f }
	};
}

matrix MatrixTranslatev(const vec3 v)
{
	return MatrixTranslate(v.x, v.y, v.z);
}

matrix MatrixScale(const float x, const float y, const float z)
{
	return (matrix)
	{
		{    x, 0.0f, 0.0f, 0.0f },
		{ 0.0f,    y, 0.0f, 0.0f },
		{ 0.0f, 0.0f,    z, 0.0f },
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	};
}

matrix MatrixScalev(const vec3 v)
{
	return MatrixScale(v.x, v.y, v.z);
}

matrix MatrixAlignPoints(const vec3 start, const vec3 end, const vec3 up)
{
	// Find the direction of the start point and end point, then normalize it.
	vec3 direction=Vec3_Subv(end, start);
	Vec3_Normalize(&direction);

	// Get the cross product between the direction
	// and the object's current orientation, and normalize that.
	// That vector is the axis of rotation
	vec3 axis=Vec3_Cross(direction, up);
	Vec3_Normalize(&axis);

	// direction.orientation=cos(angle), so arccos to get angle between
	// the new direction and the static orientation.
	float angle=acosf(Vec3_Dot(direction, up));

	// Use that angle to build a rotation and translation matrix to reorient it.
	float s=sinf(angle);
	float c=cosf(angle);
	float c1=1.0f-c;

	return (matrix)
	{
		{ c+axis.x*axis.x*c1,			axis.y*axis.x*c1+axis.z*s,	axis.z*axis.x*c1-axis.y*s,	0.0f },
		{ axis.x*axis.y*c1-axis.z*s,	c+axis.y*axis.y*c1,			axis.z*axis.y *c1+axis.x*s,	0.0f },
		{ axis.x*axis.z*c1+axis.y*s,	axis.y*axis.z*c1-axis.x*s,	c+axis.z*axis.z*c1,			0.0f },
		{ start.x,						start.y,					start.z,					1.0f }
	};
}

vec4 Matrix4x4MultVec4(const vec4 in, const matrix m)
{
	return (vec4)
	{
		in.x*m.x.x+in.y*m.y.x+in.z*m.z.x+in.w*m.w.x,
		in.x*m.x.y+in.y*m.y.y+in.z*m.z.y+in.w*m.w.y,
		in.x*m.x.z+in.y*m.y.z+in.z*m.z.z+in.w*m.w.z,
		in.x*m.x.w+in.y*m.y.w+in.z*m.z.w+in.w*m.w.w
	};
}

vec3 Matrix4x4MultVec3(const vec3 in, const matrix m)
{
	return (vec3)
	{
		in.x*m.x.x+in.y*m.y.x+in.z*m.z.x+m.w.x,
		in.x*m.x.y+in.y*m.y.y+in.z*m.z.y+m.w.y,
		in.x*m.x.z+in.y*m.y.z+in.z*m.z.z+m.w.z
	};
}

vec3 Matrix3x3MultVec3(const vec3 in, const matrix m)
{
	return (vec3)
	{
		in.x*m.x.x+in.y*m.y.x+in.z*m.z.x,
		in.x*m.x.y+in.y*m.y.y+in.z*m.z.y,
		in.x*m.x.z+in.y*m.y.z+in.z*m.z.z
	};
}

// TODO?: Should this multiply with the supplied matrix like the other functions?
matrix MatrixLookAt(const vec3 position, const vec3 forward, const vec3 up)
{
	vec3 f=Vec3_Subv(forward, position);
	vec3 u=up, s;

	Vec3_Normalize(&u);
	Vec3_Normalize(&f);
	s=Vec3_Cross(f, u);
	Vec3_Normalize(&s);
	u=Vec3_Cross(s, f);

	return (matrix)
	{
		{ s.x, u.x, -f.x, 0.0f },
		{ s.y, u.y, -f.y, 0.0f },
		{ s.z, u.z, -f.z, 0.0f },
		{
			-Vec3_Dot(s, position),
			-Vec3_Dot(u, position),
			Vec3_Dot(f, position),
			1.0f
		}
	};
}

// Projection matrix functions, these are set up for "z reverse" (depth cleared to 0.0, and greater than or equal depth test)
matrix MatrixInfPerspective(const float fovy, const float aspect, const float zNear)
{
	const float focal=tanf((fovy*PI/180.0f)*0.5f);

	return (matrix)
	{
		{ 1.0f/(aspect*focal),  0.0f,        0.0f,  0.0f },
		{ 0.0f,                 -1.0f/focal, 0.0f,  0.0f },
		{ 0.0f,                 0.0f,        0.0f,  -1.0f },
		{ 0.0f,                 0.0f,        zNear, 0.0f }
	};
}

matrix MatrixPerspective(const float fovy, const float aspect, const float zNear, const float zFar)
{
	const float focal=tanf((fovy*PI/180.0f)*0.5f);

	return (matrix)
	{
		{ 1.0f/(aspect*focal),  0.0f,        0.0f,                       0.0f },
		{ 0.0f,                 -1.0f/focal, 0.0f,                       0.0f },
		{ 0.0f,                 0.0f,        -2.0f/(zFar-zNear),         -1.0f },
		{ 0.0f,                 0.0f,        -(zFar+zNear)/(zFar-zNear), 0.0f },
	};
}

// This is *not* set up for "z reverse"
matrix MatrixOrtho(const float left, const float right, const float bottom, const float top, const float zNear, const float zFar)
{
	return (matrix)
	{
		{ 2.0f/(right-left),          0.0f,                       0.0f,               0.0f },
		{ 0.0f,                       2.0f/(bottom-top),          0.0f,               0.0f },
		{ 0.0f,                       0.0f,                       1.0f/(zNear-zFar),  0.0f },
		{ -(right+left)/(right-left), -(bottom+top)/(bottom-top), zNear/(zNear-zFar), 1.0f }
	};
}
