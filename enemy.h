#ifndef __ENEMY_H__
#define __ENEMY_H__

#include "camera/camera.h"
#include "entitylist.h"

#define NUM_ENEMY 8

#define ENEMY_SIGHT_DISTANCE 1000.0f
#define ENEMY_SIGHT_ANGLE 0.5f

#define ENEMY_ALERT_FILL_RATE 0.1f
#define ENEMY_ALERT_DRAIN_RATE 0.4f
#define ENEMY_ALERT_PURSUING 0.80f
#define ENEMY_ALERT_SUSPICIOUS 0.25f
#define ENEMY_ALERT_LOST 0.05f

#define ENEMY_TRACK_FORCE 2.0f
#define ENEMY_ROTATION_FORCE 0.15f
#define ENEMY_STRAFE_FORCE 1.5f

#define ENEMY_ATTACK_ANGLE 0.5f
#define ENEMY_ATTACK_RANGE 100.0f
#define ENEMY_FIRE_COOLDOWN_MIN 0.25f
#define ENEMY_FIRE_COOLDOWN_MAX 1.0f

#define ENEMY_SEARCH_TIMEOUT 8.0f

#define ENEMY_PATROL_RADIUS_MIN 50.0f
#define ENEMY_PATROL_RADIUS_MAX 150.0f
#define ENEMY_PATROL_ARRIVE_DIST 15.0f
#define ENEMY_PATROL_WAIT_MIN 1.0f
#define ENEMY_PATROL_WAIT_MAX 3.0f

#define ENEMY_RETREAT_HEALTH 25.0f
#define ENEMY_RETREAT_FORCE 2.0f
#define ENEMY_RETREAT_SAFE_DIST 200.0f

#define ENEMY_STUN_DURATION 0.8f

#define ENEMY_AVOIDANCE_FORCE 1.5f

typedef enum
{
	BT_FAILURE=0,
	BT_SUCCESS
} BTStatus_t;

typedef enum
{
	BT_NODE_SEQUENCE=0,
	BT_NODE_SELECTOR,
	BT_NODE_LEAF
} BTNodeType_t;

#define BT_MAX_CHILDREN 8

typedef struct Enemy_s Enemy_t;

typedef BTStatus_t (*BTLeafFunc)(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player);

typedef struct BTNode_s
{
	BTNodeType_t type;
	BTLeafFunc fn;
	struct BTNode_s *children[BT_MAX_CHILDREN];
	uint32_t childCount;
} BTNode_t;

typedef enum
{
	ENEMY_STATE_PATROLLING=0,
	ENEMY_STATE_SUSPICIOUS,
	ENEMY_STATE_SEARCHING,
	ENEMY_STATE_PURSUING,
	ENEMY_STATE_ATTACKING,
	ENEMY_STATE_RETREATING,
	ENEMY_STATE_STUNNED,
	ENEMY_STATE_DEAD,
} EnemyState_t;

typedef struct Enemy_s
{
	float health;
	EnemyState_t state;
	Camera_t *camera;
	Camera_t lastKnownPlayer;

	float alertLevel;

	float searchTimer;
	float stunTimer;
	float fireCooldownTimer;
	float fireCooldown;

	float strafeTimer;
	float strafeDuration;
	float strafeAngle;

	vec3 patrolOrigin;
	vec3 patrolTarget;
	float patrolWaitTimer;

	BTNode_t *btRoot;
} Enemy_t;

void InitEnemy(Enemy_t *enemy, Camera_t *enemyCamera, const Camera_t playerCamera);
void UpdateEnemy(Enemy_t *enemy, const EntityList_t *entityList, Camera_t player);

void DamageEnemy(Enemy_t *enemy, float damage);

#endif
