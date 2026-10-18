#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include "../camera/camera.h"
#include "../math/math.h"
#include "../physics/physics.h"
#include "../system/system.h"
#include "character.h"

#define GRAVITY_EPSILON				0.001f	// Gravity weaker than this is treated as free space
#define INPUT_EPSILON				0.001f	// Move input shorter than this is treated as no input
#define JUMP_GROUND_IGNORE_TIME		0.15f	// Time after a jump that ground contacts are ignored, so the jump can leave the surface
#define JUMP_GROUNDED_SPEED_RATIO	0.5f	// Rising faster than this fraction of jumpSpeed can't count as grounded
#define GROUND_STATE_GRACE_TIME		0.08f	// Time after leaving the ground that ground states are held, avoids flicker over bumps
#define IDLE_SPEED_RATIO			0.15f	// Below this fraction of walkSpeed is idle
#define RUN_SPEED_RATIO				1.25f	// Above this fraction of walkSpeed is running
#define LAND_CANCEL_SPEED_RATIO		0.5f	// Moving faster than this fraction of walkSpeed cancels the landing state

typedef struct
{
	vec3 heading;		// Camera heading flattened onto the up plane
	vec3 wish;			// Desired move direction, length 0 to 1
	float wishLength;
	bool hasInput;
	float maxSpeed;		// Walk or run speed
} MoveInput_t;

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

// Moves 'current' toward 'target' by at most maxDelta, without overshooting
static vec3 MoveTowards(const vec3 current, const vec3 target, const float maxDelta)
{
	const vec3 delta=Vec3_Subv(target, current);
	const float length=Vec3_Length(delta);

	if(length<=maxDelta||length<FLT_EPSILON)
		return target;

	return Vec3_Addv(current, Vec3_Muls(delta, maxDelta/length));
}

// Angle to rotate 'from' around 'axis' to reach 'to', in radians (-PI to PI)
static float SignedAngle(const vec3 from, const vec3 to, const vec3 axis)
{
	return atan2f(Vec3_Dot(Vec3_Cross(from, to), axis), Vec3_Dot(from, to));
}

// Critically damped spring on an angular error (target) that is driven toward zero.
// Returns the angle to turn this frame, velocity carries between calls.
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

	// Don't overshoot
	if((target>0.0f)==(output>target))
	{
		output=target;
		*velocity=0.0f;
	}

	return output;
}

// Parallel transport, rotates 'v' by the shortest rotation that takes 'from' to 'to'
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

	RigidBody_t *body=&c->body;

	body->position=position;
	body->velocity=Vec3b(0.0f);
	body->force=Vec3b(0.0f);
	body->mass=0.01f;
	body->invMass=1.0f/body->mass;

	body->orientation=OrientationFromUpFacing(c->up, c->facing);
	body->angularVelocity=Vec3b(0.0f);
	body->inertia=0.0f;
	body->invInertia=0.0f;

	body->restitution=0.0f;
	body->friction=0.0f;

	body->type=RIGIDBODY_CAPSULE;
	body->radiusHeight=Vec2(radius, halfHeight);
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

	// Too steep to stand on
	if(Vec3_Dot(normal, c->up)<c->maxSlopeCos)
		return;

	c->groundNormalAccum=Vec3_Addv(c->groundNormalAccum, Vec3_Muls(normal, fmaxf(penetration, 0.001f)));
	c->groundContacts++;
}

// Eases 'up' toward the direction opposing gravity, and facing direction along with it
static void UpdateUp(Character_t *c, const bool hasGravity, const float dt)
{
	const vec3 previousUp=c->up;

	if(hasGravity)
	{
		const vec3 targetUp=NormalizeOr(Vec3_Muls(c->gravityAccum, -1.0f), c->up);
		const float blend=1.0f-expf(-c->upSmoothing*dt);

		c->up=NormalizeOr(Vec3_Lerp(c->up, targetUp, blend), targetUp);
	}

	c->facing=TransportVector(c->facing, previousUp, c->up);
	c->facing=NormalizeOr(ProjectOnPlane(c->facing, c->up), AnyPerpendicular(c->up));
}

static void UpdateGroundState(Character_t *c, const float dt)
{
	c->groundIgnoreTime=fmaxf(c->groundIgnoreTime-dt, 0.0f);

	const float verticalSpeed=Vec3_Dot(c->body.velocity, c->up);
	const bool touching=c->groundContacts>0&&c->groundIgnoreTime<=0.0f&&verticalSpeed<c->jumpSpeed*JUMP_GROUNDED_SPEED_RATIO;

	if(touching)
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
}

// Flags a touchdown, but only if the character was airborne long enough and fast enough for it to count
static void UpdateLanding(Character_t *c, const bool wasGrounded, const float airTime)
{
	c->justLanded=false;

	if(c->grounded&&!wasGrounded&&airTime>=c->minLandAirTime&&c->downSpeed>=c->minLandSpeed)
	{
		c->justLanded=true;
		c->landImpact=c->downSpeed;
		c->landTimer=c->landTime;
	}
}

static void UpdateJump(Character_t *c, const Camera_t *camera, const bool hasGravity)
{
	const bool jumpPressed=camera->moveUp&&!c->jumpHeld;

	c->jumpHeld=camera->moveUp;

	if(!jumpPressed||!hasGravity)
		return;

	// Allow a short grace period after walking off a ledge ("coyote time")
	if(!c->grounded&&c->timeSinceGrounded>=c->coyoteTime)
		return;

	c->body.velocity=Vec3_Addv(ProjectOnPlane(c->body.velocity, c->up), Vec3_Muls(c->up, c->jumpSpeed));

	c->grounded=false;
	c->groundNormal=c->up;
	c->timeSinceGrounded=c->coyoteTime;
	c->groundIgnoreTime=JUMP_GROUND_IGNORE_TIME;
}

static MoveInput_t GatherMoveInput(const Character_t *c, const Camera_t *camera)
{
	MoveInput_t input;

	input.heading=NormalizeOr(ProjectOnPlane(camera->heading, c->up), c->facing);

	const vec3 side=Vec3_Cross(c->up, input.heading);

	const float forwardInput=(camera->moveForward?1.0f:0.0f)-(camera->moveBackward?1.0f:0.0f);
	const float sideInput=(camera->moveLeft?1.0f:0.0f)-(camera->moveRight?1.0f:0.0f);

	input.wish=Vec3_Addv(Vec3_Muls(input.heading, forwardInput), Vec3_Muls(side, sideInput));
	input.wishLength=Vec3_Length(input.wish);

	// Diagonals shouldn't be faster (unless you want classic DOOM style strafe running)
	if(input.wishLength>1.0f)
	{
		input.wish=Vec3_Muls(input.wish, 1.0f/input.wishLength);
		input.wishLength=1.0f;
	}

	input.hasInput=input.wishLength>INPUT_EPSILON;
	input.maxSpeed=camera->shift?c->runSpeed:c->walkSpeed;

	return input;
}

static float AccelerationRate(const Character_t *c, const bool hasInput)
{
	if(c->grounded)
		return hasInput?c->groundAcceleration:c->groundDeceleration;

	return hasInput?c->airAcceleration:0.0f;
}

// Accelerates the velocity along the ground (or the up plane in the air) toward the wanted velocity, leaving the part
// of the velocity along the plane normal (gravity, jumping, and so on) alone
static void ApplyMovement(Character_t *c, const MoveInput_t *input, const float dt)
{
	RigidBody_t *body=&c->body;

	const vec3 planeNormal=c->grounded?c->groundNormal:c->up;

	// Cancel out gravity while grounded to prevent jittering while standing still
	if(c->grounded)
	{
		const float intoGround=Vec3_Dot(body->velocity, planeNormal);

		if(intoGround<0.0f)
			body->velocity=Vec3_Subv(body->velocity, Vec3_Muls(planeNormal, intoGround));
	}

	// Keep the full input magnitude when the direction is tilted onto a slope
	vec3 wishPlane=ProjectOnPlane(input->wish, planeNormal);
	const float planeLength=Vec3_Length(wishPlane);

	wishPlane=(planeLength>1e-5f)?Vec3_Muls(wishPlane, input->wishLength/planeLength):Vec3b(0.0f);

	const vec3 targetVelocity=Vec3_Muls(wishPlane, input->maxSpeed);

	const vec3 velocityNormal=Vec3_Muls(planeNormal, Vec3_Dot(body->velocity, planeNormal));
	vec3 velocityPlane=Vec3_Subv(body->velocity, velocityNormal);

	velocityPlane=MoveTowards(velocityPlane, targetVelocity, AccelerationRate(c, input->hasInput)*dt);

	body->velocity=Vec3_Addv(velocityPlane, velocityNormal);

	c->speed=Vec3_Length(velocityPlane);
}

// First person always looks where the camera does, third person eases the body around to face where it's moving
static void UpdateFacing(Character_t *c, const Camera_t *camera, const MoveInput_t *input, const float dt)
{
	if(camera->orbitDistance<=0.001f)
	{
		c->facing=input->heading;
		c->turnVelocity=0.0f;
		return;
	}

	const vec3 desiredFacing=input->hasInput?NormalizeOr(ProjectOnPlane(input->wish, c->up), c->facing):c->facing;
	const float error=SignedAngle(c->facing, desiredFacing, c->up);
	const float turn=SmoothDamp(error, &c->turnVelocity, c->turnSmoothTime, c->turnRate, dt);

	c->facing=NormalizeOr(QuatRotate(QuatAnglev(turn, c->up), c->facing), c->facing);
}

static void SyncBodyOrientation(Character_t *c)
{
	c->body.orientation=OrientationFromUpFacing(c->up, c->facing);
	c->body.angularVelocity=Vec3b(0.0f);
}

// Picks the animation state and runs the landing timer
static void UpdateState(Character_t *c, const float dt)
{
	const RigidBody_t *body=&c->body;

	if(c->speed>=c->walkSpeed*LAND_CANCEL_SPEED_RATIO)
		c->landTimer=0.0f;

	c->landTimer=fmaxf(c->landTimer-dt, 0.0f);

	c->downSpeed=-Vec3_Dot(body->velocity, c->up);

	if(!c->grounded&&c->timeSinceGrounded>=GROUND_STATE_GRACE_TIME)
	{
		c->state=(Vec3_Dot(body->velocity, c->up)>0.0f)?CHARACTER_STATE_JUMP:CHARACTER_STATE_FALL;
		return;
	}

	if(c->landTimer>0.0f)
		c->state=CHARACTER_STATE_LAND;
	else if(c->speed<c->walkSpeed*IDLE_SPEED_RATIO)
		c->state=CHARACTER_STATE_IDLE;
	else if(c->speed<=c->walkSpeed*RUN_SPEED_RATIO)
		c->state=CHARACTER_STATE_WALK;
	else
		c->state=CHARACTER_STATE_RUN;
}

void Character_Update(Character_t *c, const Camera_t *camera, const float dt)
{
	if(!c->active||dt<=0.0f)
		return;

	const bool hasGravity=Vec3_Length(c->gravityAccum)>GRAVITY_EPSILON;

	UpdateUp(c, hasGravity, dt);

	// Landing needs to know what the ground state was before this update changes it
	const bool wasGrounded=c->grounded;
	const float airTime=c->timeSinceGrounded;

	UpdateGroundState(c, dt);
	UpdateLanding(c, wasGrounded, airTime);

	if(hasGravity)
		c->body.velocity=Vec3_Addv(c->body.velocity, Vec3_Muls(c->gravityAccum, dt));

	UpdateJump(c, camera, hasGravity);

	const MoveInput_t input=GatherMoveInput(c, camera);

	ApplyMovement(c, &input, dt);
	UpdateFacing(c, camera, &input, dt);
	SyncBodyOrientation(c);
	UpdateState(c, dt);
}
