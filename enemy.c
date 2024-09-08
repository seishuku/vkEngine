#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "math/math.h"
#include "physics/physics.h"
#include "camera/camera.h"
#include "enemy.h"

#define NUM_ASTEROIDS 1000
extern RigidBody_t asteroids[NUM_ASTEROIDS];

void InitEnemy(Enemy_t *enemy, Camera_t *enemyCamera, const Camera_t playerCamera)
{
	// Set initial state
	enemy->state=SEARCHING;

	// Assign pointer to enemy camera
	enemy->camera=enemyCamera;

	// Assign last known player camera
	enemy->lastKnownPlayer=playerCamera;
}

static bool HasLineOfSight(Enemy_t *enemy, const Camera_t player)
{
	const float MAX_SIGHT_DISTANCE=1000.0f;
	const float LOS_ANGLE_THRESHOLD=0.5f;

	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	float distance=Vec3_Normalize(&direction);

	//for(uint32_t i=0;i<numObstacles;i++)
	//{
	//	RigidBody_t *obstacle=&obstacles[i];

	//	if(raySphereIntersect(enemy->camera->body.position, direction, obstacle->position, obstacle->radius))
	//		return false;  // Obstacle blocks line of sight
	//}

	if(distance>MAX_SIGHT_DISTANCE) return false;

	float angleToPlayer=Vec3_Dot(enemy->camera->forward, direction);

	return angleToPlayer>cosf(LOS_ANGLE_THRESHOLD);  // Enemy can "see" if within LOS angle
}

static void TrackPlayer(Enemy_t *enemy, const Camera_t player)
{
	const float TRACK_FORCE=1.0f;
	const float ROTATION_FORCE=0.1f;

	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	Vec3_Normalize(&direction);

	enemy->camera->body.force=Vec3_Addv(enemy->camera->body.force, Vec3_Muls(direction, TRACK_FORCE));

	vec3 rotationAxis=QuatRotate(QuatInverse(enemy->camera->body.orientation), Vec3_Cross(enemy->camera->forward, direction));
	float cosTheta=clampf(Vec3_Dot(enemy->camera->forward, direction), -1.0f, 1.0f);
	float theta=acosf(cosTheta);
	vec4 rotation=QuatAnglev(theta, rotationAxis);

	enemy->camera->body.angularVelocity=Vec3_Addv(enemy->camera->body.angularVelocity, Vec3_Muls(Vec3(rotation.x, rotation.y, rotation.z), ROTATION_FORCE));
}

static void AttackPlayer(Enemy_t *enemy, const Camera_t player)
{
	const float ATTACK_THRESHOLD=0.5f;
	const float ATTACK_RANGE=100.0f;

	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	float distance=Vec3_Normalize(&direction);
	vec3 forward=enemy->camera->forward;
	Vec3_Normalize(&forward);
	float theta=Vec3_Dot(direction, forward);

	if(theta>ATTACK_THRESHOLD&&distance<ATTACK_RANGE)
	{
		// Fire weapon
		DBGPRINTF(DEBUG_WARNING, "PEW.\n");
	}
}

static bool IsInAttackRange(Enemy_t *enemy, const Camera_t player)
{
	const float ATTACK_RANGE=1000.0f;
	const float distance=Vec3_Distance(enemy->camera->body.position, player.body.position);

	return distance<ATTACK_RANGE;
}

bool IsAlignedForAttack(Enemy_t *enemy, const Camera_t player)
{
	const float ATTACK_ALIGNMENT_THRESHOLD=0.5f;

	vec3 direction=Vec3_Subv(player.body.position, enemy->camera->body.position);
	Vec3_Normalize(&direction);

	return Vec3_Dot(enemy->camera->forward, direction)>ATTACK_ALIGNMENT_THRESHOLD;
}

void UpdateEnemy(Enemy_t *enemy, Camera_t player)
{
	switch(enemy->state)
	{
		case PURSUING:
			TrackPlayer(enemy, player);

			// Switch to attacking if within range and aligned
			if(IsInAttackRange(enemy, player)&&IsAlignedForAttack(enemy, player))
				enemy->state=ATTACKING;
			else if(!HasLineOfSight(enemy, player))
			{
				enemy->state=SEARCHING;
				enemy->lastKnownPlayer=player;
			}
			break;

		case SEARCHING:
			TrackPlayer(enemy, enemy->lastKnownPlayer);

			if(HasLineOfSight(enemy, player))
				enemy->state=PURSUING;
			break;

		case ATTACKING:
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
