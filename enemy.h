#ifndef __ENEMY_H__
#define __ENEMY_H__

typedef enum
{
	PURSUING=0,
	SEARCHING,
	ATTACKING
} EnemyState_t;

typedef struct
{
	EnemyState_t state;
	Camera_t lastKnownPlayer;

	Camera_t *camera;
} Enemy_t;

void InitEnemy(Enemy_t *enemy, Camera_t *enemyCamera, const Camera_t playerCamera);
void UpdateEnemy(Enemy_t *enemy, Camera_t player);

#endif
