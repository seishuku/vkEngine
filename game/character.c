#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "../math/math.h"
#include "../physics/physics.h"
#include "../camera/camera.h"
#include "character.h"

static vec3 ProjectOnPlane(const vec3 v, const vec3 normal)
{
	return Vec3_Subv(v, Vec3_Muls(normal, Vec3_Dot(v, normal)));
}

static vec3 NormalizeOr(const vec3 v, const vec3 fallback)
{
	const float length=Vec3_Length(v);

	if(length<FLT_EPSILON)
		return fallback;

	return Vec3_Muls(v, 1.0f/length);
}

static vec3 AnyPerpendicular(const vec3 n)
{
	const vec3 axis=(fabsf(n.y)<0.9f)?Vec3(0.0f, 1.0f, 0.0f):Vec3(1.0f, 0.0f, 0.0f);

	return NormalizeOr(ProjectOnPlane(axis, n), Vec3(1.0f, 0.0f, 0.0f));
}

static float SmoothDamp(const float target, float *velocity, const float smoothTime, const float maxSpeed, const float dt)
{
	const float time=fmaxf(smoothTime, 0.0001f);
	const float omega=2.0f/time;
	const float x=omega*dt;
	const float decay=1.0f/(1.0f+x+0.48f*x*x+0.235f*x*x*x);

	const float maxChange=maxSpeed*time;
	const float change=clampf(-target, -maxChange, maxChange);
	const float limitedTarget=-change;

	const float temp=(*velocity+omega*change)*dt;
	*velocity=(*velocity-omega*temp)*decay;

	float output=limitedTarget+(change+temp)*decay;

	if((target>0.0f)==(output>target))
	{
		output=target;
		*velocity=0.0f;
	}

	return output;
}

static vec3 TransportVector(const vec3 v, const vec3 from, const vec3 to)
{
	const float cosAngle=clampf(Vec3_Dot(from, to), -1.0f, 1.0f);

	if(cosAngle>0.999999f)
		return v;

	vec3 axis=Vec3_Cross(from, to);
	float axisLength=Vec3_Length(axis);

	if(axisLength<FLT_EPSILON)
	{
		axis=Vec3_Cross(from, (fabsf(from.y)<0.9f)?Vec3(0.0f, 1.0f, 0.0f):Vec3(1.0f, 0.0f, 0.0f));
		axisLength=Vec3_Length(axis);
	}

	return QuatRotate(QuatAnglev(acosf(cosAngle), Vec3_Muls(axis, 1.0f/axisLength)), v);
}

static vec4 OrientationFromUpFacing(const vec3 up, const vec3 facing)
{
	const matrix m=
	{
		.x=Vec4_Vec3(Vec3_Cross(up, facing), 0.0f),
		.y=Vec4_Vec3(up, 0.0f),
		.z=Vec4_Vec3(facing, 0.0f),
		.w=Vec4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	return MatrixToQuat(m);
}

void Character_Init(Character_t *c, const vec3 position, const vec3 up, const vec3 facing, const float height)
{
	memset(c, 0, sizeof(Character_t));

	const float radius=height*0.18f;
	const float halfHeight=fmaxf(height*0.5f-radius, 0.0f);

	c->up=NormalizeOr(up, Vec3(0.0f, 1.0f, 0.0f));
	c->facing=NormalizeOr(ProjectOnPlane(facing, c->up), AnyPerpendicular(c->up));
	c->groundNormal=c->up;

	c->walkSpeed=3.5f*WORLD_SCALE;
	c->runSpeed=6.0f*WORLD_SCALE;
	c->groundAcceleration=30.0f*WORLD_SCALE;
	c->groundDeceleration=40.0f*WORLD_SCALE;
	c->airAcceleration=8.0f*WORLD_SCALE;
	c->jumpSpeed=4.5f*WORLD_SCALE;
	c->maxSlopeCos=cosf(50.0f*(PI/180.0f));
	c->coyoteTime=0.1f;
	c->upSmoothing=8.0f;
	c->turnRate=10.0f;
	c->turnSmoothTime=0.15f;
	c->landTime=0.3f;
	c->minLandSpeed=2.0f*WORLD_SCALE;
	c->minLandAirTime=0.15f;

	c->timeSinceGrounded=1.0f;

	c->body.position=position;
	c->body.velocity=Vec3b(0.0f);
	c->body.force=Vec3b(0.0f);
	c->body.mass=0.01f;
	c->body.invMass=1.0f/c->body.mass;

	c->body.orientation=OrientationFromUpFacing(c->up, c->facing);
	c->body.angularVelocity=Vec3b(0.0f);
	c->body.inertia=0.0f;
	c->body.invInertia=0.0f;

	c->body.restitution=0.0f;
	c->body.friction=0.0f;

	c->body.type=RIGIDBODY_CAPSULE;
	c->body.radiusHeight=Vec2(radius, halfHeight);
}

void Character_ResetGround(Character_t *c)
{
	c->gravityAccum=Vec3b(0.0f);
	c->groundNormalAccum=Vec3b(0.0f);
	c->groundContacts=0;
}

void Character_AddGravity(Character_t *c, const vec3 gravity)
{
	c->gravityAccum=Vec3_Addv(c->gravityAccum, gravity);
}

void Character_AddContact(Character_t *c, const vec3 normal, const float penetration)
{
	if(!c->active)
		return;

	if(Vec3_Dot(normal, c->up)<c->maxSlopeCos)
		return;

	c->groundNormalAccum=Vec3_Addv(c->groundNormalAccum, Vec3_Muls(normal, fmaxf(penetration, 0.001f)));
	c->groundContacts++;
}

void Character_Update(Character_t *c, const Camera_t *camera, const float dt)
{
	if(!c->active||dt<=0.0f)
		return;

	RigidBody_t *body=&c->body;

	const float gravityLength=Vec3_Length(c->gravityAccum);
	const bool hasGravity=gravityLength>0.001f;
	const vec3 previousUp=c->up;

	if(hasGravity)
	{
		const vec3 targetUp=Vec3_Muls(c->gravityAccum, -1.0f/gravityLength);
		const float blend=1.0f-expf(-c->upSmoothing*dt);

		c->up=NormalizeOr(Vec3_Lerp(c->up, targetUp, blend), targetUp);
	}

	c->facing=TransportVector(c->facing, previousUp, c->up);
	c->facing=NormalizeOr(ProjectOnPlane(c->facing, c->up), AnyPerpendicular(c->up));

	c->groundIgnoreTime=fmaxf(c->groundIgnoreTime-dt, 0.0f);
	c->justLanded=false;

	const bool wasGrounded=c->grounded;
	const float airTime=c->timeSinceGrounded;

	const float verticalSpeed=Vec3_Dot(body->velocity, c->up);

	if(c->groundContacts>0&&c->groundIgnoreTime<=0.0f&&verticalSpeed<c->jumpSpeed*0.5f)
	{
		c->grounded=true;
		c->groundNormal=NormalizeOr(c->groundNormalAccum, c->up);
		c->timeSinceGrounded=0.0f;
	}
	else
	{
		c->grounded=false;
		c->groundNormal=c->up;
		c->timeSinceGrounded+=dt;
		c->landTimer=0.0f;
	}

	if(c->grounded&&!wasGrounded&&airTime>=c->minLandAirTime&&c->downSpeed>=c->minLandSpeed)
	{
		c->justLanded=true;
		c->landImpact=c->downSpeed;
		c->landTimer=c->landTime;
	}

	const bool jumpPressed=camera->moveUp&&!c->jumpHeld;
	c->jumpHeld=camera->moveUp;

	if(jumpPressed&&hasGravity&&(c->grounded||c->timeSinceGrounded<c->coyoteTime))
	{
		body->velocity=Vec3_Addv(ProjectOnPlane(body->velocity, c->up), Vec3_Muls(c->up, c->jumpSpeed));

		c->grounded=false;
		c->groundNormal=c->up;
		c->timeSinceGrounded=c->coyoteTime;
		c->groundIgnoreTime=0.15f;
	}

	if(hasGravity)
		body->velocity=Vec3_Addv(body->velocity, Vec3_Muls(c->gravityAccum, dt));

	const vec3 heading=NormalizeOr(ProjectOnPlane(camera->heading, c->up), c->facing);
	const vec3 side=Vec3_Cross(c->up, heading);

	const float forwardInput=(camera->moveForward?1.0f:0.0f)-(camera->moveBackward?1.0f:0.0f);
	const float sideInput=(camera->moveLeft?1.0f:0.0f)-(camera->moveRight?1.0f:0.0f);

	vec3 wish=Vec3_Addv(Vec3_Muls(heading, forwardInput), Vec3_Muls(side, sideInput));
	float wishLength=Vec3_Length(wish);

	if(wishLength>1.0f)
	{
		wish=Vec3_Muls(wish, 1.0f/wishLength);
		wishLength=1.0f;
	}

	const vec3 planeNormal=c->grounded?c->groundNormal:c->up;

	vec3 wishPlane=ProjectOnPlane(wish, planeNormal);
	const float planeLength=Vec3_Length(wishPlane);

	wishPlane=(planeLength>1e-5f)?Vec3_Muls(wishPlane, wishLength/planeLength):Vec3b(0.0f);

	const float maxSpeed=camera->shift?c->runSpeed:c->walkSpeed;
	const vec3 targetVelocity=Vec3_Muls(wishPlane, maxSpeed);

	const vec3 velocityNormal=Vec3_Muls(planeNormal, Vec3_Dot(body->velocity, planeNormal));
	vec3 velocityPlane=Vec3_Subv(body->velocity, velocityNormal);

	float rate;

	if(c->grounded)
		rate=(wishLength>0.001f)?c->groundAcceleration:c->groundDeceleration;
	else
		rate=(wishLength>0.001f)?c->airAcceleration:0.0f;

	const vec3 delta=Vec3_Subv(targetVelocity, velocityPlane);
	const float maxDelta=rate*dt;
	const float length=Vec3_Length(delta);

	if(length>maxDelta||length>FLT_EPSILON)
		velocityPlane=Vec3_Addv(velocityPlane, Vec3_Muls(delta, maxDelta/length));

	body->velocity=Vec3_Addv(velocityPlane, velocityNormal);

	if(camera->orbitDistance<=0.001f)
	{
		c->facing=heading;
		c->turnVelocity=0.0f;
	}
	else
	{
		const vec3 desiredFacing=(wishLength>0.001f)?NormalizeOr(ProjectOnPlane(wish, c->up), c->facing):c->facing;

		const float cosAngle=clampf(Vec3_Dot(c->facing, desiredFacing), -1.0f, 1.0f);
		const float sinAngle=Vec3_Dot(Vec3_Cross(c->facing, desiredFacing), c->up);
		const float error=atan2f(sinAngle, cosAngle);

		const float turn=SmoothDamp(error, &c->turnVelocity, c->turnSmoothTime, c->turnRate, dt);

		c->facing=NormalizeOr(QuatRotate(QuatAnglev(turn, c->up), c->facing), c->facing);
	}

	body->orientation=OrientationFromUpFacing(c->up, c->facing);
	body->angularVelocity=Vec3b(0.0f);

	c->speed=Vec3_Length(velocityPlane);

	if(c->speed>=c->walkSpeed*0.5f)
		c->landTimer=0.0f;

	c->landTimer=fmaxf(c->landTimer-dt, 0.0f);

	c->downSpeed=-Vec3_Dot(body->velocity, c->up);

	if(c->grounded||c->timeSinceGrounded<0.08f)
	{
		if(c->landTimer>0.0f)
			c->state=CHARACTER_STATE_LAND;
		else if(c->speed<c->walkSpeed*0.15f)
			c->state=CHARACTER_STATE_IDLE;
		else if(c->speed<=c->walkSpeed*1.25f)
			c->state=CHARACTER_STATE_WALK;
		else
			c->state=CHARACTER_STATE_RUN;
	}
	else
		c->state=(Vec3_Dot(body->velocity, c->up)>0.0f)?CHARACTER_STATE_JUMP:CHARACTER_STATE_FALL;
}
