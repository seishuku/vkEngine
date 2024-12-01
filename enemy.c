#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "math/math.h"
#include "physics/physics.h"
#include "physics/physicslist.h"
#include "camera/camera.h"
#include "audio/audio.h"
#include "audio/sounds.h"
#include "enemy.h"

#define NUM_ASTEROIDS 1000
extern RigidBody_t asteroids[NUM_ASTEROIDS];

extern float fTimeStep;

void FireParticleEmitter(vec3 position, vec3 direction);

// angles here are in cosine space (-1.0 to 1.0)
static const float MAX_SIGHT_DISTANCE=1000.0f;
static const float MAX_SIGHT_ANGLE=0.5f;
static const float TRACK_FORCE=2.0f;
static const float TRACK_RANGE=80.0f;
static const float ROTATION_FORCE=0.2f;
static const float ATTACK_ANGLE_THRESHOLD=0.5f;
static const float ATTACK_RANGE=100.0f;

void InitEnemy(Enemy_t *enemy, Camera_t *enemyCamera, const Camera_t playerCamera)
{
	// Set initial state
	enemy->state=SEARCHING;

	// Assign camera pointer to "connect" it to a camera
	enemy->camera=enemyCamera;

	// Assign last known player camera
	enemy->lastKnownPlayer=playerCamera;
}

static bool HasLineOfSight(Enemy_t *enemy, const Camera_t player)
{
	// Calculate the direction from the camera to the target
	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	const float distance=Vec3_Normalize(&direction);

	if(distance>MAX_SIGHT_DISTANCE)
		return false;

	const float theta=Vec3_Dot(enemy->camera->forward, direction);

	// Calculate the angle between the camera's forward vector and the direction to the target
	// Check if the angle is within half of the FOV angle
	return theta>MAX_SIGHT_ANGLE;
}

static float TrackPlayer(Enemy_t *enemy, const Camera_t player)
{
	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	float distance=Vec3_Normalize(&direction);
	vec3 avoidanceDirection=Vec3b(0.0f);

	// Check for obstacles in the avoidance radius
	for(size_t i=0;i<numPhysicsObjects;i++)
	{
		const float avoidanceRadius=(enemy->camera->body.radius+physicsObjects[i].rigidBody->radius)*1.5f;
		const vec3 cameraToObstacle=Vec3_Subv(enemy->camera->body.position, physicsObjects[i].rigidBody->position);
		const float cameraToObstacleDistanceSq=Vec3_Dot(cameraToObstacle, cameraToObstacle);

		if(cameraToObstacleDistanceSq<avoidanceRadius*avoidanceRadius)
		{
			if(cameraToObstacleDistanceSq>0.0f)
			{
				// Adjust the camera trajectory to avoid the obstacle
				const float rMag=1.0f/sqrtf(cameraToObstacleDistanceSq);
				avoidanceDirection=Vec3_Muls(cameraToObstacle, rMag);
			}
		}
	}

	if(distance>TRACK_RANGE)
	{
		vec3 trackDistance=Vec3_Addv(direction, avoidanceDirection);
		Vec3_Normalize(&trackDistance);

		enemy->camera->body.force=Vec3_Addv(enemy->camera->body.force, Vec3_Muls(trackDistance, TRACK_FORCE));
	}

	vec3 rotationAxis=QuatRotate(QuatInverse(enemy->camera->body.orientation), Vec3_Cross(enemy->camera->forward, direction));
	float cosTheta=clampf(Vec3_Dot(enemy->camera->forward, direction), -1.0f, 1.0f);
	float theta=acosf(cosTheta);
	vec4 rotation=QuatAnglev(theta, rotationAxis);

	enemy->camera->body.angularVelocity=Vec3_Addv(enemy->camera->body.angularVelocity, Vec3_Muls(Vec3(rotation.x, rotation.y, rotation.z), ROTATION_FORCE));

	return distance;
}

static void AttackPlayer(Enemy_t *enemy, const Camera_t player)
{
	static float fireCooldownTimer=0.0f;
	static float fireCooldown=0.0f;

	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	float distance=Vec3_Normalize(&direction);
	vec3 forward=enemy->camera->forward;
	Vec3_Normalize(&forward);
	float theta=Vec3_Dot(direction, forward);

	if(theta>ATTACK_ANGLE_THRESHOLD&&distance<ATTACK_RANGE)
	{
		// Fire weapon
		fireCooldownTimer+=fTimeStep;

		if(fireCooldownTimer>fireCooldown)
		{
			fireCooldownTimer-=fireCooldown;
			fireCooldown=RandFloatRange(0.25f, 2.0f);

			FireParticleEmitter(Vec3_Addv(enemy->camera->body.position, Vec3_Muls(enemy->camera->forward, enemy->camera->body.radius)), enemy->camera->forward);
		}
	}
}

static bool IsInAttackRange(Enemy_t *enemy, const Camera_t player)
{
	const float distance=Vec3_Distance(enemy->camera->body.position, player.body.position);

	return distance<ATTACK_RANGE;
}

bool IsAlignedForAttack(Enemy_t *enemy, const Camera_t player)
{
	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	Vec3_Normalize(&direction);

	return Vec3_Dot(enemy->camera->forward, direction)>ATTACK_ANGLE_THRESHOLD;
}

void UpdateEnemy(Enemy_t *enemy, Camera_t player)
{
	switch(enemy->state)
	{
		case PURSUING:
		{
			const float distance=TrackPlayer(enemy, player);

			DBGPRINTF(DEBUG_WARNING, "%f\r", distance);

			// Switch to attacking if within range and aligned
			if(IsInAttackRange(enemy, player)&&IsAlignedForAttack(enemy, player))
				enemy->state=ATTACKING;
			else if(!HasLineOfSight(enemy, player))
			{
				enemy->state=SEARCHING;
				enemy->lastKnownPlayer=player;
			}
			break;
		}

		case SEARCHING:
		{
			const float distance=TrackPlayer(enemy, enemy->lastKnownPlayer);

			DBGPRINTF(DEBUG_WARNING, "%f\r", distance);

			if(HasLineOfSight(enemy, player))
				enemy->state=PURSUING;
			break;
		}

		case ATTACKING:
		{
			TrackPlayer(enemy, player);
			AttackPlayer(enemy, player);

			// Stay in attack mode while aligned and within range
			if(!IsInAttackRange(enemy, player)||!IsAlignedForAttack(enemy, player))
			{
				enemy->state=PURSUING;
			}
			else if(!HasLineOfSight(enemy, player))
			{
				enemy->state=SEARCHING;
				enemy->lastKnownPlayer=player;
			}
			break;
		}
	}
}
