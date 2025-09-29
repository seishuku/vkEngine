#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../physics/physics.h"
#include "physicslist.h"

uint32_t numPhysicsObjects=0;
PhysicsObject_t physicsObjects[MAX_PHYSICSOBJECTS];

static PhysicsObject_t SetPhysicsObject(RigidBody_t *body, PhysicsObjectType_e objectType)
{
    PhysicsObject_t object={ Vec3b(0.0f), Vec3b(0.0f), body, objectType };

    if(body->type==RIGIDBODY_SPHERE)
    {
        object.min=Vec3(body->position.x-body->radius, body->position.y-body->radius, body->position.z-body->radius);
        object.max=Vec3(body->position.x+body->radius, body->position.y+body->radius, body->position.z+body->radius);
    }
    else if(body->type==RIGIDBODY_OBB)
    {
        matrix orientation=QuatToMatrix(body->orientation);
        vec3 axis[3]=
        {
            Vec3(orientation.x.x, orientation.x.y, orientation.x.z),
            Vec3(orientation.y.x, orientation.y.y, orientation.y.z),
            Vec3(orientation.z.x, orientation.z.y, orientation.z.z)
        };

        vec3 extents={
            fabsf(axis[0].x)*body->size.x+fabsf(axis[1].x)*body->size.y+fabsf(axis[2].x)*body->size.z,
            fabsf(axis[0].y)*body->size.x+fabsf(axis[1].y)*body->size.y+fabsf(axis[2].y)*body->size.z,
            fabsf(axis[0].z)*body->size.x+fabsf(axis[1].z)*body->size.y+fabsf(axis[2].z)*body->size.z
        };

        object.min=Vec3_Subv(body->position, extents);
        object.max=Vec3_Addv(body->position, extents);
    }

    return object;
}

void ClearPhysicsObjectList(void)
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

    physicsObjects[numPhysicsObjects++]=SetPhysicsObject(physicsObject, objectType);
}
