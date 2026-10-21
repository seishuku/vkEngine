#ifndef __CHARACTER_H__
#define __CHARACTER_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../physics/physics.h"
#include "../camera/camera.h"

typedef enum
{
	CHARACTER_STATE_IDLE=0,
	CHARACTER_STATE_WALK,
	CHARACTER_STATE_RUN,
	CHARACTER_STATE_JUMP,
	CHARACTER_STATE_FALL,
	CHARACTER_STATE_LAND,
	CHARACTER_STATE_CROUCH_IDLE,
	CHARACTER_STATE_CROUCH_WALK,
	CHARACTER_STATE_COUNT
} CharacterState_e;

// Asks the world whether a volume is clear, used to check there's room to stand up. Returns true if nothing is in it.
// The volume is a capsule and never includes the character itself.
typedef bool (*CharacterHeadroomFunc_t)(const RigidBody_t *volume, void *userData);

typedef struct
{
	bool active;

	RigidBody_t body;

	float walkSpeed, runSpeed;
	float groundAcceleration, groundDeceleration;
	float airAcceleration;	// How fast steering input changes velocity in the air
	float airDeceleration;	// How fast the character slows down in the air with no input, 0 keeps all momentum
	float jumpHeight;		// How high a held jump goes, regardless of how strong gravity is where it happens
	float jumpSpeed;		// Takeoff speed for that height, worked out from the gravity at takeoff
	float riseGravityScale;	// Gravity multiplier while rising with jump held, higher is a snappier jump
	float fallGravityScale;	// Gravity multiplier while falling, higher than rise so the descent is quick and weighty
	float cutGravityScale;	// Gravity multiplier while rising after jump is let go, this is what makes tap jumps short
	float maxFallSpeed;		// Terminal speed toward the ground

	float crouchHeightRatio;	// Crouched height as a fraction of standing height
	float crouchSpeedRatio;		// Crouched walking speed as a fraction of walkSpeed
	float crouchTransitionTime;	// Time for the eye height to ease between standing and crouching (s)
	bool crouchToggle;			// False: hold the crouch key. True: press it to crouch, press again to stand
	float jumpBufferTime;	// Jump pressed this long before touching down still happens on landing
	float maxSlopeCos;		// Cosine of the steepest walkable slope
	float coyoteTime;		// Seconds after leaving the ground that a jump is still allowed
	float upSmoothing;		// How fast "up" follows a change in gravity direction (1/s)
	float turnRate;			// Third person body fastest turn speed (rad/s)
	float turnSmoothTime;	// Third person turning easing time (s)
	float landTime;			// How long the landing state lasts when standing still
	float minLandSpeed;		// Fastest downward speed that doesn't count as a landing (small steps and drops)
	float minLandAirTime;	// Time in the air needed for a touchdown to count

	// Gathered during the physics step by the attractor and collision passes, consumed by the next Update
	vec3 gravityAccum;
	vec3 groundNormalAccum;
	uint32_t groundContacts;

	// State
	vec3 up;
	vec3 facing;
	vec3 groundNormal;
	bool grounded;
	bool jumpHeld;
	bool jumping;			// Rising from a jump, which is when letting go of jump cuts it short
	float jumpBufferTimer;	// Time left on a jump press waiting for the ground
	float timeSinceGrounded;
	float groundIgnoreTime;
	float landTimer;		// Time left in the landing state

	// Crouching only changes size on the ground, in the air the capsule stays as it was
	bool crouching;			// The capsule is currently the crouched size
	bool crouchWanted;		// What the input is asking for, which can differ from crouching when there's no room to stand
	bool crouchHeld;
	float standHalfHeight, crouchHalfHeight;	// Capsule shape for each, the radius stays the same
	float standEyeHeight, crouchEyeHeight;		// Eye heights above the feet
	float eyeHeight;							// Current eye height above the feet, eased between the two
	float eyeVelocity;
	CharacterHeadroomFunc_t headroomFunc;		// Optional, set after Character_Init. Without it standing up is never blocked.
	void *headroomUserData;
	float turnVelocity;		// Current third person turn speed (rad/s)
	float downSpeed;		// Speed toward the ground at the end of the last update

	// Outputs for animation
	float speed;			// Speed along the ground plane
	bool justLanded;		// True for the one update the character touched down, useful for triggering events like sounds
	float landImpact;		// Downward speed of the most recent landing, to scale or change which landing sound
	CharacterState_e state;
} Character_t;

// Height is the full standing height of the character in world units
void Character_Init(Character_t *character, const vec3 position, const vec3 up, const vec3 facing, const float height);

// Call once per physics step before the attractor pass, then feed it gravity and contacts
void Character_ResetGround(Character_t *character);
void Character_AddGravity(Character_t *character, const vec3 gravity);

// Normal vector pointing in from the surface toward the character, contact from collision solver.
void Character_AddContact(Character_t *character, const vec3 normalIntoCharacter, const float penetration);

// How far the eye is above the body's center along up, for the camera's pivot height. Follows the crouch smoothly.
float Character_GetEyeOffset(const Character_t *character);

// Update everything
void Character_Update(Character_t *character, const Camera_t *camera, const float dt);

#endif
