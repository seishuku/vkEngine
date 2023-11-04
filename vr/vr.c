#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vr.h"

#ifndef ANDROID
intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
void VR_ShutdownInternal();
int VR_IsHmdPresent();
intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
int VR_IsRuntimeInstalled();
const char *VR_GetVRInitErrorAsSymbol(EVRInitError error);
const char *VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);

struct VR_IVRSystem_FnTable *VRSystem=NULL;
struct VR_IVRCompositor_FnTable *VRCompositor=NULL;
#endif

uint32_t rtWidth;
uint32_t rtHeight;

matrix EyeProjection[2];

#ifndef ANDROID
// Convert from OpenVR's 3x4 matrix to 4x4 matrix format
matrix HmdMatrix34toMatrix44(HmdMatrix34_t in)
{
	return (matrix)
	{
		{ in.m[0][0], in.m[1][0], in.m[2][0], 0.0f },
		{ in.m[0][1], in.m[1][1], in.m[2][1], 0.0f },
		{ in.m[0][2], in.m[1][2], in.m[2][2], 0.0f },
		{ in.m[0][3], in.m[1][3], in.m[2][3], 1.0f }
	};
}
#endif

// Get the current projection and transform for selected eye and output a projection matrix for vulkan
matrix GetEyeProjection(uint32_t Eye)
{
#ifndef ANDROID
	if(!VRSystem)
		return MatrixIdentity();

	matrix Projection;
	float zNear=0.01f;

	// Get projection matrix and copy into my matrix format
	HmdMatrix44_t HmdProj=VRSystem->GetProjectionMatrix(Eye, zNear, 1.0f);
	memcpy(&Projection, &HmdProj, sizeof(matrix));

	// Row/Col major convert
	Projection=MatrixTranspose(Projection);

	// Y-Flip for vulkan
	Projection.y.y*=-1.0f;

	// mod for infinite far plane
	Projection.z.z=0.0f;
	Projection.z.w=-1.0f;
	Projection.w.z=zNear;

	// Inverse eye transform and multiply into projection matrix
	return MatrixMult(Projection, MatrixInverse(HmdMatrix34toMatrix44(VRSystem->GetEyeToHeadTransform(Eye))));
#else
	return MatrixIdentity();
#endif
}

// Get current inverse head pose matrix
matrix GetHeadPose(void)
{
#ifndef ANDROID
	if(!VRCompositor)
		return MatrixIdentity();

	TrackedDevicePose_t trackedDevicePose[64];

	VRCompositor->WaitGetPoses(trackedDevicePose, k_unMaxTrackedDeviceCount, NULL, 0);

	if(trackedDevicePose[k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected&&trackedDevicePose[k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
		return MatrixInverse(HmdMatrix34toMatrix44(trackedDevicePose[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking));
#endif

	return MatrixIdentity();
}

bool InitOpenVR(void)
{
#ifndef ANDROID
	EVRInitError eError=EVRInitError_VRInitError_None;
	char fnTableName[128]="\0";

	if(!VR_IsHmdPresent())
	{
		DBGPRINTF(DEBUG_ERROR, "Error : HMD not detected on the system");
		return false;
	}

	if(!VR_IsRuntimeInstalled())
	{
		DBGPRINTF(DEBUG_ERROR, "Error : OpenVR Runtime not detected on the system");
		return false;
	}

	VR_InitInternal(&eError, EVRApplicationType_VRApplication_Scene);

	if(eError!=EVRInitError_VRInitError_None)
	{
		DBGPRINTF(DEBUG_ERROR, "VR_InitInternal: %s", VR_GetVRInitErrorAsSymbol(eError));
		return false;
	}

	sprintf(fnTableName, "FnTable:%s", IVRSystem_Version);
	VRSystem=(struct VR_IVRSystem_FnTable *)VR_GetGenericInterface(fnTableName, &eError);

	if(eError!=EVRInitError_VRInitError_None||!VRSystem)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to initialize VR compositor!\n ");
		return false;
	}

	sprintf(fnTableName, "FnTable:%s", IVRCompositor_Version);
	VRCompositor=(struct VR_IVRCompositor_FnTable *)VR_GetGenericInterface(fnTableName, &eError);
	
	if(eError!=EVRInitError_VRInitError_None||!VRCompositor)
	{
		DBGPRINTF(DEBUG_ERROR, "VR_GetGenericInterface(\"%s\"): %s", IVRCompositor_Version, VR_GetVRInitErrorAsSymbol(eError));
		return false;
	}

	VRSystem->GetRecommendedRenderTargetSize(&rtWidth, &rtHeight);

//	rtWidth>>=1;
//	rtHeight>>=1;

	ETrackedPropertyError tdError=ETrackedPropertyError_TrackedProp_Success;
	const float Freq=VRSystem->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, &tdError);

	if(tdError!=ETrackedPropertyError_TrackedProp_Success)
	{
		DBGPRINTF(DEBUG_ERROR, "IVRSystem::GetFloatTrackedDeviceProperty::Prop_DisplayFrequency failed: %d\n", tdError);
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "HMD suggested render target size: %d x %d @ %0.1fHz\n", rtWidth, rtHeight, Freq);

	EyeProjection[0]=GetEyeProjection(EVREye_Eye_Left);
	EyeProjection[1]=GetEyeProjection(EVREye_Eye_Right);

	return true;
#else
	return false;
#endif
}

void DestroyOpenVR(void)
{
#ifndef ANDROID
	if(VRSystem)
	{
		DBGPRINTF(DEBUG_INFO, "Shutting down OpenVR...\n");
		VR_ShutdownInternal();
		VRSystem=NULL;
	}
#endif
}
