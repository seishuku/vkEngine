#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "../math/math.h"
#include "../system/system.h"
#include <spirv-headers/spirv.h>

#define SPV_SPIRV_VERSION_MAJOR_PART(WORD) (((uint32_t)(WORD)>>16)&0xFF)
#define SPV_SPIRV_VERSION_MINOR_PART(WORD) (((uint32_t)(WORD)>>8)&0xFF)

const char *printSpvOp(SpvOp op)
{
	if(op==SpvOpNop)											return "SpvOpNop";
	else if(op==SpvOpUndef)										return "SpvOpUndef";
	else if(op==SpvOpSourceContinued)							return "SpvOpSourceContinued";
	else if(op==SpvOpSource)									return "SpvOpSource";
	else if(op==SpvOpSourceExtension)							return "SpvOpSourceExtension";
	else if(op==SpvOpName)										return "SpvOpName";
	else if(op==SpvOpMemberName)								return "SpvOpMemberName";
	else if(op==SpvOpString)									return "SpvOpString";
	else if(op==SpvOpLine)										return "SpvOpLine";
	else if(op==SpvOpExtension)									return "SpvOpExtension";
	else if(op==SpvOpExtInstImport)								return "SpvOpExtInstImport";
	else if(op==SpvOpExtInst)									return "SpvOpExtInst";
	else if(op==SpvOpMemoryModel)								return "SpvOpMemoryModel";
	else if(op==SpvOpEntryPoint)								return "SpvOpEntryPoint";
	else if(op==SpvOpExecutionMode)								return "SpvOpExecutionMode";
	else if(op==SpvOpCapability)								return "SpvOpCapability";
	else if(op==SpvOpTypeVoid)									return "SpvOpTypeVoid";
	else if(op==SpvOpTypeBool)									return "SpvOpTypeBool";
	else if(op==SpvOpTypeInt)									return "SpvOpTypeInt";
	else if(op==SpvOpTypeFloat)									return "SpvOpTypeFloat";
	else if(op==SpvOpTypeVector)								return "SpvOpTypeVector";
	else if(op==SpvOpTypeMatrix)								return "SpvOpTypeMatrix";
	else if(op==SpvOpTypeImage)									return "SpvOpTypeImage";
	else if(op==SpvOpTypeSampler)								return "SpvOpTypeSampler";
	else if(op==SpvOpTypeSampledImage)							return "SpvOpTypeSampledImage";
	else if(op==SpvOpTypeArray)									return "SpvOpTypeArray";
	else if(op==SpvOpTypeRuntimeArray)							return "SpvOpTypeRuntimeArray";
	else if(op==SpvOpTypeStruct)								return "SpvOpTypeStruct";
	else if(op==SpvOpTypeOpaque)								return "SpvOpTypeOpaque";
	else if(op==SpvOpTypePointer)								return "SpvOpTypePointer";
	else if(op==SpvOpTypeFunction)								return "SpvOpTypeFunction";
	else if(op==SpvOpTypeEvent)									return "SpvOpTypeEvent";
	else if(op==SpvOpTypeDeviceEvent)							return "SpvOpTypeDeviceEvent";
	else if(op==SpvOpTypeReserveId)								return "SpvOpTypeReserveId";
	else if(op==SpvOpTypeQueue)									return "SpvOpTypeQueue";
	else if(op==SpvOpTypePipe)									return "SpvOpTypePipe";
	else if(op==SpvOpTypeForwardPointer)						return "SpvOpTypeForwardPointer";
	else if(op==SpvOpConstantTrue)								return "SpvOpConstantTrue";
	else if(op==SpvOpConstantFalse)								return "SpvOpConstantFalse";
	else if(op==SpvOpConstant)									return "SpvOpConstant";
	else if(op==SpvOpConstantComposite)							return "SpvOpConstantComposite";
	else if(op==SpvOpConstantSampler)							return "SpvOpConstantSampler";
	else if(op==SpvOpConstantNull)								return "SpvOpConstantNull";
	else if(op==SpvOpSpecConstantTrue)							return "SpvOpSpecConstantTrue";
	else if(op==SpvOpSpecConstantFalse)							return "SpvOpSpecConstantFalse";
	else if(op==SpvOpSpecConstant)								return "SpvOpSpecConstant";
	else if(op==SpvOpSpecConstantComposite)						return "SpvOpSpecConstantComposite";
	else if(op==SpvOpSpecConstantOp)							return "SpvOpSpecConstantOp";
	else if(op==SpvOpFunction)									return "SpvOpFunction";
	else if(op==SpvOpFunctionParameter)							return "SpvOpFunctionParameter";
	else if(op==SpvOpFunctionEnd)								return "SpvOpFunctionEnd";
	else if(op==SpvOpFunctionCall)								return "SpvOpFunctionCall";
	else if(op==SpvOpVariable)									return "SpvOpVariable";
	else if(op==SpvOpImageTexelPointer)							return "SpvOpImageTexelPointer";
	else if(op==SpvOpLoad)										return "SpvOpLoad";
	else if(op==SpvOpStore)										return "SpvOpStore";
	else if(op==SpvOpCopyMemory)								return "SpvOpCopyMemory";
	else if(op==SpvOpCopyMemorySized)							return "SpvOpCopyMemorySized";
	else if(op==SpvOpAccessChain)								return "SpvOpAccessChain";
	else if(op==SpvOpInBoundsAccessChain)						return "SpvOpInBoundsAccessChain";
	else if(op==SpvOpPtrAccessChain)							return "SpvOpPtrAccessChain";
	else if(op==SpvOpArrayLength)								return "SpvOpArrayLength";
	else if(op==SpvOpGenericPtrMemSemantics)					return "SpvOpGenericPtrMemSemantics";
	else if(op==SpvOpInBoundsPtrAccessChain)					return "SpvOpInBoundsPtrAccessChain";
	else if(op==SpvOpDecorate)									return "SpvOpDecorate";
	else if(op==SpvOpMemberDecorate)							return "SpvOpMemberDecorate";
	else if(op==SpvOpDecorationGroup)							return "SpvOpDecorationGroup";
	else if(op==SpvOpGroupDecorate)								return "SpvOpGroupDecorate";
	else if(op==SpvOpGroupMemberDecorate)						return "SpvOpGroupMemberDecorate";
	else if(op==SpvOpVectorExtractDynamic)						return "SpvOpVectorExtractDynamic";
	else if(op==SpvOpVectorInsertDynamic)						return "SpvOpVectorInsertDynamic";
	else if(op==SpvOpVectorShuffle)								return "SpvOpVectorShuffle";
	else if(op==SpvOpCompositeConstruct)						return "SpvOpCompositeConstruct";
	else if(op==SpvOpCompositeExtract)							return "SpvOpCompositeExtract";
	else if(op==SpvOpCompositeInsert)							return "SpvOpCompositeInsert";
	else if(op==SpvOpCopyObject)								return "SpvOpCopyObject";
	else if(op==SpvOpTranspose)									return "SpvOpTranspose";
	else if(op==SpvOpSampledImage)								return "SpvOpSampledImage";
	else if(op==SpvOpImageSampleImplicitLod)					return "SpvOpImageSampleImplicitLod";
	else if(op==SpvOpImageSampleExplicitLod)					return "SpvOpImageSampleExplicitLod";
	else if(op==SpvOpImageSampleDrefImplicitLod)				return "SpvOpImageSampleDrefImplicitLod";
	else if(op==SpvOpImageSampleDrefExplicitLod)				return "SpvOpImageSampleDrefExplicitLod";
	else if(op==SpvOpImageSampleProjImplicitLod)				return "SpvOpImageSampleProjImplicitLod";
	else if(op==SpvOpImageSampleProjExplicitLod)				return "SpvOpImageSampleProjExplicitLod";
	else if(op==SpvOpImageSampleProjDrefImplicitLod)			return "SpvOpImageSampleProjDrefImplicitLod";
	else if(op==SpvOpImageSampleProjDrefExplicitLod)			return "SpvOpImageSampleProjDrefExplicitLod";
	else if(op==SpvOpImageFetch)								return "SpvOpImageFetch";
	else if(op==SpvOpImageGather)								return "SpvOpImageGather";
	else if(op==SpvOpImageDrefGather)							return "SpvOpImageDrefGather";
	else if(op==SpvOpImageRead)									return "SpvOpImageRead";
	else if(op==SpvOpImageWrite)								return "SpvOpImageWrite";
	else if(op==SpvOpImage)										return "SpvOpImage";
	else if(op==SpvOpImageQueryFormat)							return "SpvOpImageQueryFormat";
	else if(op==SpvOpImageQueryOrder)							return "SpvOpImageQueryOrder";
	else if(op==SpvOpImageQuerySizeLod)							return "SpvOpImageQuerySizeLod";
	else if(op==SpvOpImageQuerySize)							return "SpvOpImageQuerySize";
	else if(op==SpvOpImageQueryLod)								return "SpvOpImageQueryLod";
	else if(op==SpvOpImageQueryLevels)							return "SpvOpImageQueryLevels";
	else if(op==SpvOpImageQuerySamples)							return "SpvOpImageQuerySamples";
	else if(op==SpvOpConvertFToU)								return "SpvOpConvertFToU";
	else if(op==SpvOpConvertFToS)								return "SpvOpConvertFToS";
	else if(op==SpvOpConvertSToF)								return "SpvOpConvertSToF";
	else if(op==SpvOpConvertUToF)								return "SpvOpConvertUToF";
	else if(op==SpvOpUConvert)									return "SpvOpUConvert";
	else if(op==SpvOpSConvert)									return "SpvOpSConvert";
	else if(op==SpvOpFConvert)									return "SpvOpFConvert";
	else if(op==SpvOpQuantizeToF16)								return "SpvOpQuantizeToF16";
	else if(op==SpvOpConvertPtrToU)								return "SpvOpConvertPtrToU";
	else if(op==SpvOpSatConvertSToU)							return "SpvOpSatConvertSToU";
	else if(op==SpvOpSatConvertUToS)							return "SpvOpSatConvertUToS";
	else if(op==SpvOpConvertUToPtr)								return "SpvOpConvertUToPtr";
	else if(op==SpvOpPtrCastToGeneric)							return "SpvOpPtrCastToGeneric";
	else if(op==SpvOpGenericCastToPtr)							return "SpvOpGenericCastToPtr";
	else if(op==SpvOpGenericCastToPtrExplicit)					return "SpvOpGenericCastToPtrExplicit";
	else if(op==SpvOpBitcast)									return "SpvOpBitcast";
	else if(op==SpvOpSNegate)									return "SpvOpSNegate";
	else if(op==SpvOpFNegate)									return "SpvOpFNegate";
	else if(op==SpvOpIAdd)										return "SpvOpIAdd";
	else if(op==SpvOpFAdd)										return "SpvOpFAdd";
	else if(op==SpvOpISub)										return "SpvOpISub";
	else if(op==SpvOpFSub)										return "SpvOpFSub";
	else if(op==SpvOpIMul)										return "SpvOpIMul";
	else if(op==SpvOpFMul)										return "SpvOpFMul";
	else if(op==SpvOpUDiv)										return "SpvOpUDiv";
	else if(op==SpvOpSDiv)										return "SpvOpSDiv";
	else if(op==SpvOpFDiv)										return "SpvOpFDiv";
	else if(op==SpvOpUMod)										return "SpvOpUMod";
	else if(op==SpvOpSRem)										return "SpvOpSRem";
	else if(op==SpvOpSMod)										return "SpvOpSMod";
	else if(op==SpvOpFRem)										return "SpvOpFRem";
	else if(op==SpvOpFMod)										return "SpvOpFMod";
	else if(op==SpvOpVectorTimesScalar)							return "SpvOpVectorTimesScalar";
	else if(op==SpvOpMatrixTimesScalar)							return "SpvOpMatrixTimesScalar";
	else if(op==SpvOpVectorTimesMatrix)							return "SpvOpVectorTimesMatrix";
	else if(op==SpvOpMatrixTimesVector)							return "SpvOpMatrixTimesVector";
	else if(op==SpvOpMatrixTimesMatrix)							return "SpvOpMatrixTimesMatrix";
	else if(op==SpvOpOuterProduct)								return "SpvOpOuterProduct";
	else if(op==SpvOpDot)										return "SpvOpDot";
	else if(op==SpvOpIAddCarry)									return "SpvOpIAddCarry";
	else if(op==SpvOpISubBorrow)								return "SpvOpISubBorrow";
	else if(op==SpvOpUMulExtended)								return "SpvOpUMulExtended";
	else if(op==SpvOpSMulExtended)								return "SpvOpSMulExtended";
	else if(op==SpvOpAny)										return "SpvOpAny";
	else if(op==SpvOpAll)										return "SpvOpAll";
	else if(op==SpvOpIsNan)										return "SpvOpIsNan";
	else if(op==SpvOpIsInf)										return "SpvOpIsInf";
	else if(op==SpvOpIsFinite)									return "SpvOpIsFinite";
	else if(op==SpvOpIsNormal)									return "SpvOpIsNormal";
	else if(op==SpvOpSignBitSet)								return "SpvOpSignBitSet";
	else if(op==SpvOpLessOrGreater)								return "SpvOpLessOrGreater";
	else if(op==SpvOpOrdered)									return "SpvOpOrdered";
	else if(op==SpvOpUnordered)									return "SpvOpUnordered";
	else if(op==SpvOpLogicalEqual)								return "SpvOpLogicalEqual";
	else if(op==SpvOpLogicalNotEqual)							return "SpvOpLogicalNotEqual";
	else if(op==SpvOpLogicalOr)									return "SpvOpLogicalOr";
	else if(op==SpvOpLogicalAnd)								return "SpvOpLogicalAnd";
	else if(op==SpvOpLogicalNot)								return "SpvOpLogicalNot";
	else if(op==SpvOpSelect)									return "SpvOpSelect";
	else if(op==SpvOpIEqual)									return "SpvOpIEqual";
	else if(op==SpvOpINotEqual)									return "SpvOpINotEqual";
	else if(op==SpvOpUGreaterThan)								return "SpvOpUGreaterThan";
	else if(op==SpvOpSGreaterThan)								return "SpvOpSGreaterThan";
	else if(op==SpvOpUGreaterThanEqual)							return "SpvOpUGreaterThanEqual";
	else if(op==SpvOpSGreaterThanEqual)							return "SpvOpSGreaterThanEqual";
	else if(op==SpvOpULessThan)									return "SpvOpULessThan";
	else if(op==SpvOpSLessThan)									return "SpvOpSLessThan";
	else if(op==SpvOpULessThanEqual)							return "SpvOpULessThanEqual";
	else if(op==SpvOpSLessThanEqual)							return "SpvOpSLessThanEqual";
	else if(op==SpvOpFOrdEqual)									return "SpvOpFOrdEqual";
	else if(op==SpvOpFUnordEqual)								return "SpvOpFUnordEqual";
	else if(op==SpvOpFOrdNotEqual)								return "SpvOpFOrdNotEqual";
	else if(op==SpvOpFUnordNotEqual)							return "SpvOpFUnordNotEqual";
	else if(op==SpvOpFOrdLessThan)								return "SpvOpFOrdLessThan";
	else if(op==SpvOpFUnordLessThan)							return "SpvOpFUnordLessThan";
	else if(op==SpvOpFOrdGreaterThan)							return "SpvOpFOrdGreaterThan";
	else if(op==SpvOpFUnordGreaterThan)							return "SpvOpFUnordGreaterThan";
	else if(op==SpvOpFOrdLessThanEqual)							return "SpvOpFOrdLessThanEqual";
	else if(op==SpvOpFUnordLessThanEqual)						return "SpvOpFUnordLessThanEqual";
	else if(op==SpvOpFOrdGreaterThanEqual)						return "SpvOpFOrdGreaterThanEqual";
	else if(op==SpvOpFUnordGreaterThanEqual)					return "SpvOpFUnordGreaterThanEqual";
	else if(op==SpvOpShiftRightLogical)							return "SpvOpShiftRightLogical";
	else if(op==SpvOpShiftRightArithmetic)						return "SpvOpShiftRightArithmetic";
	else if(op==SpvOpShiftLeftLogical)							return "SpvOpShiftLeftLogical";
	else if(op==SpvOpBitwiseOr)									return "SpvOpBitwiseOr";
	else if(op==SpvOpBitwiseXor)								return "SpvOpBitwiseXor";
	else if(op==SpvOpBitwiseAnd)								return "SpvOpBitwiseAnd";
	else if(op==SpvOpNot)										return "SpvOpNot";
	else if(op==SpvOpBitFieldInsert)							return "SpvOpBitFieldInsert";
	else if(op==SpvOpBitFieldSExtract)							return "SpvOpBitFieldSExtract";
	else if(op==SpvOpBitFieldUExtract)							return "SpvOpBitFieldUExtract";
	else if(op==SpvOpBitReverse)								return "SpvOpBitReverse";
	else if(op==SpvOpBitCount)									return "SpvOpBitCount";
	else if(op==SpvOpDPdx)										return "SpvOpDPdx";
	else if(op==SpvOpDPdy)										return "SpvOpDPdy";
	else if(op==SpvOpFwidth)									return "SpvOpFwidth";
	else if(op==SpvOpDPdxFine)									return "SpvOpDPdxFine";
	else if(op==SpvOpDPdyFine)									return "SpvOpDPdyFine";
	else if(op==SpvOpFwidthFine)								return "SpvOpFwidthFine";
	else if(op==SpvOpDPdxCoarse)								return "SpvOpDPdxCoarse";
	else if(op==SpvOpDPdyCoarse)								return "SpvOpDPdyCoarse";
	else if(op==SpvOpFwidthCoarse)								return "SpvOpFwidthCoarse";
	else if(op==SpvOpEmitVertex)								return "SpvOpEmitVertex";
	else if(op==SpvOpEndPrimitive)								return "SpvOpEndPrimitive";
	else if(op==SpvOpEmitStreamVertex)							return "SpvOpEmitStreamVertex";
	else if(op==SpvOpEndStreamPrimitive)						return "SpvOpEndStreamPrimitive";
	else if(op==SpvOpControlBarrier)							return "SpvOpControlBarrier";
	else if(op==SpvOpMemoryBarrier)								return "SpvOpMemoryBarrier";
	else if(op==SpvOpAtomicLoad)								return "SpvOpAtomicLoad";
	else if(op==SpvOpAtomicStore)								return "SpvOpAtomicStore";
	else if(op==SpvOpAtomicExchange)							return "SpvOpAtomicExchange";
	else if(op==SpvOpAtomicCompareExchange)						return "SpvOpAtomicCompareExchange";
	else if(op==SpvOpAtomicCompareExchangeWeak)					return "SpvOpAtomicCompareExchangeWeak";
	else if(op==SpvOpAtomicIIncrement)							return "SpvOpAtomicIIncrement";
	else if(op==SpvOpAtomicIDecrement)							return "SpvOpAtomicIDecrement";
	else if(op==SpvOpAtomicIAdd)								return "SpvOpAtomicIAdd";
	else if(op==SpvOpAtomicISub)								return "SpvOpAtomicISub";
	else if(op==SpvOpAtomicSMin)								return "SpvOpAtomicSMin";
	else if(op==SpvOpAtomicUMin)								return "SpvOpAtomicUMin";
	else if(op==SpvOpAtomicSMax)								return "SpvOpAtomicSMax";
	else if(op==SpvOpAtomicUMax)								return "SpvOpAtomicUMax";
	else if(op==SpvOpAtomicAnd)									return "SpvOpAtomicAnd";
	else if(op==SpvOpAtomicOr)									return "SpvOpAtomicOr";
	else if(op==SpvOpAtomicXor)									return "SpvOpAtomicXor";
	else if(op==SpvOpPhi)										return "SpvOpPhi";
	else if(op==SpvOpLoopMerge)									return "SpvOpLoopMerge";
	else if(op==SpvOpSelectionMerge)							return "SpvOpSelectionMerge";
	else if(op==SpvOpLabel)										return "SpvOpLabel";
	else if(op==SpvOpBranch)									return "SpvOpBranch";
	else if(op==SpvOpBranchConditional)							return "SpvOpBranchConditional";
	else if(op==SpvOpSwitch)									return "SpvOpSwitch";
	else if(op==SpvOpKill)										return "SpvOpKill";
	else if(op==SpvOpReturn)									return "SpvOpReturn";
	else if(op==SpvOpReturnValue)								return "SpvOpReturnValue";
	else if(op==SpvOpUnreachable)								return "SpvOpUnreachable";
	else if(op==SpvOpLifetimeStart)								return "SpvOpLifetimeStart";
	else if(op==SpvOpLifetimeStop)								return "SpvOpLifetimeStop";
	else if(op==SpvOpGroupAsyncCopy)							return "SpvOpGroupAsyncCopy";
	else if(op==SpvOpGroupWaitEvents)							return "SpvOpGroupWaitEvents";
	else if(op==SpvOpGroupAll)									return "SpvOpGroupAll";
	else if(op==SpvOpGroupAny)									return "SpvOpGroupAny";
	else if(op==SpvOpGroupBroadcast)							return "SpvOpGroupBroadcast";
	else if(op==SpvOpGroupIAdd)									return "SpvOpGroupIAdd";
	else if(op==SpvOpGroupFAdd)									return "SpvOpGroupFAdd";
	else if(op==SpvOpGroupFMin)									return "SpvOpGroupFMin";
	else if(op==SpvOpGroupUMin)									return "SpvOpGroupUMin";
	else if(op==SpvOpGroupSMin)									return "SpvOpGroupSMin";
	else if(op==SpvOpGroupFMax)									return "SpvOpGroupFMax";
	else if(op==SpvOpGroupUMax)									return "SpvOpGroupUMax";
	else if(op==SpvOpGroupSMax)									return "SpvOpGroupSMax";
	else if(op==SpvOpReadPipe)									return "SpvOpReadPipe";
	else if(op==SpvOpWritePipe)									return "SpvOpWritePipe";
	else if(op==SpvOpReservedReadPipe)							return "SpvOpReservedReadPipe";
	else if(op==SpvOpReservedWritePipe)							return "SpvOpReservedWritePipe";
	else if(op==SpvOpReserveReadPipePackets)					return "SpvOpReserveReadPipePackets";
	else if(op==SpvOpReserveWritePipePackets)					return "SpvOpReserveWritePipePackets";
	else if(op==SpvOpCommitReadPipe)							return "SpvOpCommitReadPipe";
	else if(op==SpvOpCommitWritePipe)							return "SpvOpCommitWritePipe";
	else if(op==SpvOpIsValidReserveId)							return "SpvOpIsValidReserveId";
	else if(op==SpvOpGetNumPipePackets)							return "SpvOpGetNumPipePackets";
	else if(op==SpvOpGetMaxPipePackets)							return "SpvOpGetMaxPipePackets";
	else if(op==SpvOpGroupReserveReadPipePackets)				return "SpvOpGroupReserveReadPipePackets";
	else if(op==SpvOpGroupReserveWritePipePackets)				return "SpvOpGroupReserveWritePipePackets";
	else if(op==SpvOpGroupCommitReadPipe)						return "SpvOpGroupCommitReadPipe";
	else if(op==SpvOpGroupCommitWritePipe)						return "SpvOpGroupCommitWritePipe";
	else if(op==SpvOpEnqueueMarker)								return "SpvOpEnqueueMarker";
	else if(op==SpvOpEnqueueKernel)								return "SpvOpEnqueueKernel";
	else if(op==SpvOpGetKernelNDrangeSubGroupCount)				return "SpvOpGetKernelNDrangeSubGroupCount";
	else if(op==SpvOpGetKernelNDrangeMaxSubGroupSize)			return "SpvOpGetKernelNDrangeMaxSubGroupSize";
	else if(op==SpvOpGetKernelWorkGroupSize)					return "SpvOpGetKernelWorkGroupSize";
	else if(op==SpvOpGetKernelPreferredWorkGroupSizeMultiple)	return "SpvOpGetKernelPreferredWorkGroupSizeMultiple";
	else if(op==SpvOpRetainEvent)								return "SpvOpRetainEvent";
	else if(op==SpvOpReleaseEvent)								return "SpvOpReleaseEvent";
	else if(op==SpvOpCreateUserEvent)							return "SpvOpCreateUserEvent";
	else if(op==SpvOpIsValidEvent)								return "SpvOpIsValidEvent";
	else if(op==SpvOpSetUserEventStatus)						return "SpvOpSetUserEventStatus";
	else if(op==SpvOpCaptureEventProfilingInfo)					return "SpvOpCaptureEventProfilingInfo";
	else if(op==SpvOpGetDefaultQueue)							return "SpvOpGetDefaultQueue";
	else if(op==SpvOpBuildNDRange)								return "SpvOpBuildNDRange";
	else if(op==SpvOpImageSparseSampleImplicitLod)				return "SpvOpImageSparseSampleImplicitLod";
	else if(op==SpvOpImageSparseSampleExplicitLod)				return "SpvOpImageSparseSampleExplicitLod";
	else if(op==SpvOpImageSparseSampleDrefImplicitLod)			return "SpvOpImageSparseSampleDrefImplicitLod";
	else if(op==SpvOpImageSparseSampleDrefExplicitLod)			return "SpvOpImageSparseSampleDrefExplicitLod";
	else if(op==SpvOpImageSparseSampleProjImplicitLod)			return "SpvOpImageSparseSampleProjImplicitLod";
	else if(op==SpvOpImageSparseSampleProjExplicitLod)			return "SpvOpImageSparseSampleProjExplicitLod";
	else if(op==SpvOpImageSparseSampleProjDrefImplicitLod)		return "SpvOpImageSparseSampleProjDrefImplicitLod";
	else if(op==SpvOpImageSparseSampleProjDrefExplicitLod)		return "SpvOpImageSparseSampleProjDrefExplicitLod";
	else if(op==SpvOpImageSparseFetch)							return "SpvOpImageSparseFetch";
	else if(op==SpvOpImageSparseGather)							return "SpvOpImageSparseGather";
	else if(op==SpvOpImageSparseDrefGather)						return "SpvOpImageSparseDrefGather";
	else if(op==SpvOpImageSparseTexelsResident)					return "SpvOpImageSparseTexelsResident";
	else if(op==SpvOpNoLine)									return "SpvOpNoLine";
	else if(op==SpvOpAtomicFlagTestAndSet)						return "SpvOpAtomicFlagTestAndSet";
	else if(op==SpvOpAtomicFlagClear)							return "SpvOpAtomicFlagClear";
	else if(op==SpvOpImageSparseRead)							return "SpvOpImageSparseRead";
	else if(op==SpvOpSizeOf)									return "SpvOpSizeOf";
	else if(op==SpvOpTypePipeStorage)							return "SpvOpTypePipeStorage";
	else if(op==SpvOpConstantPipeStorage)						return "SpvOpConstantPipeStorage";
	else if(op==SpvOpCreatePipeFromPipeStorage)					return "SpvOpCreatePipeFromPipeStorage";
	else if(op==SpvOpGetKernelLocalSizeForSubgroupCount)		return "SpvOpGetKernelLocalSizeForSubgroupCount";
	else if(op==SpvOpGetKernelMaxNumSubgroups)					return "SpvOpGetKernelMaxNumSubgroups";
	else if(op==SpvOpTypeNamedBarrier)							return "SpvOpTypeNamedBarrier";
	else if(op==SpvOpNamedBarrierInitialize)					return "SpvOpNamedBarrierInitialize";
	else if(op==SpvOpMemoryNamedBarrier)						return "SpvOpMemoryNamedBarrier";
	else if(op==SpvOpModuleProcessed)							return "SpvOpModuleProcessed";
	else if(op==SpvOpExecutionModeId)							return "SpvOpExecutionModeId";
	else if(op==SpvOpDecorateId)								return "SpvOpDecorateId";
	else if(op==SpvOpGroupNonUniformElect)						return "SpvOpGroupNonUniformElect";
	else if(op==SpvOpGroupNonUniformAll)						return "SpvOpGroupNonUniformAll";
	else if(op==SpvOpGroupNonUniformAny)						return "SpvOpGroupNonUniformAny";
	else if(op==SpvOpGroupNonUniformAllEqual)					return "SpvOpGroupNonUniformAllEqual";
	else if(op==SpvOpGroupNonUniformBroadcast)					return "SpvOpGroupNonUniformBroadcast";
	else if(op==SpvOpGroupNonUniformBroadcastFirst)				return "SpvOpGroupNonUniformBroadcastFirst";
	else if(op==SpvOpGroupNonUniformBallot)						return "SpvOpGroupNonUniformBallot";
	else if(op==SpvOpGroupNonUniformInverseBallot)				return "SpvOpGroupNonUniformInverseBallot";
	else if(op==SpvOpGroupNonUniformBallotBitExtract)			return "SpvOpGroupNonUniformBallotBitExtract";
	else if(op==SpvOpGroupNonUniformBallotBitCount)				return "SpvOpGroupNonUniformBallotBitCount";
	else if(op==SpvOpGroupNonUniformBallotFindLSB)				return "SpvOpGroupNonUniformBallotFindLSB";
	else if(op==SpvOpGroupNonUniformBallotFindMSB)				return "SpvOpGroupNonUniformBallotFindMSB";
	else if(op==SpvOpGroupNonUniformShuffle)					return "SpvOpGroupNonUniformShuffle";
	else if(op==SpvOpGroupNonUniformShuffleXor)					return "SpvOpGroupNonUniformShuffleXor";
	else if(op==SpvOpGroupNonUniformShuffleUp)					return "SpvOpGroupNonUniformShuffleUp";
	else if(op==SpvOpGroupNonUniformShuffleDown)				return "SpvOpGroupNonUniformShuffleDown";
	else if(op==SpvOpGroupNonUniformIAdd)						return "SpvOpGroupNonUniformIAdd";
	else if(op==SpvOpGroupNonUniformFAdd)						return "SpvOpGroupNonUniformFAdd";
	else if(op==SpvOpGroupNonUniformIMul)						return "SpvOpGroupNonUniformIMul";
	else if(op==SpvOpGroupNonUniformFMul)						return "SpvOpGroupNonUniformFMul";
	else if(op==SpvOpGroupNonUniformSMin)						return "SpvOpGroupNonUniformSMin";
	else if(op==SpvOpGroupNonUniformUMin)						return "SpvOpGroupNonUniformUMin";
	else if(op==SpvOpGroupNonUniformFMin)						return "SpvOpGroupNonUniformFMin";
	else if(op==SpvOpGroupNonUniformSMax)						return "SpvOpGroupNonUniformSMax";
	else if(op==SpvOpGroupNonUniformUMax)						return "SpvOpGroupNonUniformUMax";
	else if(op==SpvOpGroupNonUniformFMax)						return "SpvOpGroupNonUniformFMax";
	else if(op==SpvOpGroupNonUniformBitwiseAnd)					return "SpvOpGroupNonUniformBitwiseAnd";
	else if(op==SpvOpGroupNonUniformBitwiseOr)					return "SpvOpGroupNonUniformBitwiseOr";
	else if(op==SpvOpGroupNonUniformBitwiseXor)					return "SpvOpGroupNonUniformBitwiseXor";
	else if(op==SpvOpGroupNonUniformLogicalAnd)					return "SpvOpGroupNonUniformLogicalAnd";
	else if(op==SpvOpGroupNonUniformLogicalOr)					return "SpvOpGroupNonUniformLogicalOr";
	else if(op==SpvOpGroupNonUniformLogicalXor)					return "SpvOpGroupNonUniformLogicalXor";
	else if(op==SpvOpGroupNonUniformQuadBroadcast)				return "SpvOpGroupNonUniformQuadBroadcast";
	else if(op==SpvOpGroupNonUniformQuadSwap)					return "SpvOpGroupNonUniformQuadSwap";
	else if(op==SpvOpCopyLogical)								return "SpvOpCopyLogical";
	else if(op==SpvOpPtrEqual)									return "SpvOpPtrEqual";
	else if(op==SpvOpPtrNotEqual)								return "SpvOpPtrNotEqual";
	else if(op==SpvOpPtrDiff)									return "SpvOpPtrDiff";
	else														return "unknown";
}

const char *printSpvDecoration(SpvDecoration decoration)
{
	if(decoration==SpvDecorationRelaxedPrecision)			return "SpvDecorationRelaxedPrecision";
	else if(decoration==SpvDecorationSpecId)				return "SpvDecorationSpecId";
	else if(decoration==SpvDecorationBlock)					return "SpvDecorationBlock";
	else if(decoration==SpvDecorationBufferBlock)			return "SpvDecorationBufferBlock";
	else if(decoration==SpvDecorationRowMajor)				return "SpvDecorationRowMajor";
	else if(decoration==SpvDecorationColMajor)				return "SpvDecorationColMajor";
	else if(decoration==SpvDecorationArrayStride)			return "SpvDecorationArrayStride";
	else if(decoration==SpvDecorationMatrixStride)			return "SpvDecorationMatrixStride";
	else if(decoration==SpvDecorationGLSLShared)			return "SpvDecorationGLSLShared";
	else if(decoration==SpvDecorationGLSLPacked)			return "SpvDecorationGLSLPacked";
	else if(decoration==SpvDecorationCPacked)				return "SpvDecorationCPacked";
	else if(decoration==SpvDecorationBuiltIn)				return "SpvDecorationBuiltIn";
	else if(decoration==SpvDecorationNoPerspective)			return "SpvDecorationNoPerspective";
	else if(decoration==SpvDecorationFlat)					return "SpvDecorationFlat";
	else if(decoration==SpvDecorationPatch)					return "SpvDecorationPatch";
	else if(decoration==SpvDecorationCentroid)				return "SpvDecorationCentroid";
	else if(decoration==SpvDecorationSample)				return "SpvDecorationSample";
	else if(decoration==SpvDecorationInvariant)				return "SpvDecorationInvariant";
	else if(decoration==SpvDecorationRestrict)				return "SpvDecorationRestrict";
	else if(decoration==SpvDecorationAliased)				return "SpvDecorationAliased";
	else if(decoration==SpvDecorationVolatile)				return "SpvDecorationVolatile";
	else if(decoration==SpvDecorationConstant)				return "SpvDecorationConstant";
	else if(decoration==SpvDecorationCoherent)				return "SpvDecorationCoherent";
	else if(decoration==SpvDecorationNonWritable)			return "SpvDecorationNonWritable";
	else if(decoration==SpvDecorationNonReadable)			return "SpvDecorationNonReadable";
	else if(decoration==SpvDecorationUniform)				return "SpvDecorationUniform";
	else if(decoration==SpvDecorationUniformId)				return "SpvDecorationUniformId";
	else if(decoration==SpvDecorationSaturatedConversion)	return "SpvDecorationSaturatedConversion";
	else if(decoration==SpvDecorationStream)				return "SpvDecorationStream";
	else if(decoration==SpvDecorationLocation)				return "SpvDecorationLocation";
	else if(decoration==SpvDecorationComponent)				return "SpvDecorationComponent";
	else if(decoration==SpvDecorationIndex)					return "SpvDecorationIndex";
	else if(decoration==SpvDecorationBinding)				return "SpvDecorationBinding";
	else if(decoration==SpvDecorationDescriptorSet)			return "SpvDecorationDescriptorSet";
	else if(decoration==SpvDecorationOffset)				return "SpvDecorationOffset";
	else if(decoration==SpvDecorationXfbBuffer)				return "SpvDecorationXfbBuffer";
	else if(decoration==SpvDecorationXfbStride)				return "SpvDecorationXfbStride";
	else if(decoration==SpvDecorationFuncParamAttr)			return "SpvDecorationFuncParamAttr";
	else if(decoration==SpvDecorationFPRoundingMode)		return "SpvDecorationFPRoundingMode";
	else if(decoration==SpvDecorationFPFastMathMode)		return "SpvDecorationFPFastMathMode";
	else if(decoration==SpvDecorationLinkageAttributes)		return "SpvDecorationLinkageAttributes";
	else if(decoration==SpvDecorationNoContraction)			return "SpvDecorationNoContraction";
	else if(decoration==SpvDecorationInputAttachmentIndex)	return "SpvDecorationInputAttachmentIndex";
	else if(decoration==SpvDecorationAlignment)				return "SpvDecorationAlignment";
	else if(decoration==SpvDecorationMaxByteOffset)			return "SpvDecorationMaxByteOffset";
	else if(decoration==SpvDecorationAlignmentId)			return "SpvDecorationAlignmentId";
	else if(decoration==SpvDecorationMaxByteOffsetId)		return "SpvDecorationMaxByteOffsetId";
	else													return "SpvDecorationUnknown";
}

const char *printSpvExecutionModel(SpvExecutionModel executionModel)
{
	if(executionModel==SpvExecutionModelVertex)							return "SpvExecutionModelVertex";
	else if(executionModel==SpvExecutionModelTessellationControl)		return "SpvExecutionModelTessellationControl";
	else if(executionModel==SpvExecutionModelTessellationEvaluation)	return "SpvExecutionModelTessellationEvaluation";
	else if(executionModel==SpvExecutionModelGeometry)					return "SpvExecutionModelGeometry";
	else if(executionModel==SpvExecutionModelFragment)					return "SpvExecutionModelFragment";
	else if(executionModel==SpvExecutionModelGLCompute)					return "SpvExecutionModelGLCompute";
	else if(executionModel==SpvExecutionModelKernel)					return "SpvExecutionModelKernel";
	else																return "SpvExecutionModelUnknown";
}

const char *printSpvExecutionMode(SpvExecutionMode executionMode)
{
	if(executionMode==SpvExecutionModeInvocations)					return "SpvExecutionModeInvocations";
	else if(executionMode==SpvExecutionModeSpacingEqual)			return "SpvExecutionModeSpacingEqual";
	else if(executionMode==SpvExecutionModeSpacingFractionalEven)	return "SpvExecutionModeSpacingFractionalEven";
	else if(executionMode==SpvExecutionModeSpacingFractionalOdd)	return "SpvExecutionModeSpacingFractionalOdd";
	else if(executionMode==SpvExecutionModeVertexOrderCw)			return "SpvExecutionModeVertexOrderCw";
	else if(executionMode==SpvExecutionModeVertexOrderCcw)			return "SpvExecutionModeVertexOrderCcw";
	else if(executionMode==SpvExecutionModePixelCenterInteger)		return "SpvExecutionModePixelCenterInteger";
	else if(executionMode==SpvExecutionModeOriginUpperLeft)			return "SpvExecutionModeOriginUpperLeft";
	else if(executionMode==SpvExecutionModeOriginLowerLeft)			return "SpvExecutionModeOriginLowerLeft";
	else if(executionMode==SpvExecutionModeEarlyFragmentTests)		return "SpvExecutionModeEarlyFragmentTests";
	else if(executionMode==SpvExecutionModePointMode)				return "SpvExecutionModePointMode";
	else if(executionMode==SpvExecutionModeXfb)						return "SpvExecutionModeXfb";
	else if(executionMode==SpvExecutionModeDepthReplacing)			return "SpvExecutionModeDepthReplacing";
	else if(executionMode==SpvExecutionModeDepthGreater)			return "SpvExecutionModeDepthGreater";
	else if(executionMode==SpvExecutionModeDepthLess)				return "SpvExecutionModeDepthLess";
	else if(executionMode==SpvExecutionModeDepthUnchanged)			return "SpvExecutionModeDepthUnchanged";
	else if(executionMode==SpvExecutionModeLocalSize)				return "SpvExecutionModeLocalSize";
	else if(executionMode==SpvExecutionModeLocalSizeHint)			return "SpvExecutionModeLocalSizeHint";
	else if(executionMode==SpvExecutionModeInputPoints)				return "SpvExecutionModeInputPoints";
	else if(executionMode==SpvExecutionModeInputLines)				return "SpvExecutionModeInputLines";
	else if(executionMode==SpvExecutionModeInputLinesAdjacency)		return "SpvExecutionModeInputLinesAdjacency";
	else if(executionMode==SpvExecutionModeTriangles)				return "SpvExecutionModeTriangles";
	else if(executionMode==SpvExecutionModeInputTrianglesAdjacency)	return "SpvExecutionModeInputTrianglesAdjacency";
	else if(executionMode==SpvExecutionModeQuads)					return "SpvExecutionModeQuads";
	else if(executionMode==SpvExecutionModeIsolines)				return "SpvExecutionModeIsolines";
	else if(executionMode==SpvExecutionModeOutputVertices)			return "SpvExecutionModeOutputVertices";
	else if(executionMode==SpvExecutionModeOutputPoints)			return "SpvExecutionModeOutputPoints";
	else if(executionMode==SpvExecutionModeOutputLineStrip)			return "SpvExecutionModeOutputLineStrip";
	else if(executionMode==SpvExecutionModeOutputTriangleStrip)		return "SpvExecutionModeOutputTriangleStrip";
	else if(executionMode==SpvExecutionModeVecTypeHint)				return "SpvExecutionModeVecTypeHint";
	else if(executionMode==SpvExecutionModeContractionOff)			return "SpvExecutionModeContractionOff";
	else if(executionMode==SpvExecutionModeInitializer)				return "SpvExecutionModeInitializer";
	else if(executionMode==SpvExecutionModeFinalizer)				return "SpvExecutionModeFinalizer";
	else if(executionMode==SpvExecutionModeSubgroupSize)			return "SpvExecutionModeSubgroupSize";
	else if(executionMode==SpvExecutionModeSubgroupsPerWorkgroup)	return "SpvExecutionModeSubgroupsPerWorkgroup";
	else if(executionMode==SpvExecutionModeSubgroupsPerWorkgroupId)	return "SpvExecutionModeSubgroupsPerWorkgroupId";
	else if(executionMode==SpvExecutionModeLocalSizeId)				return "SpvExecutionModeLocalSizeId";
	else if(executionMode==SpvExecutionModeLocalSizeHintId)			return "SpvExecutionModeLocalSizeHintId";
	else															return "SpvExecutionModeUnknown";
}

const char *printSpvStorageClass(SpvStorageClass storageClass)
{
	if(storageClass==SpvStorageClassUniformConstant)		return "SpvStorageClassUniformConstant";
	else if(storageClass==SpvStorageClassInput)				return "SpvStorageClassInput";
	else if(storageClass==SpvStorageClassUniform)			return "SpvStorageClassUniform";
	else if(storageClass==SpvStorageClassOutput)			return "SpvStorageClassOutput";
	else if(storageClass==SpvStorageClassWorkgroup)			return "SpvStorageClassWorkgroup";
	else if(storageClass==SpvStorageClassCrossWorkgroup)	return "SpvStorageClassCrossWorkgroup";
	else if(storageClass==SpvStorageClassPrivate)			return "SpvStorageClassPrivate";
	else if(storageClass==SpvStorageClassFunction)			return "SpvStorageClassFunction";
	else if(storageClass==SpvStorageClassGeneric)			return "SpvStorageClassGeneric";
	else if(storageClass==SpvStorageClassPushConstant)		return "SpvStorageClassPushConstant";
	else if(storageClass==SpvStorageClassAtomicCounter)		return "SpvStorageClassAtomicCounter";
	else if(storageClass==SpvStorageClassImage)				return "SpvStorageClassImage";
	else if(storageClass==SpvStorageClassStorageBuffer)		return "SpvStorageClassStorageBuffer";
	else													return "SpvStorageClassUnknown";
}

#define SPV_MAX_MEMBERS 32

// TODO:
//	Need to figure out how to make less messy, proably unions/structs?
//	Lots of types and such share variables, should be doable.
typedef struct
{
	uint32_t opcode;
	uint32_t typeId;
	uint32_t storageClass;

	struct
	{
		uint32_t typeID;
		uint32_t offset;
		uint32_t arrayStride;
		uint32_t matrixStride;
	} member[SPV_MAX_MEMBERS];
	uint32_t numMembers;

	uint32_t arrayStride;
	uint32_t matrixStride;
	uint32_t location;
	uint32_t binding;
	uint32_t descriptorSet;
	uint32_t constant;
} SpvID_t;

typedef struct
{
	uint32_t magicNumber;
	uint32_t version;
	uint32_t generator;
	uint32_t bound;
	uint32_t schema;
} SpvHeader_t;

void printSpvHeader(const SpvHeader_t *header)
{
	DBGPRINTF(DEBUG_INFO, "SPIR-V Header:\n");
	DBGPRINTF(DEBUG_INFO, "\tMagic Number: 0x%x\n", header->magicNumber);
	DBGPRINTF(DEBUG_INFO, "\tVersion: %d.%d\n", SPV_SPIRV_VERSION_MAJOR_PART(header->version), SPV_SPIRV_VERSION_MINOR_PART(header->version));
	DBGPRINTF(DEBUG_INFO, "\tGenerator: 0x%x\n", header->generator);
	DBGPRINTF(DEBUG_INFO, "\tBound: %d\n", header->bound);
	DBGPRINTF(DEBUG_INFO, "\tSchema: %d\n", header->schema);
}

bool parseSpv(const uint32_t *opCodes, const uint32_t codeSize)
{
	if(opCodes==NULL)
		return false;
	
	uint32_t offset=5;
	const uint32_t codeEnd=codeSize/sizeof(uint32_t);

	if(opCodes[0]!=SpvMagicNumber)
	{
		DBGPRINTF(DEBUG_INFO, "SPIR-V magic number not found.\n");
		return false;
	}

	const SpvHeader_t header={ opCodes[0], opCodes[1], opCodes[2], opCodes[3], opCodes[4] };

	printSpvHeader(&header);

	SpvID_t *IDs=(SpvID_t *)Zone_Malloc(zone, sizeof(SpvID_t)*header.bound);

	if(IDs==NULL)
		return false;

	memset(IDs, 0, sizeof(SpvID_t)*header.bound);

	while(offset<codeEnd)
	{
		uint16_t wordCount=(uint16_t)(opCodes[offset]>>16);
		uint16_t opCode=(uint16_t)(opCodes[offset]&0xFFFFu);

		switch(opCode)
		{
			case SpvOpEntryPoint:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpEntryPoint word count too small.\n");
					return false;
				}

				uint32_t executionModel=opCodes[offset+1];
				uint32_t entryPointID=opCodes[offset+2];
				const char *entryPointName=(const char *)&opCodes[offset+3];

				DBGPRINTF(DEBUG_INFO, "SpvOpEntryPoint:\n\tExecution model: %s\n\tEntry point ID: %u\n\tEntry point name: %s\n", printSpvExecutionModel(executionModel), entryPointID, entryPointName);
				break;
			}

			case SpvOpExecutionMode:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpExecutionMode word count too small.\n");
					return false;
				}

				uint32_t entryPointID=opCodes[offset+1];
				uint32_t executionMode=opCodes[offset+2];

				DBGPRINTF(DEBUG_INFO, "SpvOpExecutionMode:\n\tEntry point ID: %u\n\tExecution mode: %s\n", entryPointID, printSpvExecutionMode(executionMode));

				switch(executionMode)
				{
					case SpvExecutionModeLocalSize:
						// Local sizes for compute shaders:
						// localSizeX=opCodes[offset+3];
						// localSizeY=opCodes[offset+4];
						// localSizeZ=opCodes[offset+5];
						break;
				}
				break;
			}

			case SpvOpDecorate:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpDecorate word count too small.\n");
					return false;
				}

				uint32_t targetID=opCodes[offset+1];
				uint32_t decoration=opCodes[offset+2];

				switch(decoration)
				{
					case SpvDecorationDescriptorSet:
						IDs[targetID].descriptorSet=opCodes[offset+3];
						break;

					case SpvDecorationBinding:
						IDs[targetID].binding=opCodes[offset+3];
						break;

					case SpvDecorationLocation:
						IDs[targetID].location=opCodes[offset+3];
						break;
				}

				break;
			}

			case SpvOpMemberDecorate:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpMemberDecorate word count too small.\n");
					return false;
				}

				uint32_t targetID=opCodes[offset+1];
				uint32_t member=opCodes[offset+2];
				uint32_t decoration=opCodes[offset+3];

				switch(decoration)
				{
					case SpvDecorationDescriptorSet:
						IDs[targetID].descriptorSet=opCodes[offset+4];
						break;

					case SpvDecorationBinding:
						IDs[targetID].binding=opCodes[offset+4];
						break;

					case SpvDecorationLocation:
						IDs[targetID].location=opCodes[offset+4];
						break;

					case SpvDecorationOffset:
						IDs[targetID].member[member].offset=opCodes[offset+4];
						break;

					case SpvDecorationArrayStride:
						IDs[targetID].member[member].arrayStride=opCodes[offset+4];
						break;

					case SpvDecorationMatrixStride:
						IDs[targetID].member[member].matrixStride=opCodes[offset+4];
						break;
				}

				break;
			}

			case SpvOpTypeStruct:
			{
				if(wordCount<2)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpTypeStruct word count too small.\n");
					return false;
				}
				uint32_t targetID=opCodes[offset+1];
				IDs[targetID].numMembers=min(SPV_MAX_MEMBERS, wordCount-2);
				IDs[targetID].opcode=opCode;

				for(uint32_t i=0;i<IDs[targetID].numMembers;i++)
					IDs[targetID].member[i].typeID=opCodes[offset+i+2];
				break;
			}

			case SpvOpTypeImage:
			case SpvOpTypeSampler:
			case SpvOpTypeSampledImage:
			{
				uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opcode=opCode;
				break;
			}

			case SpvOpTypePointer:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpTypePointer invalid word count.\n");
					return false;
				}

				uint32_t targetID=opCodes[offset+1];

				IDs[targetID].opcode=opCode;
				IDs[targetID].storageClass=opCodes[offset+2];
				IDs[targetID].typeId=opCodes[offset+3];

				break;
			}

			case SpvOpTypeVoid:
			case SpvOpTypeBool:
			case SpvOpTypeInt:
			case SpvOpTypeFloat:
			case SpvOpTypeVector:
			case SpvOpTypeMatrix:
			{
				uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opcode=opCode;
				break;
			}

			case SpvOpConstant:
			{
				uint32_t targetID=opCodes[offset+2];

				IDs[targetID].opcode=opCode;
				IDs[targetID].typeId=opCodes[offset+1];
				IDs[targetID].constant=opCodes[offset+3];

				break;
			}

			case SpvOpVariable:
			{
				uint32_t targetID=opCodes[offset+2];

				IDs[targetID].opcode=opCode;
				IDs[targetID].typeId=opCodes[offset+1];
				IDs[targetID].storageClass=opCodes[offset+3];

				break;
			}
		}

		offset+=wordCount;
	}

	for(uint32_t i=0;i<header.bound;i++)
	{
		SpvID_t *id=&IDs[i];
		uint32_t typeKind=IDs[IDs[id->typeId].typeId].opcode;

		if(typeKind!=SpvOpNop&&!(id->storageClass==SpvStorageClassInput||id->storageClass==SpvStorageClassOutput))
		{
			DBGPRINTF(DEBUG_INFO, "ID: %d set: %d binding: %d %s %s\n", i, id->descriptorSet, id->binding, printSpvOp(typeKind), printSpvStorageClass(id->storageClass));

			if(typeKind==SpvOpTypeStruct)
			{
				SpvID_t *typeID=&IDs[IDs[id->typeId].typeId];

				for(uint32_t j=0;j<typeID->numMembers;j++)
				{
					DBGPRINTF(DEBUG_INFO, "\tmember: %d type: %s offset: %d\n", j, printSpvOp(IDs[typeID->member[j].typeID].opcode), typeID->member[j].offset);
				}
			}
		}
	}

	Zone_Free(zone, IDs);

	return true;
}
