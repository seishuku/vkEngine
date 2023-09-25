#ifndef __VR_H__
#define __VR_H__

// undefine __WIN32 for mingw/msys building, otherwise it tries to define bool
#undef __WIN32

#include <openvr/openvr_capi.h>

extern struct VR_IVRSystem_FnTable *VRSystem;
extern struct VR_IVRCompositor_FnTable *VRCompositor;

extern uint32_t rtWidth;
extern uint32_t rtHeight;

extern matrix EyeProjection[2];

matrix GetEyeProjection(EVREye Eye);
matrix GetHeadPose(void);
bool InitOpenVR(void);
void DestroyOpenVR(void);

#endif
