#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "physics/physics.h"
#include "physicslist.h"

uint32_t numPhysicsObjects=0;
PhysicsObject_t physicsObjects[MAX_PHYSICSOBJECTS];

void ResetPhysicsObjectList(void)
{
	numPhysicsObjects=0;
	memset(physicsObjects, 0, sizeof(PhysicsObject_t)*MAX_PHYSICSOBJECTS);
}

void AddPhysicsObject(RigidBody_t *physicsObject, PhysicsObjectType_e objectType)
{
	if(numPhysicsObjects>=MAX_PHYSICSOBJECTS)
	{
		DBGPRINTF(DEBUG_ERROR, "Ran out of physics object space.\n");
		return;
	}

	physicsObjects[numPhysicsObjects].objectType=objectType;
	physicsObjects[numPhysicsObjects++].rigidBody=physicsObject;
}
