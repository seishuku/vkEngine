#include <float.h>
#include "../math/math.h"
#include "physics.h"

vec3 AttractorOBBComputeGravity(vec3 position, vec3 center, vec3 halfExtents, vec4 orientation, float baseGravity, float influenceRadius)
{
    vec3 local=Vec3_Subv(position, center);
    local=QuatRotate(QuatInverse(orientation), local);

    vec3 clampedLocal;
    clampedLocal.x=clampf(local.x, -halfExtents.x, halfExtents.x);
    clampedLocal.y=clampf(local.y, -halfExtents.y, halfExtents.y);
    clampedLocal.z=clampf(local.z, -halfExtents.z, halfExtents.z);

    vec3 closestWorld=Vec3_Addv(center, QuatRotate(orientation, clampedLocal));

    vec3 toSurface=Vec3_Subv(closestWorld, position);
    float dist=Vec3_Length(toSurface);

    vec3 gravityDir;
    if(dist>FLT_EPSILON)
        gravityDir=Vec3_Muls(toSurface, 1.0f/dist);
    else
    {
        float dx=halfExtents.x-fabsf(local.x);
        float dy=halfExtents.y-fabsf(local.y);
        float dz=halfExtents.z-fabsf(local.z);

        vec3 localNormal;

		if(dx<=dy&&dx<=dz)
            localNormal=(vec3) { local.x<0.0f?-1.0f:1.0f, 0.0f, 0.0f };
        else if(dy<=dx&&dy<=dz)
            localNormal=(vec3) { 0.0f, local.y<0.0f?-1.0f:1.0f, 0.0f };
        else
            localNormal=(vec3) { 0.0f, 0.0f, local.z<0.0f?-1.0f:1.0f };

        gravityDir=QuatRotate(orientation, localNormal);
        dist=0.0f;
    }

    float strength=baseGravity;
    float falloffStart=influenceRadius*0.8f;

    if(dist>influenceRadius)
        strength=0.0f;
    else if(dist>falloffStart)
    {
        float t=(influenceRadius-dist)/(influenceRadius-falloffStart);
        strength*=t*t*(3.0f-2.0f*t); // smoothstep
    }

    return Vec3_Muls(gravityDir, strength);
}

vec3 AttractorCapsuleComputeGravity(vec3 position, vec3 center, vec4 orientation, float radius, float halfHeight, float baseGravity, float influenceRadius)
{
    vec3 localAxis={ 0.0f, 1.0f, 0.0f }; // capsule's long axis in local space
    vec3 worldAxis=QuatRotate(orientation, localAxis);

    vec3 segStart=Vec3_Subv(center, Vec3_Muls(worldAxis, halfHeight));
    vec3 segEnd=Vec3_Addv(center, Vec3_Muls(worldAxis, halfHeight));

    vec3 segDir=Vec3_Subv(segEnd, segStart);
    float segLenSq=Vec3_LengthSq(segDir);

    float t=0.0f;
    if(segLenSq>FLT_EPSILON)
    {
        vec3 toPos=Vec3_Subv(position, segStart);
        t=clampf(Vec3_Dot(toPos, segDir)/segLenSq, 0.0f, 1.0f);
    }

    vec3 closestOnSeg=Vec3_Addv(segStart, Vec3_Muls(segDir, t));

    vec3 toClosest=Vec3_Subv(closestOnSeg, position);
    float distToClosest=Vec3_Length(toClosest);

    float dist=distToClosest-radius;

	if(dist<0.0f)
		dist=0.0f;

    vec3 gravityDir;

	if(distToClosest>FLT_EPSILON)
        gravityDir=Vec3_Muls(toClosest, 1.0f/distToClosest);
	else
		gravityDir=(vec3) { 0.0f, 1.0f, 0.0f };

    float strength=baseGravity;
    float falloffStart=influenceRadius*0.8f;

    if(dist>influenceRadius)
        strength=0.0f;
    else if(dist>falloffStart)
    {
        float t2=(influenceRadius-dist)/(influenceRadius-falloffStart);
        strength*=t2*t2*(3.0f-2.0f*t2);
    }

    return Vec3_Muls(gravityDir, strength);
}

vec3 AttractorSphereComputeGravity(vec3 position, vec3 center, float radius, float baseGravity, float influenceRadius)
{
    vec3 toCenter=Vec3_Subv(center, position);
    float distToCenter=Vec3_Length(toCenter);

    float dist=distToCenter-radius;

	if(dist<0.0f)
		dist=0.0f;

    vec3 gravityDir;

	if(distToCenter>FLT_EPSILON)
        gravityDir=Vec3_Muls(toCenter, 1.0f/distToCenter);
    else
        gravityDir=(vec3){ 0.0f, 1.0f, 0.0f };

    float strength=baseGravity;
    float falloffStart=influenceRadius * 0.8f;

    if(dist>influenceRadius)
        strength=0.0f;
    else if(dist>falloffStart)
    {
        float t=(influenceRadius-dist)/(influenceRadius-falloffStart);
        strength*=t*t*(3.0f-2.0f*t);
    }

    return Vec3_Muls(gravityDir, strength);
}
