#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "math/math.h"
#include "physics/particle.h"
#include "physics/physics.h"
#include "camera/camera.h"
#include "audio/audio.h"
#include "assetmanager.h"
#include "enemy.h"
#include "entitylist.h"

extern float fTimeStep;
extern ParticleSystem_t particleSystem;

void FireParticleEmitter(vec3 position, vec3 direction);
void ExplodeEmitterCallback(uint32_t index, uint32_t numParticles, Particle_t *particle);

static void ThrustToward(Enemy_t *enemy, const EntityList_t *list, vec3 target, float force)
{
    vec3 avoidance=Vec3b(0.0f);

    for(size_t i=0;i<list->entityCount;i++)
    {
        float radSum=(enemy->camera->body.radius+list->entities[i].body->radius)*1.5f;
        vec3 away=Vec3_Subv(enemy->camera->body.position, list->entities[i].body->position);
        float distSq=Vec3_Dot(away, away);

        if(distSq>0.0f&&distSq<radSum*radSum)
			avoidance=Vec3_Addv(avoidance, Vec3_Muls(away, 1.0f/sqrtf(distSq)));
    }

	vec3 dir=Vec3_Subv(target, enemy->camera->body.position);
	vec3 steer=Vec3_Addv(dir, avoidance);
	Vec3_Normalize(&steer);

	enemy->camera->body.force=Vec3_Addv(enemy->camera->body.force, Vec3_Muls(steer, force));
}

static void ThrustAway(Enemy_t *enemy, vec3 from, float force)
{
	vec3 dir=Vec3_Subv(enemy->camera->body.position, from);
	Vec3_Normalize(&dir);

	enemy->camera->body.force=Vec3_Addv(enemy->camera->body.force, Vec3_Muls(dir, force));
}

static void FaceTarget(Enemy_t *enemy, vec3 target)
{
    vec3 dir=Vec3_Subv(target, enemy->camera->body.position);
	float dist=Vec3_Normalize(&dir);

	if(dist<0.001f)
		return;
	
	vec3 axis=QuatRotate(QuatInverse(enemy->camera->body.orientation), Vec3_Cross(enemy->camera->forward, dir));
	float cosT=clampf(Vec3_Dot(enemy->camera->forward, dir), -1.0f, 1.0f);
	vec4 rot=QuatAnglev(acosf(cosT), axis);

	enemy->camera->body.angularVelocity=Vec3_Addv(enemy->camera->body.angularVelocity, Vec3_Muls(Vec3(rot.x, rot.y, rot.z), ENEMY_ROTATION_FORCE));
}

static void ApplyStrafe(Enemy_t *enemy)
{
	enemy->strafeTimer+=fTimeStep;

	if(enemy->strafeTimer>=enemy->strafeDuration)
	{
		enemy->strafeTimer=0.0f;
		enemy->strafeDuration=RandFloatRange(0.5f, 2.0f);
		enemy->strafeAngle=RandFloatRange(0.0f, 2.0f*PI);
	}

	vec3 strafe=Vec3_Addv(
		Vec3_Muls(enemy->camera->right, cosf(enemy->strafeAngle)),
		Vec3_Muls(enemy->camera->up, sinf(enemy->strafeAngle))
	);
	enemy->camera->body.force=Vec3_Addv(enemy->camera->body.force, Vec3_Muls(strafe, ENEMY_STRAFE_FORCE));
}

static void PatrolTarget(Enemy_t *enemy)
{
	float theta=RandFloatRange(0.0f, 2.0f*PI);
	float phi=acosf(RandFloatRange(-1.0f, 1.0f));
	float r=RandFloatRange(ENEMY_PATROL_RADIUS_MIN, ENEMY_PATROL_RADIUS_MAX);

	enemy->patrolTarget=Vec3_Addv(enemy->patrolOrigin, Vec3(r*sinf(phi)*cosf(theta), r*cosf(phi), r*sinf(phi)*sinf(theta)));
    enemy->patrolWaitTimer=0.0f;
}

static BTStatus_t CondIsDead(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	return (enemy->health<=0.0f||enemy->state==ENEMY_STATE_DEAD)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondIsStunned(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	return (enemy->stunTimer>0.0f)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondIsLowHealth(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	return (enemy->health<ENEMY_RETREAT_HEALTH)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondIsAlerted(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	return (enemy->alertLevel>=ENEMY_ALERT_PURSUING)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondInAttackRange(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	return (Vec3_Distance(enemy->camera->body.position, player->body.position)<ENEMY_ATTACK_RANGE)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondAlignedToAttack(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
    vec3 dir=Vec3_Subv(player->body.position, enemy->camera->body.position);
	return (Vec3_Dot(enemy->camera->forward, dir)>ENEMY_ATTACK_ANGLE)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t CondShouldSearch(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
    return (enemy->alertLevel>ENEMY_ALERT_LOST&&enemy->searchTimer<ENEMY_SEARCH_TIMEOUT)?BT_SUCCESS:BT_FAILURE;
}

static BTStatus_t ActHandleDead(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	if(enemy->state!=ENEMY_STATE_DEAD)
	{
		Audio_PlaySample(&AssetManager_GetAsset(assets, RandRange(SOUND_EXPLODE1, SOUND_EXPLODE3))->sound, false, 1.0f, enemy->camera->body.position);
		ParticleSystem_AddEmitter(&particleSystem,
			enemy->camera->body.position,
			Vec3b(0.0f),
			Vec3(1000.0f, 0.0f, 0.0f),
			Vec3(0.0f, 0.0f, 0.0f),
			20.0f, 1000,
			PARTICLE_EMITTER_ONCE,
			ExplodeEmitterCallback
		);

		enemy->state=ENEMY_STATE_DEAD;
	}

	return BT_SUCCESS;
}

static BTStatus_t ActCoastStunned(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	enemy->stunTimer=fmaxf(0.0f, enemy->stunTimer-fTimeStep);
	enemy->state=ENEMY_STATE_STUNNED;

	return BT_SUCCESS;
}

static BTStatus_t ActRetreat(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	float dist=Vec3_Distance(enemy->camera->body.position, player->body.position);

	if(dist<ENEMY_RETREAT_SAFE_DIST)
	{
		ThrustAway(enemy, player->body.position, ENEMY_RETREAT_FORCE);
		FaceTarget(enemy, player->body.position);
	}

	enemy->state=ENEMY_STATE_RETREATING;

	return BT_SUCCESS;
}

static BTStatus_t ActAttack(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
    vec3 dir=Vec3_Subv(player->body.position, enemy->camera->body.position);
	float dist=Vec3_Normalize(&dir);
	float theta=Vec3_Dot(enemy->camera->forward, dir);

	if(dist<ENEMY_ATTACK_RANGE*0.7f)
		ThrustToward(enemy, list, player->body.position, ENEMY_TRACK_FORCE*0.3f);
	else if(dist>ENEMY_ATTACK_RANGE*0.3f)
		ThrustAway(enemy, player->body.position, ENEMY_TRACK_FORCE*0.3f);
	
	FaceTarget(enemy, player->body.position);
	ApplyStrafe(enemy);

	if(theta>ENEMY_ATTACK_ANGLE&&dist<ENEMY_ATTACK_RANGE)
	{
		enemy->fireCooldownTimer+=fTimeStep;

		if(enemy->fireCooldownTimer>=enemy->fireCooldown)
		{
			enemy->fireCooldownTimer-=enemy->fireCooldown;
			enemy->fireCooldown=RandFloatRange(ENEMY_FIRE_COOLDOWN_MIN, ENEMY_FIRE_COOLDOWN_MAX);

			FireParticleEmitter(Vec3_Addv(enemy->camera->body.position, Vec3_Muls(enemy->camera->forward, enemy->camera->body.radius)), enemy->camera->forward);
        }
    }

    enemy->state=ENEMY_STATE_ATTACKING;

	return BT_SUCCESS;
}

static BTStatus_t ActPursue(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	ThrustToward(enemy, list, player->body.position, ENEMY_TRACK_FORCE);
	FaceTarget(enemy, player->body.position);

	enemy->state=ENEMY_STATE_PURSUING;

	return BT_SUCCESS;
}

static BTStatus_t ActSearch(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	float speedScale=0.4f+0.4f*(enemy->alertLevel/ENEMY_ALERT_PURSUING);

	ThrustToward(enemy, list, enemy->lastKnownPlayer.body.position, ENEMY_TRACK_FORCE*speedScale);
	FaceTarget(enemy, enemy->lastKnownPlayer.body.position);

	enemy->searchTimer+=fTimeStep;
	enemy->state=(enemy->alertLevel>=ENEMY_ALERT_SUSPICIOUS)?ENEMY_STATE_SUSPICIOUS:ENEMY_STATE_SEARCHING;

    return BT_SUCCESS;
}

static BTStatus_t ActPatrol(Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	if(enemy->patrolWaitTimer>0.0f)
	{
		enemy->patrolWaitTimer-=fTimeStep;
		enemy->state=ENEMY_STATE_PATROLLING;

		return BT_SUCCESS;
	}

	float dist=Vec3_Distance(enemy->camera->body.position, enemy->patrolTarget);

	if(dist<ENEMY_PATROL_ARRIVE_DIST)
	{
		enemy->patrolWaitTimer=RandFloatRange(ENEMY_PATROL_WAIT_MIN, ENEMY_PATROL_WAIT_MAX);
		PatrolTarget(enemy);
	}
	else
	{
		ThrustToward(enemy, list, enemy->patrolTarget, ENEMY_TRACK_FORCE*0.5f);
		FaceTarget(enemy, enemy->patrolTarget);
	}

	enemy->state=ENEMY_STATE_PATROLLING;

	return BT_SUCCESS;
}

enum
{
	NODE_ROOT=0,
	NODE_DEAD_SEQUENCE,
	NODE_COND_DEAD,
	NODE_ACT_DEAD,

	NODE_STUN_SEQUENCE,
	NODE_COND_STUN,
	NODE_ACT_STUN,
	NODE_RETREAT_SEQUENCE,
	NODE_COND_LOW_HEALTH,
	NODE_ACT_RETREAT,

	NODE_COMBAT_SEQUENCE,
	NODE_COND_ALERTED,
	NODE_COMBAT_SELECTOR,
	NODE_ATTACK_SEQUENCE,
	NODE_COND_IN_RANGE,
	NODE_COND_ALIGNED,
	NODE_ACT_ATTACK,
	NODE_ACT_PURSUE,

	NODE_SEARCH_SEQUENCE,
	NODE_COND_SEARCH,
	NODE_ACT_SEARCH,

	NODE_ACT_PATROL,

	N_COUNT
};

static BTNode_t behaviorNodes[N_COUNT];
static bool behaviorTreeBuilt=false;

static void MakeLeaf(int id, BTLeafFunc fn)
{
	behaviorNodes[id].type=BT_NODE_LEAF;
	behaviorNodes[id].fn=fn;
	behaviorNodes[id].childCount=0;
}

static void MakeComposition(int id, BTNodeType_t type, int *childIDs, uint32_t count)
{
	behaviorNodes[id].type=type;
	behaviorNodes[id].fn=NULL;
	behaviorNodes[id].childCount=count;

	for(uint32_t i=0;i<count;i++)
		behaviorNodes[id].children[i]=&behaviorNodes[childIDs[i]];
}

static void BuildTree(void)
{
	if(behaviorTreeBuilt)
		return;

	MakeLeaf(NODE_COND_DEAD, CondIsDead);
	MakeLeaf(NODE_ACT_DEAD, ActHandleDead);

	MakeLeaf(NODE_COND_STUN, CondIsStunned);
	MakeLeaf(NODE_ACT_STUN, ActCoastStunned);

	MakeLeaf(NODE_COND_LOW_HEALTH, CondIsLowHealth);
	MakeLeaf(NODE_ACT_RETREAT, ActRetreat);

	MakeLeaf(NODE_COND_ALERTED, CondIsAlerted);
	MakeLeaf(NODE_ACT_PURSUE, ActPursue);

	MakeLeaf(NODE_COND_IN_RANGE, CondInAttackRange);
	MakeLeaf(NODE_COND_ALIGNED, CondAlignedToAttack);
	MakeLeaf(NODE_ACT_ATTACK, ActAttack);

	MakeLeaf(NODE_COND_SEARCH, CondShouldSearch);
	MakeLeaf(NODE_ACT_SEARCH, ActSearch);

	MakeLeaf(NODE_ACT_PATROL, ActPatrol);

	MakeComposition(NODE_ATTACK_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_IN_RANGE, NODE_COND_ALIGNED, NODE_ACT_ATTACK }, 3);
	MakeComposition(NODE_COMBAT_SELECTOR, BT_NODE_SELECTOR, (int[]) { NODE_ATTACK_SEQUENCE, NODE_ACT_PURSUE }, 2);
	MakeComposition(NODE_COMBAT_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_ALERTED, NODE_COMBAT_SELECTOR }, 2);

	MakeComposition(NODE_DEAD_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_DEAD, NODE_ACT_DEAD }, 2);

	MakeComposition(NODE_STUN_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_STUN, NODE_ACT_STUN }, 2);

	MakeComposition(NODE_RETREAT_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_LOW_HEALTH, NODE_ACT_RETREAT }, 2);

	MakeComposition(NODE_SEARCH_SEQUENCE, BT_NODE_SEQUENCE, (int[]) { NODE_COND_SEARCH, NODE_ACT_SEARCH }, 2);

	MakeComposition(NODE_ROOT, BT_NODE_SELECTOR, (int[]) { NODE_DEAD_SEQUENCE, NODE_STUN_SEQUENCE, NODE_RETREAT_SEQUENCE, NODE_COMBAT_SEQUENCE, NODE_SEARCH_SEQUENCE, NODE_ACT_PATROL }, 6);

	behaviorTreeBuilt=true;
}

static BTStatus_t BT_Tick(BTNode_t *node, Enemy_t *enemy, const EntityList_t *list, const Camera_t *player)
{
	switch(node->type)
	{
		case BT_NODE_SEQUENCE:
		{
			for(uint32_t i=0;i<node->childCount;i++)
			{
				if(BT_Tick(node->children[i], enemy, list, player)==BT_FAILURE)
					return BT_FAILURE;
			}

			return BT_SUCCESS;
		}

		case BT_NODE_SELECTOR:
		{
			for(uint32_t i=0;i<node->childCount;i++)
			{
				if(BT_Tick(node->children[i], enemy, list, player)==BT_SUCCESS)
					return BT_SUCCESS;
			}

			return BT_FAILURE;
		}

		case BT_NODE_LEAF:
			return node->fn(enemy, list, player);
	}

	return BT_FAILURE;
}

static void UpdateSensors(Enemy_t *enemy, const Camera_t *player)
{
	float prevAlert=enemy->alertLevel;

	vec3 dir=Vec3_Subv(player->body.position, enemy->camera->body.position);
	float dist=Vec3_Normalize(&dir);
	float theta=Vec3_Dot(enemy->camera->forward, dir);

	if(dist<=ENEMY_SIGHT_DISTANCE&&theta>ENEMY_SIGHT_ANGLE)
	{
		enemy->alertLevel=fminf(1.0f, enemy->alertLevel+fTimeStep*ENEMY_ALERT_FILL_RATE);
		enemy->lastKnownPlayer=*player;
	}
	else
		enemy->alertLevel=fmaxf(0.0f, enemy->alertLevel-fTimeStep*ENEMY_ALERT_DRAIN_RATE);

	if(prevAlert<ENEMY_ALERT_PURSUING&&enemy->alertLevel>=ENEMY_ALERT_PURSUING)
		enemy->searchTimer=0.0f;
}

void InitEnemy(Enemy_t *enemy, Camera_t *enemyCamera, const Camera_t playerCamera)
{
	BuildTree();

	enemy->health=100.0f;
	enemy->state=ENEMY_STATE_PATROLLING;
	enemy->camera=enemyCamera;
	enemy->lastKnownPlayer=playerCamera;

	enemy->alertLevel=0.0f;
	enemy->searchTimer=0.0f;
	enemy->stunTimer=0.0f;

	enemy->fireCooldownTimer=0.0f;
	enemy->fireCooldown=RandFloatRange(ENEMY_FIRE_COOLDOWN_MIN, ENEMY_FIRE_COOLDOWN_MAX);

	enemy->strafeTimer=0.0f;
	enemy->strafeDuration=RandFloatRange(0.5f, 2.0f);
	enemy->strafeAngle=RandFloatRange(0.0f, 2.0f*PI);

	enemy->patrolOrigin=enemyCamera->body.position;
	enemy->patrolWaitTimer=0.0f;
	PatrolTarget(enemy);

	enemy->btRoot=&behaviorNodes[NODE_ROOT];
}

void DamageEnemy(Enemy_t *enemy, float damage)
{
	if(enemy->state==ENEMY_STATE_DEAD)
		return;

	enemy->health-=damage;
	enemy->alertLevel=1.0f;

	if(enemy->health>0.0f)
		enemy->stunTimer=ENEMY_STUN_DURATION;
}

void UpdateEnemy(Enemy_t *enemy, const EntityList_t *entityList, Camera_t player)
{
	UpdateSensors(enemy, &player);
	BT_Tick(enemy->btRoot, enemy, entityList, &player);
}
