#include <stdbool.h>
#include "math.h"

static vec4 NormalizePlane(vec4 p)
{
    float len=sqrtf(p.x*p.x+p.y*p.y+p.z*p.z);

    if(len>0.0f)
    {
        float inv=1.0f/len;
        p.x*=inv;
        p.y*=inv;
        p.z*=inv;
        p.w*=inv;
    }

    return p;
}

#define REVERSE_DEPTH

frustum Frustum_ExtractPlanes(const matrix m)
{
	const vec4 c0=Vec4(m.x.x, m.y.x, m.z.x, m.w.x);
	const vec4 c1=Vec4(m.x.y, m.y.y, m.z.y, m.w.y);
	const vec4 c2=Vec4(m.x.z, m.y.z, m.z.z, m.w.z);
	const vec4 c3=Vec4(m.x.w, m.y.w, m.z.w, m.w.w);

	return (frustum) {
		.planes={
			NormalizePlane(Vec4(c3.x+c0.x, c3.y+c0.y, c3.z+c0.z, c3.w+c0.w)),
			NormalizePlane(Vec4(c3.x-c0.x, c3.y-c0.y, c3.z-c0.z, c3.w-c0.w)),
			NormalizePlane(Vec4(c3.x+c1.x, c3.y+c1.y, c3.z+c1.z, c3.w+c1.w)),
			NormalizePlane(Vec4(c3.x-c1.x, c3.y-c1.y, c3.z-c1.z, c3.w-c1.w)),

#ifdef REVERSE_DEPTH
			NormalizePlane(Vec4(c3.x-c2.x, c3.y-c2.y, c3.z-c2.z, c3.w-c2.w)),
			NormalizePlane(Vec4(c3.x+c2.x, c3.y+c2.y, c3.z+c2.z, c3.w+c2.w)),
#else
			NormalizePlane(Vec4(c3.x+c2.x, c3.y+c2.y, c3.z+c2.z, c3.w+c2.w)),
			NormalizePlane(Vec4(c3.x-c2.x, c3.y-c2.y, c3.z-c2.z, c3.w-c2.w)),
#endif
		}
	};
}

bool Frustum_TestAABB(const frustum frustum, const aabb bounds)
{
	// Due to infinite far plane, test everything but the far plane
    for(uint32_t i=0;i<5;i++)
    {
        const vec4 *p=&frustum.planes[i];

        float px=(p->x>=0.0f)?bounds.max.x:bounds.min.x;
        float py=(p->y>=0.0f)?bounds.max.y:bounds.min.y;
        float pz=(p->z>=0.0f)?bounds.max.z:bounds.min.z;

        if((p->x*px+p->y*py+p->z*pz+p->w)<0.0f)
            return false;
    }

    return true;
}
