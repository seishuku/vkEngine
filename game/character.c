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
#define EYE_HEIGHT_RATIO			0.89f	// Eye height as a fraction of body height
#define HEADROOM_RADIUS_RATIO		0.9f	// The headroom volume is a bit thinner than the body, so touching a wall beside it doesn't block standing

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

	c->walkSpeed=1.5f*WORLD_SCALE;
	c->runSpeed=6.0f*WORLD_SCALE;

	c->groundAcceleration=30.0f*WORLD_SCALE;
	c->groundDeceleration=40.0f*WORLD_SCALE;

	c->airAcceleration=20.0f*WORLD_SCALE;
	c->airDeceleration=6.0f*WORLD_SCALE;

	c->riseGravityScale=2.0f;
	c->fallGravityScale=3.0f;
	c->cutGravityScale=5.0f;
	c->maxFallSpeed=25.0f*WORLD_SCALE;

	c->jumpHeight=2.0f*WORLD_SCALE;
	c->jumpBufferTime=0.12f;
	c->jumpSpeed=sqrtf(2.0f*9.81f*WORLD_SCALE*c->riseGravityScale*c->jumpHeight);	// Reworked from the real gravity at takeoff

	c->maxSlopeCos=cosf(65.0f*(PI/180.0f));

	c->friction=1.0f;

	c->turnWithGround=true;

	c->coyoteTime=0.1f;

	c->upSmoothing=8.0f;
	c->turnRate=10.0f;
	c->turnSmoothTime=0.15f;

	c->landTime=0.3f;
	c->minLandSpeed=2.0f*WORLD_SCALE;
	c->minLandAirTime=0.15f;

	c->crouchHeightRatio=0.6f;
	c->crouchSpeedRatio=0.6f;
	c->crouchTransitionTime=0.12f;
	c->crouchToggle=false;

	c->crouchHalfHeight=fmaxf(height*c->crouchHeightRatio*0.5f-radius, 0.0f);
	c->crouchEyeHeight=height*c->crouchHeightRatio*EYE_HEIGHT_RATIO;

	c->standHalfHeight=halfHeight;
	c->standEyeHeight=height*EYE_HEIGHT_RATIO;

	c->eyeHeight=c->standEyeHeight;

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
	c->numContacts=0;
}

void Character_AddGravity(Character_t *c, const vec3 gravity)
{
	c->gravityAccum=Vec3_Addv(c->gravityAccum, gravity);
}

void Character_AddContact(Character_t *c, RigidBody_t *a, RigidBody_t *b, const ContactPoint_t *contact)
{
	// Which body belongs to the controller
	const bool isA=(a==&c->body);

	if(!c->active||(!isA&&b!=&c->body))
		return;

	RigidBody_t *other=isA?b:a;
	const vec3 normal=Vec3_Muls(contact->normal, isA?-1.0f:1.0f);

	// Too steep to stand on
	if(Vec3_Dot(normal, c->up)<c->maxSlopeCos)
		return;

	c->groundNormalAccum=Vec3_Addv(c->groundNormalAccum, Vec3_Muls(normal, fmaxf(contact->penetration, 0.001f)));

	if(c->numContacts<CHARACTER_MAX_CONTACTS)
		c->contacts[c->numContacts++]=(CharacterContact_t){ other, contact->position, normal, sqrtf(fmaxf(c->friction*other->friction, 0.0f)) };
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
	const bool touching=c->numContacts>0&&c->groundIgnoreTime<=0.0f&&verticalSpeed<c->jumpSpeed*JUMP_GROUNDED_SPEED_RATIO;

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

// Distance from the body's center down to the feet along up
static float FeetOffset(const Character_t *c)
{
	return c->body.radiusHeight.x+c->body.radiusHeight.y;
}

float Character_GetEyeOffset(const Character_t *c)
{
	return c->eyeHeight-FeetOffset(c);
}

// Swaps the capsule between standing and crouched size, keeping the feet where they are
static void SetCrouching(Character_t *c, const bool crouching)
{
	const float oldOffset=FeetOffset(c);

	c->body.radiusHeight.y=crouching?c->crouchHalfHeight:c->standHalfHeight;
	c->body.position=Vec3_Addv(c->body.position, Vec3_Muls(c->up, FeetOffset(c)-oldOffset));
	c->crouching=crouching;
}

// Checks the extra space standing would take up, from the top of the crouched capsule to the top of the standing one
static bool HasHeadroom(const Character_t *c)
{
	if(!c->headroomFunc)
		return true;

	const float radius=c->body.radiusHeight.x;
	const vec3 feet=Vec3_Subv(c->body.position, Vec3_Muls(c->up, FeetOffset(c)));

	RigidBody_t volume;

	memset(&volume, 0, sizeof(RigidBody_t));

	volume.position=Vec3_Addv(feet, Vec3_Muls(c->up, c->crouchHalfHeight+c->standHalfHeight+radius));
	volume.orientation=c->body.orientation;
	volume.type=RIGIDBODY_CAPSULE;
	volume.radiusHeight=Vec2(radius*HEADROOM_RADIUS_RATIO, fmaxf(c->standHalfHeight-c->crouchHalfHeight, 0.0f));

	return c->headroomFunc(&volume, c->headroomUserData);
}

static void UpdateCrouch(Character_t *c, const Camera_t *camera, const float dt)
{
	const bool moveDown=camera->moveAxis.y<0.0f;
	if(c->crouchToggle)
	{
		if(moveDown&&!c->crouchHeld)
			c->crouchWanted=!c->crouchWanted;
	}
	else
		c->crouchWanted=moveDown;

	c->crouchHeld=moveDown;

	// Only change size on the ground, and standing back up also needs room above
	if(c->grounded)
	{
		if(c->crouchWanted&&!c->crouching)
			SetCrouching(c, true);
		else if(!c->crouchWanted&&c->crouching&&HasHeadroom(c))
			SetCrouching(c, false);
	}

	// The capsule changes size right away, the eye eases to its new height
	const float targetEye=c->crouching?c->crouchEyeHeight:c->standEyeHeight;

	c->eyeHeight+=SmoothDamp(targetEye-c->eyeHeight, &c->eyeVelocity, c->crouchTransitionTime, 100.0f*WORLD_SCALE, dt);
}

static void UpdateJump(Character_t *c, const Camera_t *camera, const bool hasGravity, const float dt)
{
	const bool moveUp=camera->moveAxis.y>0.0f;
	const bool jumpPressed=moveUp&&!c->jumpHeld;

	c->jumpHeld=moveUp;

	// Remember a press for a moment, so pressing jump just before touching down still jumps on landing
	c->jumpBufferTimer=jumpPressed?c->jumpBufferTime:fmaxf(c->jumpBufferTimer-dt, 0.0f);

	if(c->jumpBufferTimer<=0.0f||!hasGravity)
		return;

	// Allow a short grace period after walking off a ledge ("coyote time")
	if(!c->grounded&&c->timeSinceGrounded>=c->coyoteTime)
		return;

	// Jumping out of a crouch stands up first, which needs room
	if(c->crouching)
	{
		if(!HasHeadroom(c))
			return;

		SetCrouching(c, false);
		c->crouchWanted=false;
	}

	// Takeoff speed that reaches jumpHeight against the rising gravity, so the height is the same on any gravity
	c->jumpSpeed=sqrtf(2.0f*Vec3_Length(c->gravityAccum)*c->riseGravityScale*c->jumpHeight);

	c->body.velocity=Vec3_Addv(ProjectOnPlane(c->body.velocity, c->up), Vec3_Muls(c->up, c->jumpSpeed));

	c->jumping=true;
	c->jumpBufferTimer=0.0f;
	c->grounded=false;
	c->groundNormal=c->up;
	c->timeSinceGrounded=c->coyoteTime;
	c->groundIgnoreTime=JUMP_GROUND_IGNORE_TIME;
}

static void ApplyGravity(Character_t *c, const Camera_t *camera, const float dt)
{
	const float verticalSpeed=Vec3_Dot(c->body.velocity, c->up);
	const bool moveUp=camera->moveAxis.y>0.0f;

	if(c->grounded||verticalSpeed<=0.0f)
		c->jumping=false;

	float scale=1.0f;

	if(!c->grounded)
	{
		if(verticalSpeed>0.0f)
			scale=(c->jumping&&!moveUp)?c->cutGravityScale:c->riseGravityScale;
		else
			scale=c->fallGravityScale;
	}

	c->body.velocity=Vec3_Addv(c->body.velocity, Vec3_Muls(c->gravityAccum, scale*dt));

	const float fallSpeed=-Vec3_Dot(c->body.velocity, c->up);

	if(fallSpeed>c->maxFallSpeed)
		c->body.velocity=Vec3_Addv(c->body.velocity, Vec3_Muls(c->up, fallSpeed-c->maxFallSpeed));
}

static MoveInput_t GatherMoveInput(const Character_t *c, const Camera_t *camera)
{
	MoveInput_t input;

	input.heading=NormalizeOr(ProjectOnPlane(camera->heading, c->up), c->facing);

	const vec3 side=Vec3_Cross(c->up, input.heading);

	const float forwardInput=camera->moveAxis.z;
	const float sideInput=camera->moveAxis.x;

	input.wish=Vec3_Addv(Vec3_Muls(input.heading, forwardInput), Vec3_Muls(side, sideInput));
	input.wishLength=Vec3_Length(input.wish);

	// Diagonals shouldn't be faster (unless you want classic DOOM style strafe running)
	if(input.wishLength>1.0f)
	{
		input.wish=Vec3_Muls(input.wish, 1.0f/input.wishLength);
		input.wishLength=1.0f;
	}

	input.hasInput=input.wishLength>INPUT_EPSILON;
	input.maxSpeed=c->crouching?c->walkSpeed*c->crouchSpeedRatio:(camera->shift?c->runSpeed:c->walkSpeed);

	return input;
}

static float AccelerationRate(const Character_t *c, const bool hasInput)
{
	if(c->grounded)
		return hasInput?c->groundAcceleration:c->groundDeceleration;

	return hasInput?c->airAcceleration:c->airDeceleration;
}

static vec3 PointVelocity(const RigidBody_t *body, const vec3 point)
{
	const vec3 w=QuatRotate(body->orientation, body->angularVelocity);

	return Vec3_Addv(body->velocity, Vec3_Cross(w, Vec3_Subv(point, body->position)));
}

static float ContactInvMass(const RigidBody_t *a, const RigidBody_t *b, const vec3 point, const vec3 direction)
{
	const vec3 r1=Vec3_Subv(point, a->position);
	const vec3 r2=Vec3_Subv(point, b->position);

	const vec3 d1=Vec3_Cross(Vec3_Muls(Vec3_Cross(r1, direction), a->invInertia), r1);
	const vec3 d2=Vec3_Cross(Vec3_Muls(Vec3_Cross(r2, direction), b->invInertia), r2);

	return a->invMass+b->invMass+Vec3_Dot(direction, Vec3_Addv(d1, d2));
}

static vec3 ApplySupport(Character_t *c, float *load)
{
	RigidBody_t *body=&c->body;
	vec3 groundVelocity=Vec3b(0.0f);

	*load=0.0f;

	for(uint32_t i=0;i<c->numContacts;i++)
	{
		const CharacterContact_t *contact=&c->contacts[i];
		const float approach=Vec3_Dot(Vec3_Subv(body->velocity, PointVelocity(contact->other, contact->point)), contact->normal);
		const float impulse=-approach/ContactInvMass(contact->other, body, contact->point, contact->normal);

		if(approach<0.0f)
		{
			PhysicsApplyImpulse(body, Vec3_Muls(contact->normal, impulse), contact->point);
			PhysicsApplyImpulse(contact->other, Vec3_Muls(contact->normal, -impulse), contact->point);
			*load+=impulse;
		}

		// After the impulse, which moves a surface that can be pushed
		groundVelocity=Vec3_Addv(groundVelocity, PointVelocity(contact->other, contact->point));
	}

	return Vec3_Muls(groundVelocity, 1.0f/(float)c->numContacts);
}

// Coulomb friction
static vec3 ApplyFriction(Character_t *c, vec3 slide, const float load)
{
	for(uint32_t i=0;i<c->numContacts;i++)
	{
		const CharacterContact_t *contact=&c->contacts[i];
		const float speed=Vec3_Length(slide);

		if(speed<FLT_EPSILON)
			break;

		const vec3 tangent=Vec3_Muls(slide, 1.0f/speed);
		const float impulse=fminf(speed/ContactInvMass(contact->other, &c->body, contact->point, tangent), contact->friction*load/(float)c->numContacts);

		slide=Vec3_Subv(slide, Vec3_Muls(tangent, impulse*c->body.invMass));
		PhysicsApplyImpulse(contact->other, Vec3_Muls(tangent, impulse), contact->point);
	}

	return slide;
}

// Accelerates the velocity along the ground (or the up plane in the air) toward the wanted velocity,
// leaving the part of the velocity along the plane normal (gravity, jumping, and so on) alone.
static void ApplyMovement(Character_t *c, const MoveInput_t *input, const float dt)
{
	RigidBody_t *body=&c->body;

	const vec3 planeNormal=c->grounded?c->groundNormal:c->up;

	vec3 ground=c->groundVelocity;
	float load=0.0f, traction=1.0f;

	if(c->grounded)
	{
		ground=ApplySupport(c, &load);

		if(c->riding)
			body->position=Vec3_Addv(body->position, Vec3_Muls(Vec3_Subv(ground, c->groundVelocity), dt));

		float friction=0.0f;

		for(uint32_t i=0;i<c->numContacts;i++)
			friction+=c->contacts[i].friction/(float)c->numContacts;

		body->velocity=Vec3_Subv(body->velocity, Vec3_Muls(planeNormal, fminf(Vec3_Dot(Vec3_Subv(body->velocity, ground), planeNormal), 0.0f)));
	}

	// Everything from here is relative to the ground
	const vec3 relativeVelocity=Vec3_Subv(body->velocity, ground);
	const vec3 velocityNormal=Vec3_Muls(planeNormal, Vec3_Dot(relativeVelocity, planeNormal));
	vec3 velocityPlane=Vec3_Subv(relativeVelocity, velocityNormal);

	// Keep the full input magnitude when the direction is tilted onto a slope
	vec3 wishPlane=ProjectOnPlane(input->wish, planeNormal);
	const float planeLength=Vec3_Length(wishPlane);

	wishPlane=(planeLength>FLT_EPSILON)?Vec3_Muls(wishPlane, input->wishLength/planeLength):Vec3b(0.0f);

	float maxSpeed=input->maxSpeed;

	if(!c->grounded&&input->hasInput)
		maxSpeed=fmaxf(maxSpeed, Vec3_Length(velocityPlane));

	vec3 slide=Vec3b(0.0f);
	const vec3 slopeGravity=c->grounded?ProjectOnPlane(c->gravityAccum, planeNormal):Vec3b(0.0f);

	if(c->grounded)
	{
		velocityPlane=Vec3_Subv(velocityPlane, Vec3_Muls(slopeGravity, dt));

		const vec3 slidePlane=ProjectOnPlane(c->slideVelocity, planeNormal);
		const float length=Vec3_Length(slidePlane);
		const float along=(length>FLT_EPSILON)?Vec3_Dot(velocityPlane, slidePlane)/length:0.0f;

		slide=(along>0.0f)?Vec3_Muls(slidePlane, fminf(length, along)/length):Vec3b(0.0f);
		velocityPlane=Vec3_Subv(velocityPlane, slide);
	}

	const vec3 driveStart=velocityPlane;

	velocityPlane=MoveTowards(velocityPlane, Vec3_Muls(wishPlane, maxSpeed), AccelerationRate(c, input->hasInput)*traction*dt);

	if(c->grounded)
	{
		const vec3 reaction=Vec3_Muls(Vec3_Subv(velocityPlane, driveStart), -body->mass/(float)c->numContacts);

		for(uint32_t i=0;i<c->numContacts;i++)
			PhysicsApplyImpulse(c->contacts[i].other, reaction, c->contacts[i].point);

		slide=ApplyFriction(c, Vec3_Addv(slide, Vec3_Muls(slopeGravity, dt)), load);

		if(Vec3_Length(slide)>c->maxFallSpeed)
			slide=Vec3_Muls(slide, c->maxFallSpeed/Vec3_Length(slide));
	}

	c->groundVelocity=ground;
	c->riding=c->grounded;

	c->slideVelocity=slide;
	c->slideSpeed=Vec3_Length(slide);

	body->velocity=Vec3_Addv(Vec3_Addv(ground, Vec3_Addv(velocityPlane, slide)), velocityNormal);

	c->speed=Vec3_Length(velocityPlane);
}

// Ground spinning turns the character only about it's up vector, a tumbling box doesn't swing the view around.
static void ApplyGroundTurn(Character_t *c, Camera_t *camera, const float dt)
{
	if(!c->grounded||c->numContacts==0||!c->turnWithGround)
		return;

	float spin=0.0f;

	for(uint32_t i=0;i<c->numContacts;i++)
		spin+=Vec3_Dot(QuatRotate(c->contacts[i].other->orientation, c->contacts[i].other->angularVelocity), c->up);

	const vec4 turn=QuatAnglev(spin/(float)c->numContacts*dt, c->up);

	camera->heading=QuatRotate(turn, camera->heading);
	c->facing=QuatRotate(turn, c->facing);
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

	if(c->crouching)
		c->state=(c->speed<c->walkSpeed*IDLE_SPEED_RATIO)?CHARACTER_STATE_CROUCH_IDLE:CHARACTER_STATE_CROUCH_WALK;
	else if(c->landTimer>0.0f)
		c->state=CHARACTER_STATE_LAND;
	else if(c->speed<c->walkSpeed*IDLE_SPEED_RATIO)
		c->state=CHARACTER_STATE_IDLE;
	else if(c->speed<=c->walkSpeed*RUN_SPEED_RATIO)
		c->state=CHARACTER_STATE_WALK;
	else
		c->state=CHARACTER_STATE_RUN;
}

void Character_Update(Character_t *c, Camera_t *camera, const float dt)
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
	UpdateCrouch(c, camera, dt);

	if(hasGravity)
		ApplyGravity(c, camera, dt);

	UpdateJump(c, camera, hasGravity, dt);

	ApplyGroundTurn(c, camera, dt);

	const MoveInput_t input=GatherMoveInput(c, camera);

	ApplyMovement(c, &input, dt);
	UpdateFacing(c, camera, &input, dt);
	SyncBodyOrientation(c);
	UpdateState(c, dt);
}
