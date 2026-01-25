#ifndef __PHYSICSLIST_H__
#define __PHYSICSLIST_H__

#include "../math/math.h"
#include "physics.h"

typedef enum
{
	PHYSICSOBJECTTYPE_PLAYER,
	PHYSICSOBJECTTYPE_FIELD,
	PHYSICSOBJECTTYPE_PROJECTILE,
} PhysicsObjectType_e;

typedef struct
{
	aabb bounds;
	RigidBody_t *rigidBody;
	PhysicsObjectType_e objectType;
} PhysicsObject_t;

#define MAX_PHYSICSOBJECTS 10000

extern uint32_t numPhysicsObjects;
extern PhysicsObject_t physicsObjects[MAX_PHYSICSOBJECTS];

void ClearPhysicsObjectList(void);
void AddPhysicsObject(RigidBody_t *physicsObject, PhysicsObjectType_e objectType);

#endif
