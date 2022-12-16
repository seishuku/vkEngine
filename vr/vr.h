#ifndef __VR_H__
#define __VR_H__

#include <openvr/openvr_capi.h>

extern struct VR_IVRSystem_FnTable *VRSystem;
extern struct VR_IVRCompositor_FnTable *VRCompositor;

extern uint32_t rtWidth;
extern uint32_t rtHeight;

extern matrix EyeProjection[2];

void GetEyeProjection(EVREye Eye, matrix Projection);
void GetHeadPose(matrix Pose);
bool InitOpenVR(void);
void DestroyOpenVR(void);

#endif
