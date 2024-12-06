#ifndef __PHYSICSLIST_H__
#define __PHYSICSLIST_H__

typedef enum
{
	PHYSICSOBJECTTYPE_PLAYER,
	PHYSICSOBJECTTYPE_FIELD,
	PHYSICSOBJECTTYPE_PROJECTILE,
} PhysicsObjectType_e;

typedef struct
{
	RigidBody_t *rigidBody;
	PhysicsObjectType_e objectType;
} PhysicsObject_t;

#define MAX_PHYSICSOBJECTS 2000

extern uint32_t numPhysicsObjects;
extern PhysicsObject_t physicsObjects[MAX_PHYSICSOBJECTS];

void ResetPhysicsObjectList(void);
void AddPhysicsObject(RigidBody_t *physicsObject, PhysicsObjectType_e objectType);

#endif
