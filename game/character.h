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
	CHARACTER_STATE_COUNT
} CharacterState_e;

typedef struct
{
	bool active;

	RigidBody_t body;

	float walkSpeed, runSpeed;
	float groundAcceleration, groundDeceleration, airAcceleration;
	float jumpSpeed;
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
	float timeSinceGrounded;
	float groundIgnoreTime;
	float landTimer;		// Time left in the landing state
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

// Update everything
void Character_Update(Character_t *character, const Camera_t *camera, const float dt);

#endif
