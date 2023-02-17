#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vr.h"

intptr_t VR_InitInternal(EVRInitError *peError, EVRApplicationType eType);
void VR_ShutdownInternal();
int VR_IsHmdPresent();
intptr_t VR_GetGenericInterface(const char *pchInterfaceVersion, EVRInitError *peError);
int VR_IsRuntimeInstalled();
const char *VR_GetVRInitErrorAsSymbol(EVRInitError error);
const char *VR_GetVRInitErrorAsEnglishDescription(EVRInitError error);

struct VR_IVRSystem_FnTable *VRSystem=NULL;
struct VR_IVRCompositor_FnTable *VRCompositor=NULL;

uint32_t rtWidth;
uint32_t rtHeight;

matrix EyeProjection[2];

// Convert from OpenVR's 3x4 matrix to 4x4 matrix format
void HmdMatrix34toMatrix44(HmdMatrix34_t in, matrix out)
{
	out[0]=in.m[0][0];
	out[1]=in.m[1][0];
	out[2]=in.m[2][0];
	out[3]=0.0f;
	out[4]=in.m[0][1];
	out[5]=in.m[1][1];
	out[6]=in.m[2][1];
	out[7]=0.0f;
	out[8]=in.m[0][2];
	out[9]=in.m[1][2];
	out[10]=in.m[2][2];
	out[11]=0.0f;
	out[12]=in.m[0][3];
	out[13]=in.m[1][3];
	out[14]=in.m[2][3];
	out[15]=1.0f;
}

// Get the current projection and transform for selected eye and output a projection matrix for vulkan
void GetEyeProjection(EVREye Eye, matrix Projection)
{
	if(!VRSystem)
		return;

	matrix Transform;
	HmdMatrix44_t HmdProj;
	float zNear=0.01f;

	// Get projection matrix and copy into my matrix format
	HmdProj=VRSystem->GetProjectionMatrix(Eye, zNear, 1.0f);
	memcpy(Projection, &HmdProj, sizeof(matrix));

	// Row/Col major convert
	MatrixTranspose(Projection, Projection);

	// Y-Flip for vulkan
	Projection[5]*=-1.0f;

	// mod for infinite far plane
	Projection[10]=0.0f;
	Projection[11]=-1.0f;
	Projection[14]=zNear;

	// Inverse eye transform and multiply into projection matrix
	HmdMatrix34toMatrix44(VRSystem->GetEyeToHeadTransform(Eye), Transform);
	MatrixInverse(Transform, Transform);
	MatrixMult(Projection, Transform, Projection);
}

// Get current inverse head pose matrix
void GetHeadPose(matrix Pose)
{
	if(!VRCompositor)
		return;

	TrackedDevicePose_t trackedDevicePose[64];

	VRCompositor->WaitGetPoses(trackedDevicePose, k_unMaxTrackedDeviceCount, NULL, 0);

	if(trackedDevicePose[k_unTrackedDeviceIndex_Hmd].bDeviceIsConnected&&trackedDevicePose[k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
	{
		HmdMatrix34toMatrix44(trackedDevicePose[k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking, Pose);
		MatrixInverse(Pose, Pose);
	}
}

bool InitOpenVR(void)
{
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

	GetEyeProjection(EVREye_Eye_Left, EyeProjection[0]);
	GetEyeProjection(EVREye_Eye_Right, EyeProjection[1]);

	return true;
}

void DestroyOpenVR(void)
{
	if(VRSystem)
	{
		DBGPRINTF(DEBUG_INFO, "Shutting down OpenVR...\n");
		VR_ShutdownInternal();
		VRSystem=NULL;
	}
}
