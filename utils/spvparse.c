#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include "../math/math.h"
#include "../system/system.h"

// Local copy from the Vulkan SDK, because some SDK deployments are dumb and don't include this or are in differing locations.
#include "spirv.h"

#define SPV_SPIRV_VERSION_MAJOR_PART(WORD) (((uint32_t)(WORD)>>16)&0xFF)
#define SPV_SPIRV_VERSION_MINOR_PART(WORD) (((uint32_t)(WORD)>>8)&0xFF)

#define SPV_MAX_MEMBERS 32
#define SPV_MAX_MEMBER_DECORATIONS 8
#define SPV_MAX_LITERALS 32

typedef struct
{
	uint32_t targetID;
	struct
	{
		SpvDecoration decoration;
		uint32_t decorationValue;
	} decorations[SPV_MAX_MEMBER_DECORATIONS];
	uint32_t decorationCount;
} SpvDecorate_t;

typedef struct
{
	uint32_t structTypeID;
	struct
	{
		struct
		{
			SpvDecoration decoration;
			uint32_t decorationValue;
		} decorations[SPV_MAX_MEMBER_DECORATIONS];
		uint32_t decorationCount;
	} member[SPV_MAX_MEMBERS];
} SpvMemberDecorate_t;

typedef struct
{
	uint32_t resultID;
	union
	{
		struct
		{
			uint32_t intWidth;
			bool intSignedness;
		};
		uint32_t floatWidth;
		struct
		{
			uint32_t vectorComponentTypeID;
			uint32_t vectorComponentCount;
		};
		struct
		{
			uint32_t matrixColumnTypeID;
			uint32_t matrixColumnCount;
		};
		struct
		{
			uint32_t imageSampledTypeID;
			SpvDim imageDim;
			uint8_t imageDepth;
			bool imageArrayed;
			bool imageMultisample;
			uint8_t imageSampled;
			SpvImageFormat imageFormat;
			SpvAccessQualifier imageAccessQualifier;
		};
		uint32_t sampledImageTypeID;
		struct
		{
			uint32_t arrayElementTypeID;
			uint32_t arrayLengthID;
		};
		struct
		{
			uint32_t runtimeArrayElementTypeID;
		};
		struct
		{
			uint32_t structMemberTypeIDs[SPV_MAX_MEMBERS];
			uint32_t structMemberCount;
		};
		const char *opaqueTypeName;
		struct
		{
			SpvStorageClass pointerStorageClass;
			uint32_t pointerTypeID;
		};
		SpvAccessQualifier pipeAccessQualifier;
	};
} SpvType_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	bool value;
} SpvConstantBool_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	union
	{
		double fLiteral;
		int64_t iLiteral;
	} literals[SPV_MAX_LITERALS];
	uint32_t literalCount;
} SpvConstant_t;

typedef struct
{
	uint32_t resultTypeID;
	uint32_t resultID;
	SpvStorageClass storageClass;
	uint32_t initializerID;
} SpvVariable_t;

typedef struct
{
	SpvOp opCode;

	SpvDecorate_t decoration;
	SpvMemberDecorate_t memberDecoration;

	union
	{
		SpvType_t type;
		SpvConstantBool_t constantBool;
		SpvConstant_t constant;
		SpvVariable_t variable;
	};
} SpvID_t;

typedef struct
{
	uint32_t magicNumber;
	uint32_t version;
	uint32_t generator;
	uint32_t bound;
	uint32_t schema;
} SpvHeader_t;

static inline const char *printSpvOp(const SpvOp op)
{
	switch(op)
	{
		case SpvOpNop:										return "SpvOpNop";
		case SpvOpUndef:									return "SpvOpUndef";
		case SpvOpSourceContinued:							return "SpvOpSourceContinued";
		case SpvOpSource:									return "SpvOpSource";
		case SpvOpSourceExtension:							return "SpvOpSourceExtension";
		case SpvOpName:										return "SpvOpName";
		case SpvOpMemberName:								return "SpvOpMemberName";
		case SpvOpString:									return "SpvOpString";
		case SpvOpLine:										return "SpvOpLine";
		case SpvOpExtension:								return "SpvOpExtension";
		case SpvOpExtInstImport:							return "SpvOpExtInstImport";
		case SpvOpExtInst:									return "SpvOpExtInst";
		case SpvOpMemoryModel:								return "SpvOpMemoryModel";
		case SpvOpEntryPoint:								return "SpvOpEntryPoint";
		case SpvOpExecutionMode:							return "SpvOpExecutionMode";
		case SpvOpCapability:								return "SpvOpCapability";
		case SpvOpTypeVoid:									return "SpvOpTypeVoid";
		case SpvOpTypeBool:									return "SpvOpTypeBool";
		case SpvOpTypeInt:									return "SpvOpTypeInt";
		case SpvOpTypeFloat:								return "SpvOpTypeFloat";
		case SpvOpTypeVector:								return "SpvOpTypeVector";
		case SpvOpTypeMatrix:								return "SpvOpTypeMatrix";
		case SpvOpTypeImage:								return "SpvOpTypeImage";
		case SpvOpTypeSampler:								return "SpvOpTypeSampler";
		case SpvOpTypeSampledImage:							return "SpvOpTypeSampledImage";
		case SpvOpTypeArray:								return "SpvOpTypeArray";
		case SpvOpTypeRuntimeArray:							return "SpvOpTypeRuntimeArray";
		case SpvOpTypeStruct:								return "SpvOpTypeStruct";
		case SpvOpTypeOpaque:								return "SpvOpTypeOpaque";
		case SpvOpTypePointer:								return "SpvOpTypePointer";
		case SpvOpTypeFunction:								return "SpvOpTypeFunction";
		case SpvOpTypeEvent:								return "SpvOpTypeEvent";
		case SpvOpTypeDeviceEvent:							return "SpvOpTypeDeviceEvent";
		case SpvOpTypeReserveId:							return "SpvOpTypeReserveId";
		case SpvOpTypeQueue:								return "SpvOpTypeQueue";
		case SpvOpTypePipe:									return "SpvOpTypePipe";
		case SpvOpTypeForwardPointer:						return "SpvOpTypeForwardPointer";
		case SpvOpConstantTrue:								return "SpvOpConstantTrue";
		case SpvOpConstantFalse:							return "SpvOpConstantFalse";
		case SpvOpConstant:									return "SpvOpConstant";
		case SpvOpConstantComposite:						return "SpvOpConstantComposite";
		case SpvOpConstantSampler:							return "SpvOpConstantSampler";
		case SpvOpConstantNull:								return "SpvOpConstantNull";
		case SpvOpSpecConstantTrue:							return "SpvOpSpecConstantTrue";
		case SpvOpSpecConstantFalse:						return "SpvOpSpecConstantFalse";
		case SpvOpSpecConstant:								return "SpvOpSpecConstant";
		case SpvOpSpecConstantComposite:					return "SpvOpSpecConstantComposite";
		case SpvOpSpecConstantOp:							return "SpvOpSpecConstantOp";
		case SpvOpFunction:									return "SpvOpFunction";
		case SpvOpFunctionParameter:						return "SpvOpFunctionParameter";
		case SpvOpFunctionEnd:								return "SpvOpFunctionEnd";
		case SpvOpFunctionCall:								return "SpvOpFunctionCall";
		case SpvOpVariable:									return "SpvOpVariable";
		case SpvOpImageTexelPointer:						return "SpvOpImageTexelPointer";
		case SpvOpLoad:										return "SpvOpLoad";
		case SpvOpStore:									return "SpvOpStore";
		case SpvOpCopyMemory:								return "SpvOpCopyMemory";
		case SpvOpCopyMemorySized:							return "SpvOpCopyMemorySized";
		case SpvOpAccessChain:								return "SpvOpAccessChain";
		case SpvOpInBoundsAccessChain:						return "SpvOpInBoundsAccessChain";
		case SpvOpPtrAccessChain:							return "SpvOpPtrAccessChain";
		case SpvOpArrayLength:								return "SpvOpArrayLength";
		case SpvOpGenericPtrMemSemantics:					return "SpvOpGenericPtrMemSemantics";
		case SpvOpInBoundsPtrAccessChain:					return "SpvOpInBoundsPtrAccessChain";
		case SpvOpDecorate:									return "SpvOpDecorate";
		case SpvOpMemberDecorate:							return "SpvOpMemberDecorate";
		case SpvOpDecorationGroup:							return "SpvOpDecorationGroup";
		case SpvOpGroupDecorate:							return "SpvOpGroupDecorate";
		case SpvOpGroupMemberDecorate:						return "SpvOpGroupMemberDecorate";
		case SpvOpVectorExtractDynamic:						return "SpvOpVectorExtractDynamic";
		case SpvOpVectorInsertDynamic:						return "SpvOpVectorInsertDynamic";
		case SpvOpVectorShuffle:							return "SpvOpVectorShuffle";
		case SpvOpCompositeConstruct:						return "SpvOpCompositeConstruct";
		case SpvOpCompositeExtract:							return "SpvOpCompositeExtract";
		case SpvOpCompositeInsert:							return "SpvOpCompositeInsert";
		case SpvOpCopyObject:								return "SpvOpCopyObject";
		case SpvOpTranspose:								return "SpvOpTranspose";
		case SpvOpSampledImage:								return "SpvOpSampledImage";
		case SpvOpImageSampleImplicitLod:					return "SpvOpImageSampleImplicitLod";
		case SpvOpImageSampleExplicitLod:					return "SpvOpImageSampleExplicitLod";
		case SpvOpImageSampleDrefImplicitLod:				return "SpvOpImageSampleDrefImplicitLod";
		case SpvOpImageSampleDrefExplicitLod:				return "SpvOpImageSampleDrefExplicitLod";
		case SpvOpImageSampleProjImplicitLod:				return "SpvOpImageSampleProjImplicitLod";
		case SpvOpImageSampleProjExplicitLod:				return "SpvOpImageSampleProjExplicitLod";
		case SpvOpImageSampleProjDrefImplicitLod:			return "SpvOpImageSampleProjDrefImplicitLod";
		case SpvOpImageSampleProjDrefExplicitLod:			return "SpvOpImageSampleProjDrefExplicitLod";
		case SpvOpImageFetch:								return "SpvOpImageFetch";
		case SpvOpImageGather:								return "SpvOpImageGather";
		case SpvOpImageDrefGather:							return "SpvOpImageDrefGather";
		case SpvOpImageRead:								return "SpvOpImageRead";
		case SpvOpImageWrite:								return "SpvOpImageWrite";
		case SpvOpImage:									return "SpvOpImage";
		case SpvOpImageQueryFormat:							return "SpvOpImageQueryFormat";
		case SpvOpImageQueryOrder:							return "SpvOpImageQueryOrder";
		case SpvOpImageQuerySizeLod:						return "SpvOpImageQuerySizeLod";
		case SpvOpImageQuerySize:							return "SpvOpImageQuerySize";
		case SpvOpImageQueryLod:							return "SpvOpImageQueryLod";
		case SpvOpImageQueryLevels:							return "SpvOpImageQueryLevels";
		case SpvOpImageQuerySamples:						return "SpvOpImageQuerySamples";
		case SpvOpConvertFToU:								return "SpvOpConvertFToU";
		case SpvOpConvertFToS:								return "SpvOpConvertFToS";
		case SpvOpConvertSToF:								return "SpvOpConvertSToF";
		case SpvOpConvertUToF:								return "SpvOpConvertUToF";
		case SpvOpUConvert:									return "SpvOpUConvert";
		case SpvOpSConvert:									return "SpvOpSConvert";
		case SpvOpFConvert:									return "SpvOpFConvert";
		case SpvOpQuantizeToF16:							return "SpvOpQuantizeToF16";
		case SpvOpConvertPtrToU:							return "SpvOpConvertPtrToU";
		case SpvOpSatConvertSToU:							return "SpvOpSatConvertSToU";
		case SpvOpSatConvertUToS:							return "SpvOpSatConvertUToS";
		case SpvOpConvertUToPtr:							return "SpvOpConvertUToPtr";
		case SpvOpPtrCastToGeneric:							return "SpvOpPtrCastToGeneric";
		case SpvOpGenericCastToPtr:							return "SpvOpGenericCastToPtr";
		case SpvOpGenericCastToPtrExplicit:					return "SpvOpGenericCastToPtrExplicit";
		case SpvOpBitcast:									return "SpvOpBitcast";
		case SpvOpSNegate:									return "SpvOpSNegate";
		case SpvOpFNegate:									return "SpvOpFNegate";
		case SpvOpIAdd:										return "SpvOpIAdd";
		case SpvOpFAdd:										return "SpvOpFAdd";
		case SpvOpISub:										return "SpvOpISub";
		case SpvOpFSub:										return "SpvOpFSub";
		case SpvOpIMul:										return "SpvOpIMul";
		case SpvOpFMul:										return "SpvOpFMul";
		case SpvOpUDiv:										return "SpvOpUDiv";
		case SpvOpSDiv:										return "SpvOpSDiv";
		case SpvOpFDiv:										return "SpvOpFDiv";
		case SpvOpUMod:										return "SpvOpUMod";
		case SpvOpSRem:										return "SpvOpSRem";
		case SpvOpSMod:										return "SpvOpSMod";
		case SpvOpFRem:										return "SpvOpFRem";
		case SpvOpFMod:										return "SpvOpFMod";
		case SpvOpVectorTimesScalar:						return "SpvOpVectorTimesScalar";
		case SpvOpMatrixTimesScalar:						return "SpvOpMatrixTimesScalar";
		case SpvOpVectorTimesMatrix:						return "SpvOpVectorTimesMatrix";
		case SpvOpMatrixTimesVector:						return "SpvOpMatrixTimesVector";
		case SpvOpMatrixTimesMatrix:						return "SpvOpMatrixTimesMatrix";
		case SpvOpOuterProduct:								return "SpvOpOuterProduct";
		case SpvOpDot:										return "SpvOpDot";
		case SpvOpIAddCarry:								return "SpvOpIAddCarry";
		case SpvOpISubBorrow:								return "SpvOpISubBorrow";
		case SpvOpUMulExtended:								return "SpvOpUMulExtended";
		case SpvOpSMulExtended:								return "SpvOpSMulExtended";
		case SpvOpAny:										return "SpvOpAny";
		case SpvOpAll:										return "SpvOpAll";
		case SpvOpIsNan:									return "SpvOpIsNan";
		case SpvOpIsInf:									return "SpvOpIsInf";
		case SpvOpIsFinite:									return "SpvOpIsFinite";
		case SpvOpIsNormal:									return "SpvOpIsNormal";
		case SpvOpSignBitSet:								return "SpvOpSignBitSet";
		case SpvOpLessOrGreater:							return "SpvOpLessOrGreater";
		case SpvOpOrdered:									return "SpvOpOrdered";
		case SpvOpUnordered:								return "SpvOpUnordered";
		case SpvOpLogicalEqual:								return "SpvOpLogicalEqual";
		case SpvOpLogicalNotEqual:							return "SpvOpLogicalNotEqual";
		case SpvOpLogicalOr:								return "SpvOpLogicalOr";
		case SpvOpLogicalAnd:								return "SpvOpLogicalAnd";
		case SpvOpLogicalNot:								return "SpvOpLogicalNot";
		case SpvOpSelect:									return "SpvOpSelect";
		case SpvOpIEqual:									return "SpvOpIEqual";
		case SpvOpINotEqual:								return "SpvOpINotEqual";
		case SpvOpUGreaterThan:								return "SpvOpUGreaterThan";
		case SpvOpSGreaterThan:								return "SpvOpSGreaterThan";
		case SpvOpUGreaterThanEqual:						return "SpvOpUGreaterThanEqual";
		case SpvOpSGreaterThanEqual:						return "SpvOpSGreaterThanEqual";
		case SpvOpULessThan:								return "SpvOpULessThan";
		case SpvOpSLessThan:								return "SpvOpSLessThan";
		case SpvOpULessThanEqual:							return "SpvOpULessThanEqual";
		case SpvOpSLessThanEqual:							return "SpvOpSLessThanEqual";
		case SpvOpFOrdEqual:								return "SpvOpFOrdEqual";
		case SpvOpFUnordEqual:								return "SpvOpFUnordEqual";
		case SpvOpFOrdNotEqual:								return "SpvOpFOrdNotEqual";
		case SpvOpFUnordNotEqual:							return "SpvOpFUnordNotEqual";
		case SpvOpFOrdLessThan:								return "SpvOpFOrdLessThan";
		case SpvOpFUnordLessThan:							return "SpvOpFUnordLessThan";
		case SpvOpFOrdGreaterThan:							return "SpvOpFOrdGreaterThan";
		case SpvOpFUnordGreaterThan:						return "SpvOpFUnordGreaterThan";
		case SpvOpFOrdLessThanEqual:						return "SpvOpFOrdLessThanEqual";
		case SpvOpFUnordLessThanEqual:						return "SpvOpFUnordLessThanEqual";
		case SpvOpFOrdGreaterThanEqual:						return "SpvOpFOrdGreaterThanEqual";
		case SpvOpFUnordGreaterThanEqual:					return "SpvOpFUnordGreaterThanEqual";
		case SpvOpShiftRightLogical:						return "SpvOpShiftRightLogical";
		case SpvOpShiftRightArithmetic:						return "SpvOpShiftRightArithmetic";
		case SpvOpShiftLeftLogical:							return "SpvOpShiftLeftLogical";
		case SpvOpBitwiseOr:								return "SpvOpBitwiseOr";
		case SpvOpBitwiseXor:								return "SpvOpBitwiseXor";
		case SpvOpBitwiseAnd:								return "SpvOpBitwiseAnd";
		case SpvOpNot:										return "SpvOpNot";
		case SpvOpBitFieldInsert:							return "SpvOpBitFieldInsert";
		case SpvOpBitFieldSExtract:							return "SpvOpBitFieldSExtract";
		case SpvOpBitFieldUExtract:							return "SpvOpBitFieldUExtract";
		case SpvOpBitReverse:								return "SpvOpBitReverse";
		case SpvOpBitCount:									return "SpvOpBitCount";
		case SpvOpDPdx:										return "SpvOpDPdx";
		case SpvOpDPdy:										return "SpvOpDPdy";
		case SpvOpFwidth:									return "SpvOpFwidth";
		case SpvOpDPdxFine:									return "SpvOpDPdxFine";
		case SpvOpDPdyFine:									return "SpvOpDPdyFine";
		case SpvOpFwidthFine:								return "SpvOpFwidthFine";
		case SpvOpDPdxCoarse:								return "SpvOpDPdxCoarse";
		case SpvOpDPdyCoarse:								return "SpvOpDPdyCoarse";
		case SpvOpFwidthCoarse:								return "SpvOpFwidthCoarse";
		case SpvOpEmitVertex:								return "SpvOpEmitVertex";
		case SpvOpEndPrimitive:								return "SpvOpEndPrimitive";
		case SpvOpEmitStreamVertex:							return "SpvOpEmitStreamVertex";
		case SpvOpEndStreamPrimitive:						return "SpvOpEndStreamPrimitive";
		case SpvOpControlBarrier:							return "SpvOpControlBarrier";
		case SpvOpMemoryBarrier:							return "SpvOpMemoryBarrier";
		case SpvOpAtomicLoad:								return "SpvOpAtomicLoad";
		case SpvOpAtomicStore:								return "SpvOpAtomicStore";
		case SpvOpAtomicExchange:							return "SpvOpAtomicExchange";
		case SpvOpAtomicCompareExchange:					return "SpvOpAtomicCompareExchange";
		case SpvOpAtomicCompareExchangeWeak:				return "SpvOpAtomicCompareExchangeWeak";
		case SpvOpAtomicIIncrement:							return "SpvOpAtomicIIncrement";
		case SpvOpAtomicIDecrement:							return "SpvOpAtomicIDecrement";
		case SpvOpAtomicIAdd:								return "SpvOpAtomicIAdd";
		case SpvOpAtomicISub:								return "SpvOpAtomicISub";
		case SpvOpAtomicSMin:								return "SpvOpAtomicSMin";
		case SpvOpAtomicUMin:								return "SpvOpAtomicUMin";
		case SpvOpAtomicSMax:								return "SpvOpAtomicSMax";
		case SpvOpAtomicUMax:								return "SpvOpAtomicUMax";
		case SpvOpAtomicAnd:								return "SpvOpAtomicAnd";
		case SpvOpAtomicOr:									return "SpvOpAtomicOr";
		case SpvOpAtomicXor:								return "SpvOpAtomicXor";
		case SpvOpPhi:										return "SpvOpPhi";
		case SpvOpLoopMerge:								return "SpvOpLoopMerge";
		case SpvOpSelectionMerge:							return "SpvOpSelectionMerge";
		case SpvOpLabel:									return "SpvOpLabel";
		case SpvOpBranch:									return "SpvOpBranch";
		case SpvOpBranchConditional:						return "SpvOpBranchConditional";
		case SpvOpSwitch:									return "SpvOpSwitch";
		case SpvOpKill:										return "SpvOpKill";
		case SpvOpReturn:									return "SpvOpReturn";
		case SpvOpReturnValue:								return "SpvOpReturnValue";
		case SpvOpUnreachable:								return "SpvOpUnreachable";
		case SpvOpLifetimeStart:							return "SpvOpLifetimeStart";
		case SpvOpLifetimeStop:								return "SpvOpLifetimeStop";
		case SpvOpGroupAsyncCopy:							return "SpvOpGroupAsyncCopy";
		case SpvOpGroupWaitEvents:							return "SpvOpGroupWaitEvents";
		case SpvOpGroupAll:									return "SpvOpGroupAll";
		case SpvOpGroupAny:									return "SpvOpGroupAny";
		case SpvOpGroupBroadcast:							return "SpvOpGroupBroadcast";
		case SpvOpGroupIAdd:								return "SpvOpGroupIAdd";
		case SpvOpGroupFAdd:								return "SpvOpGroupFAdd";
		case SpvOpGroupFMin:								return "SpvOpGroupFMin";
		case SpvOpGroupUMin:								return "SpvOpGroupUMin";
		case SpvOpGroupSMin:								return "SpvOpGroupSMin";
		case SpvOpGroupFMax:								return "SpvOpGroupFMax";
		case SpvOpGroupUMax:								return "SpvOpGroupUMax";
		case SpvOpGroupSMax:								return "SpvOpGroupSMax";
		case SpvOpReadPipe:									return "SpvOpReadPipe";
		case SpvOpWritePipe:								return "SpvOpWritePipe";
		case SpvOpReservedReadPipe:							return "SpvOpReservedReadPipe";
		case SpvOpReservedWritePipe:						return "SpvOpReservedWritePipe";
		case SpvOpReserveReadPipePackets:					return "SpvOpReserveReadPipePackets";
		case SpvOpReserveWritePipePackets:					return "SpvOpReserveWritePipePackets";
		case SpvOpCommitReadPipe:							return "SpvOpCommitReadPipe";
		case SpvOpCommitWritePipe:							return "SpvOpCommitWritePipe";
		case SpvOpIsValidReserveId:							return "SpvOpIsValidReserveId";
		case SpvOpGetNumPipePackets:						return "SpvOpGetNumPipePackets";
		case SpvOpGetMaxPipePackets:						return "SpvOpGetMaxPipePackets";
		case SpvOpGroupReserveReadPipePackets:				return "SpvOpGroupReserveReadPipePackets";
		case SpvOpGroupReserveWritePipePackets:				return "SpvOpGroupReserveWritePipePackets";
		case SpvOpGroupCommitReadPipe:						return "SpvOpGroupCommitReadPipe";
		case SpvOpGroupCommitWritePipe:						return "SpvOpGroupCommitWritePipe";
		case SpvOpEnqueueMarker:							return "SpvOpEnqueueMarker";
		case SpvOpEnqueueKernel:							return "SpvOpEnqueueKernel";
		case SpvOpGetKernelNDrangeSubGroupCount:			return "SpvOpGetKernelNDrangeSubGroupCount";
		case SpvOpGetKernelNDrangeMaxSubGroupSize:			return "SpvOpGetKernelNDrangeMaxSubGroupSize";
		case SpvOpGetKernelWorkGroupSize:					return "SpvOpGetKernelWorkGroupSize";
		case SpvOpGetKernelPreferredWorkGroupSizeMultiple:	return "SpvOpGetKernelPreferredWorkGroupSizeMultiple";
		case SpvOpRetainEvent:								return "SpvOpRetainEvent";
		case SpvOpReleaseEvent:								return "SpvOpReleaseEvent";
		case SpvOpCreateUserEvent:							return "SpvOpCreateUserEvent";
		case SpvOpIsValidEvent:								return "SpvOpIsValidEvent";
		case SpvOpSetUserEventStatus:						return "SpvOpSetUserEventStatus";
		case SpvOpCaptureEventProfilingInfo:				return "SpvOpCaptureEventProfilingInfo";
		case SpvOpGetDefaultQueue:							return "SpvOpGetDefaultQueue";
		case SpvOpBuildNDRange:								return "SpvOpBuildNDRange";
		case SpvOpImageSparseSampleImplicitLod:				return "SpvOpImageSparseSampleImplicitLod";
		case SpvOpImageSparseSampleExplicitLod:				return "SpvOpImageSparseSampleExplicitLod";
		case SpvOpImageSparseSampleDrefImplicitLod:			return "SpvOpImageSparseSampleDrefImplicitLod";
		case SpvOpImageSparseSampleDrefExplicitLod:			return "SpvOpImageSparseSampleDrefExplicitLod";
		case SpvOpImageSparseSampleProjImplicitLod:			return "SpvOpImageSparseSampleProjImplicitLod";
		case SpvOpImageSparseSampleProjExplicitLod:			return "SpvOpImageSparseSampleProjExplicitLod";
		case SpvOpImageSparseSampleProjDrefImplicitLod:		return "SpvOpImageSparseSampleProjDrefImplicitLod";
		case SpvOpImageSparseSampleProjDrefExplicitLod:		return "SpvOpImageSparseSampleProjDrefExplicitLod";
		case SpvOpImageSparseFetch:							return "SpvOpImageSparseFetch";
		case SpvOpImageSparseGather:						return "SpvOpImageSparseGather";
		case SpvOpImageSparseDrefGather:					return "SpvOpImageSparseDrefGather";
		case SpvOpImageSparseTexelsResident:				return "SpvOpImageSparseTexelsResident";
		case SpvOpNoLine:									return "SpvOpNoLine";
		case SpvOpAtomicFlagTestAndSet:						return "SpvOpAtomicFlagTestAndSet";
		case SpvOpAtomicFlagClear:							return "SpvOpAtomicFlagClear";
		case SpvOpImageSparseRead:							return "SpvOpImageSparseRead";
		case SpvOpSizeOf:									return "SpvOpSizeOf";
		case SpvOpTypePipeStorage:							return "SpvOpTypePipeStorage";
		case SpvOpConstantPipeStorage:						return "SpvOpConstantPipeStorage";
		case SpvOpCreatePipeFromPipeStorage:				return "SpvOpCreatePipeFromPipeStorage";
		case SpvOpGetKernelLocalSizeForSubgroupCount:		return "SpvOpGetKernelLocalSizeForSubgroupCount";
		case SpvOpGetKernelMaxNumSubgroups:					return "SpvOpGetKernelMaxNumSubgroups";
		case SpvOpTypeNamedBarrier:							return "SpvOpTypeNamedBarrier";
		case SpvOpNamedBarrierInitialize:					return "SpvOpNamedBarrierInitialize";
		case SpvOpMemoryNamedBarrier:						return "SpvOpMemoryNamedBarrier";
		case SpvOpModuleProcessed:							return "SpvOpModuleProcessed";
		case SpvOpExecutionModeId:							return "SpvOpExecutionModeId";
		case SpvOpDecorateId:								return "SpvOpDecorateId";
		case SpvOpGroupNonUniformElect:						return "SpvOpGroupNonUniformElect";
		case SpvOpGroupNonUniformAll:						return "SpvOpGroupNonUniformAll";
		case SpvOpGroupNonUniformAny:						return "SpvOpGroupNonUniformAny";
		case SpvOpGroupNonUniformAllEqual:					return "SpvOpGroupNonUniformAllEqual";
		case SpvOpGroupNonUniformBroadcast:					return "SpvOpGroupNonUniformBroadcast";
		case SpvOpGroupNonUniformBroadcastFirst:			return "SpvOpGroupNonUniformBroadcastFirst";
		case SpvOpGroupNonUniformBallot:					return "SpvOpGroupNonUniformBallot";
		case SpvOpGroupNonUniformInverseBallot:				return "SpvOpGroupNonUniformInverseBallot";
		case SpvOpGroupNonUniformBallotBitExtract:			return "SpvOpGroupNonUniformBallotBitExtract";
		case SpvOpGroupNonUniformBallotBitCount:			return "SpvOpGroupNonUniformBallotBitCount";
		case SpvOpGroupNonUniformBallotFindLSB:				return "SpvOpGroupNonUniformBallotFindLSB";
		case SpvOpGroupNonUniformBallotFindMSB:				return "SpvOpGroupNonUniformBallotFindMSB";
		case SpvOpGroupNonUniformShuffle:					return "SpvOpGroupNonUniformShuffle";
		case SpvOpGroupNonUniformShuffleXor:				return "SpvOpGroupNonUniformShuffleXor";
		case SpvOpGroupNonUniformShuffleUp:					return "SpvOpGroupNonUniformShuffleUp";
		case SpvOpGroupNonUniformShuffleDown:				return "SpvOpGroupNonUniformShuffleDown";
		case SpvOpGroupNonUniformIAdd:						return "SpvOpGroupNonUniformIAdd";
		case SpvOpGroupNonUniformFAdd:						return "SpvOpGroupNonUniformFAdd";
		case SpvOpGroupNonUniformIMul:						return "SpvOpGroupNonUniformIMul";
		case SpvOpGroupNonUniformFMul:						return "SpvOpGroupNonUniformFMul";
		case SpvOpGroupNonUniformSMin:						return "SpvOpGroupNonUniformSMin";
		case SpvOpGroupNonUniformUMin:						return "SpvOpGroupNonUniformUMin";
		case SpvOpGroupNonUniformFMin:						return "SpvOpGroupNonUniformFMin";
		case SpvOpGroupNonUniformSMax:						return "SpvOpGroupNonUniformSMax";
		case SpvOpGroupNonUniformUMax:						return "SpvOpGroupNonUniformUMax";
		case SpvOpGroupNonUniformFMax:						return "SpvOpGroupNonUniformFMax";
		case SpvOpGroupNonUniformBitwiseAnd:				return "SpvOpGroupNonUniformBitwiseAnd";
		case SpvOpGroupNonUniformBitwiseOr:					return "SpvOpGroupNonUniformBitwiseOr";
		case SpvOpGroupNonUniformBitwiseXor:				return "SpvOpGroupNonUniformBitwiseXor";
		case SpvOpGroupNonUniformLogicalAnd:				return "SpvOpGroupNonUniformLogicalAnd";
		case SpvOpGroupNonUniformLogicalOr:					return "SpvOpGroupNonUniformLogicalOr";
		case SpvOpGroupNonUniformLogicalXor:				return "SpvOpGroupNonUniformLogicalXor";
		case SpvOpGroupNonUniformQuadBroadcast:				return "SpvOpGroupNonUniformQuadBroadcast";
		case SpvOpGroupNonUniformQuadSwap:					return "SpvOpGroupNonUniformQuadSwap";
		case SpvOpCopyLogical:								return "SpvOpCopyLogical";
		case SpvOpPtrEqual:									return "SpvOpPtrEqual";
		case SpvOpPtrNotEqual:								return "SpvOpPtrNotEqual";
		case SpvOpPtrDiff:									return "SpvOpPtrDiff";
		default:											return "unknown";
	}
}

static inline const char *printSpvDecoration(const SpvDecoration decoration)
{
	switch(decoration)
	{
		case SpvDecorationRelaxedPrecision:		return "SpvDecorationRelaxedPrecision";
		case SpvDecorationSpecId:				return "SpvDecorationSpecId";
		case SpvDecorationBlock:				return "SpvDecorationBlock";
		case SpvDecorationBufferBlock:			return "SpvDecorationBufferBlock";
		case SpvDecorationRowMajor:				return "SpvDecorationRowMajor";
		case SpvDecorationColMajor:				return "SpvDecorationColMajor";
		case SpvDecorationArrayStride:			return "SpvDecorationArrayStride";
		case SpvDecorationMatrixStride:			return "SpvDecorationMatrixStride";
		case SpvDecorationGLSLShared:			return "SpvDecorationGLSLShared";
		case SpvDecorationGLSLPacked:			return "SpvDecorationGLSLPacked";
		case SpvDecorationCPacked:				return "SpvDecorationCPacked";
		case SpvDecorationBuiltIn:				return "SpvDecorationBuiltIn";
		case SpvDecorationNoPerspective:		return "SpvDecorationNoPerspective";
		case SpvDecorationFlat:					return "SpvDecorationFlat";
		case SpvDecorationPatch:				return "SpvDecorationPatch";
		case SpvDecorationCentroid:				return "SpvDecorationCentroid";
		case SpvDecorationSample:				return "SpvDecorationSample";
		case SpvDecorationInvariant:			return "SpvDecorationInvariant";
		case SpvDecorationRestrict:				return "SpvDecorationRestrict";
		case SpvDecorationAliased:				return "SpvDecorationAliased";
		case SpvDecorationVolatile:				return "SpvDecorationVolatile";
		case SpvDecorationConstant:				return "SpvDecorationConstant";
		case SpvDecorationCoherent:				return "SpvDecorationCoherent";
		case SpvDecorationNonWritable:			return "SpvDecorationNonWritable";
		case SpvDecorationNonReadable:			return "SpvDecorationNonReadable";
		case SpvDecorationUniform:				return "SpvDecorationUniform";
		case SpvDecorationUniformId:			return "SpvDecorationUniformId";
		case SpvDecorationSaturatedConversion:	return "SpvDecorationSaturatedConversion";
		case SpvDecorationStream:				return "SpvDecorationStream";
		case SpvDecorationLocation:				return "SpvDecorationLocation";
		case SpvDecorationComponent:			return "SpvDecorationComponent";
		case SpvDecorationIndex:				return "SpvDecorationIndex";
		case SpvDecorationBinding:				return "SpvDecorationBinding";
		case SpvDecorationDescriptorSet:		return "SpvDecorationDescriptorSet";
		case SpvDecorationOffset:				return "SpvDecorationOffset";
		case SpvDecorationXfbBuffer:			return "SpvDecorationXfbBuffer";
		case SpvDecorationXfbStride:			return "SpvDecorationXfbStride";
		case SpvDecorationFuncParamAttr:		return "SpvDecorationFuncParamAttr";
		case SpvDecorationFPRoundingMode:		return "SpvDecorationFPRoundingMode";
		case SpvDecorationFPFastMathMode:		return "SpvDecorationFPFastMathMode";
		case SpvDecorationLinkageAttributes:	return "SpvDecorationLinkageAttributes";
		case SpvDecorationNoContraction:		return "SpvDecorationNoContraction";
		case SpvDecorationInputAttachmentIndex:	return "SpvDecorationInputAttachmentIndex";
		case SpvDecorationAlignment:			return "SpvDecorationAlignment";
		case SpvDecorationMaxByteOffset:		return "SpvDecorationMaxByteOffset";
		case SpvDecorationAlignmentId:			return "SpvDecorationAlignmentId";
		case SpvDecorationMaxByteOffsetId:		return "SpvDecorationMaxByteOffsetId";
		default:								return "SpvDecorationUnknown";
	}
}

static inline const char *printSpvExecutionModel(const SpvExecutionModel executionModel)
{
	switch(executionModel)
	{
		case SpvExecutionModelVertex:					return "SpvExecutionModelVertex";
		case SpvExecutionModelTessellationControl:		return "SpvExecutionModelTessellationControl";
		case SpvExecutionModelTessellationEvaluation:	return "SpvExecutionModelTessellationEvaluation";
		case SpvExecutionModelGeometry:					return "SpvExecutionModelGeometry";
		case SpvExecutionModelFragment:					return "SpvExecutionModelFragment";
		case SpvExecutionModelGLCompute:				return "SpvExecutionModelGLCompute";
		case SpvExecutionModelKernel:					return "SpvExecutionModelKernel";
		default:										return "SpvExecutionModelUnknown";
	}
}

static inline const char *printSpvExecutionMode(const SpvExecutionMode executionMode)
{
	switch(executionMode)
	{
		case SpvExecutionModeInvocations:				return "SpvExecutionModeInvocations";
		case SpvExecutionModeSpacingEqual:				return "SpvExecutionModeSpacingEqual";
		case SpvExecutionModeSpacingFractionalEven:		return "SpvExecutionModeSpacingFractionalEven";
		case SpvExecutionModeSpacingFractionalOdd:		return "SpvExecutionModeSpacingFractionalOdd";
		case SpvExecutionModeVertexOrderCw:				return "SpvExecutionModeVertexOrderCw";
		case SpvExecutionModeVertexOrderCcw:			return "SpvExecutionModeVertexOrderCcw";
		case SpvExecutionModePixelCenterInteger:		return "SpvExecutionModePixelCenterInteger";
		case SpvExecutionModeOriginUpperLeft:			return "SpvExecutionModeOriginUpperLeft";
		case SpvExecutionModeOriginLowerLeft:			return "SpvExecutionModeOriginLowerLeft";
		case SpvExecutionModeEarlyFragmentTests:		return "SpvExecutionModeEarlyFragmentTests";
		case SpvExecutionModePointMode:					return "SpvExecutionModePointMode";
		case SpvExecutionModeXfb:						return "SpvExecutionModeXfb";
		case SpvExecutionModeDepthReplacing:			return "SpvExecutionModeDepthReplacing";
		case SpvExecutionModeDepthGreater:				return "SpvExecutionModeDepthGreater";
		case SpvExecutionModeDepthLess:					return "SpvExecutionModeDepthLess";
		case SpvExecutionModeDepthUnchanged:			return "SpvExecutionModeDepthUnchanged";
		case SpvExecutionModeLocalSize:					return "SpvExecutionModeLocalSize";
		case SpvExecutionModeLocalSizeHint:				return "SpvExecutionModeLocalSizeHint";
		case SpvExecutionModeInputPoints:				return "SpvExecutionModeInputPoints";
		case SpvExecutionModeInputLines:				return "SpvExecutionModeInputLines";
		case SpvExecutionModeInputLinesAdjacency:		return "SpvExecutionModeInputLinesAdjacency";
		case SpvExecutionModeTriangles:					return "SpvExecutionModeTriangles";
		case SpvExecutionModeInputTrianglesAdjacency:	return "SpvExecutionModeInputTrianglesAdjacency";
		case SpvExecutionModeQuads:						return "SpvExecutionModeQuads";
		case SpvExecutionModeIsolines:					return "SpvExecutionModeIsolines";
		case SpvExecutionModeOutputVertices:			return "SpvExecutionModeOutputVertices";
		case SpvExecutionModeOutputPoints:				return "SpvExecutionModeOutputPoints";
		case SpvExecutionModeOutputLineStrip:			return "SpvExecutionModeOutputLineStrip";
		case SpvExecutionModeOutputTriangleStrip:		return "SpvExecutionModeOutputTriangleStrip";
		case SpvExecutionModeVecTypeHint:				return "SpvExecutionModeVecTypeHint";
		case SpvExecutionModeContractionOff:			return "SpvExecutionModeContractionOff";
		case SpvExecutionModeInitializer:				return "SpvExecutionModeInitializer";
		case SpvExecutionModeFinalizer:					return "SpvExecutionModeFinalizer";
		case SpvExecutionModeSubgroupSize:				return "SpvExecutionModeSubgroupSize";
		case SpvExecutionModeSubgroupsPerWorkgroup:		return "SpvExecutionModeSubgroupsPerWorkgroup";
		case SpvExecutionModeSubgroupsPerWorkgroupId:	return "SpvExecutionModeSubgroupsPerWorkgroupId";
		case SpvExecutionModeLocalSizeId:				return "SpvExecutionModeLocalSizeId";
		case SpvExecutionModeLocalSizeHintId:			return "SpvExecutionModeLocalSizeHintId";
		default:										return "SpvExecutionModeUnknown";
	}
}

static inline const char *printSpvStorageClass(const SpvStorageClass storageClass)
{
	switch(storageClass)
	{
		case SpvStorageClassUniformConstant:	return "SpvStorageClassUniformConstant";
		case SpvStorageClassInput:				return "SpvStorageClassInput";
		case SpvStorageClassUniform:			return "SpvStorageClassUniform";
		case SpvStorageClassOutput:				return "SpvStorageClassOutput";
		case SpvStorageClassWorkgroup:			return "SpvStorageClassWorkgroup";
		case SpvStorageClassCrossWorkgroup:		return "SpvStorageClassCrossWorkgroup";
		case SpvStorageClassPrivate:			return "SpvStorageClassPrivate";
		case SpvStorageClassFunction:			return "SpvStorageClassFunction";
		case SpvStorageClassGeneric:			return "SpvStorageClassGeneric";
		case SpvStorageClassPushConstant:		return "SpvStorageClassPushConstant";
		case SpvStorageClassAtomicCounter:		return "SpvStorageClassAtomicCounter";
		case SpvStorageClassImage:				return "SpvStorageClassImage";
		case SpvStorageClassStorageBuffer:		return "SpvStorageClassStorageBuffer";
		default:								return "SpvStorageClassUnknown";
	}
}

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

	const SpvHeader_t *header=(const SpvHeader_t *)opCodes;

	printSpvHeader(header);

	SpvID_t *IDs=(SpvID_t *)Zone_Malloc(zone, sizeof(SpvID_t)*header->bound);

	if(IDs==NULL)
		return false;

	memset(IDs, 0, sizeof(SpvID_t)*header->bound);

	while(offset<codeEnd)
	{
		uint16_t wordCount=(uint16_t)(opCodes[offset]>>16);
		SpvOp opCode=(SpvOp)(opCodes[offset]&0xFFFFu);

		switch(opCode)
		{
			case SpvOpEntryPoint:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpEntryPoint word count too small.\n");
					return false;
				}

				SpvExecutionModel executionModel=(SpvExecutionModel)opCodes[offset+1];
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

				const uint32_t entryPointID=opCodes[offset+1];
				const SpvExecutionMode executionMode=(SpvExecutionMode)opCodes[offset+2];

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

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].decoration.targetID=targetID;
				const uint32_t decorationCount=IDs[targetID].decoration.decorationCount;

				if(decorationCount>SPV_MAX_MEMBER_DECORATIONS)
				{
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more decorations than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBER_DECORATIONS);
					break;
				}

				IDs[targetID].decoration.decorations[decorationCount].decoration=(SpvDecoration)opCodes[offset+2];

				if(wordCount>3)
					IDs[targetID].decoration.decorations[decorationCount].decorationValue=opCodes[offset+3];

				IDs[targetID].decoration.decorationCount=decorationCount+1;
				break;
			}

			case SpvOpMemberDecorate:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "SpvOpMemberDecorate word count too small.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				const uint32_t member=opCodes[offset+2];
				const uint32_t decorationCount=IDs[targetID].memberDecoration.member[member].decorationCount;

				if(decorationCount>SPV_MAX_MEMBER_DECORATIONS)
				{
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more decorations than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBER_DECORATIONS);
					break;
				}

				IDs[targetID].memberDecoration.structTypeID=targetID;
				IDs[targetID].memberDecoration.member[member].decorations[decorationCount].decoration=(SpvDecoration)opCodes[offset+3];

				if(wordCount>4)
					IDs[targetID].memberDecoration.member[member].decorations[decorationCount].decorationValue=(SpvDecoration)opCodes[offset+4];

				IDs[targetID].memberDecoration.member[member].decorationCount=decorationCount+1;
				break;
			}

			case SpvOpTypeVoid:
			case SpvOpTypeBool:
			case SpvOpTypeSampler:
			case SpvOpTypeOpaque:
			case SpvOpTypeFunction:
			case SpvOpTypeEvent:
			case SpvOpTypeDeviceEvent:
			case SpvOpTypeReserveId:
			case SpvOpTypeQueue:
			case SpvOpTypePipe:
			case SpvOpTypeForwardPointer:
			{
				if(wordCount<2)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				break;
			}

			case SpvOpTypeInt:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.intWidth=opCodes[offset+2];
				IDs[targetID].type.intSignedness=opCodes[offset+3];
				break;
			}

			case SpvOpTypeFloat:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.floatWidth=opCodes[offset+2];
				break;
			}

			case SpvOpTypeVector:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.vectorComponentTypeID=opCodes[offset+2];
				IDs[targetID].type.vectorComponentCount=opCodes[offset+3];
				break;
			}

			case SpvOpTypeMatrix:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.matrixColumnTypeID=opCodes[offset+2];
				IDs[targetID].type.matrixColumnCount=opCodes[offset+3];
				break;
			}

			case SpvOpTypeImage:
			{
				if(wordCount<9)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.imageSampledTypeID=opCodes[offset+2];
				IDs[targetID].type.imageDim=(SpvDim)opCodes[offset+3];
				IDs[targetID].type.imageDepth=opCodes[offset+4];
				IDs[targetID].type.imageMultisample=opCodes[offset+5];
				IDs[targetID].type.imageSampled=opCodes[offset+6];
				IDs[targetID].type.imageFormat=(SpvImageFormat)opCodes[offset+7];

				if(wordCount>9)
					IDs[targetID].type.imageAccessQualifier=(SpvAccessQualifier)opCodes[offset+8];
				break;
			}

			case SpvOpTypeSampledImage:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.sampledImageTypeID=opCodes[offset+2];
				break;
			}

			case SpvOpTypeArray:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.arrayElementTypeID=opCodes[offset+2];
				IDs[targetID].type.arrayLengthID=opCodes[offset+3];
				break;
			}

			case SpvOpTypeRuntimeArray:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.runtimeArrayElementTypeID=opCodes[offset+2];
				break;
			}


			case SpvOpTypeStruct:
			{
				if(wordCount<2)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				const uint32_t memberCount=wordCount-2;

				if(memberCount>SPV_MAX_MEMBERS)
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more members than code supports (%d), more will *not* be added.\n", targetID, SPV_MAX_MEMBERS);

				IDs[targetID].type.structMemberCount=min(SPV_MAX_MEMBERS, memberCount);

				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;

				for(uint32_t i=0;i<IDs[targetID].type.structMemberCount;i++)
					IDs[targetID].type.structMemberTypeIDs[i]=opCodes[offset+i+2];
				break;
			}

			case SpvOpTypePointer:
			{
				if(wordCount!=4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+1];
				IDs[targetID].opCode=opCode;
				IDs[targetID].type.resultID=targetID;
				IDs[targetID].type.pointerStorageClass=(SpvStorageClass)opCodes[offset+2];
				IDs[targetID].type.pointerTypeID=opCodes[offset+3];

				break;
			}

			case SpvOpConstantTrue:
			case SpvOpConstantFalse:
			{
				if(wordCount!=3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].constantBool.resultTypeID=opCodes[offset+1];
				IDs[targetID].constantBool.resultID=opCodes[offset+1];

				if(opCode==SpvOpConstantTrue)
					IDs[targetID].constantBool.value=true;
				else
					IDs[targetID].constantBool.value=false;

				break;
			}

			case SpvOpConstant:
			{
				if(wordCount<3)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].constant.resultTypeID=opCodes[offset+1];
				IDs[targetID].constant.resultID=targetID;

				const uint32_t literalCount=wordCount-3;

				if(literalCount>SPV_MAX_MEMBERS)
					DBGPRINTF(DEBUG_WARNING, "WARNING: targetID %d has more literals than code supports (%d), clamping to max supported.\n", targetID, SPV_MAX_LITERALS);

				IDs[targetID].constant.literalCount=min(SPV_MAX_LITERALS, literalCount);

				for(uint32_t i=0;i<IDs[targetID].constant.literalCount;i++)
					IDs[targetID].constant.literals[i].iLiteral=opCodes[offset+i+3];
				break;
			}

			case SpvOpVariable:
			{
				if(wordCount<4)
				{
					DBGPRINTF(DEBUG_ERROR, "Word count invalid.\n");
					return false;
				}

				const uint32_t targetID=opCodes[offset+2];
				IDs[targetID].opCode=opCode;
				IDs[targetID].variable.resultTypeID=opCodes[offset+1];
				IDs[targetID].variable.resultID=targetID;
				IDs[targetID].variable.storageClass=(SpvStorageClass)opCodes[offset+3];

				if(wordCount==5)
					IDs[targetID].variable.initializerID=opCodes[offset+4];
				break;
			}
		}

		offset+=wordCount;
	}

	for(uint32_t i=0;i<header->bound;i++)
	{
		SpvID_t *id=&IDs[i];

		if(id->opCode==SpvOpVariable)
		{
			// Don't care about input or output variables for info output
			if(!(id->variable.storageClass==SpvStorageClassInput||id->variable.storageClass==SpvStorageClassOutput))
			{
				// Get the ID pointer to the type of this variable.
				// TODO: This is typically through a pointer ID to the actual ID, but might not be? Should do checking.
				SpvID_t *resultTypeKind=&IDs[IDs[id->variable.resultTypeID].type.pointerTypeID];

				// Some decorations that we want that are associated with this variable.
				// These are in the decorations list for this ID, so extract them.
				uint32_t descriptorSet=0, binding=0;

				for(uint32_t j=0;j<id->decoration.decorationCount;j++)
				{
					switch(id->decoration.decorations[j].decoration)
					{
						case SpvDecorationDescriptorSet:
							descriptorSet=id->decoration.decorations[j].decorationValue;
							break;

						case SpvDecorationBinding:
							binding=id->decoration.decorations[j].decorationValue;
							break;
					}
				}

				// Print out a header for the ID
				DBGPRINTF(DEBUG_INFO, "ID: %d set: %d binding: %d %s %s\n", i, descriptorSet, binding, printSpvOp(resultTypeKind->opCode), printSpvStorageClass(id->variable.storageClass));

				// If it a struct type, print out the member info.
				if(resultTypeKind->opCode==SpvOpTypeStruct)
				{
					for(uint32_t j=0;j<resultTypeKind->type.structMemberCount;j++)
					{
						// Get the pointer to the member type ID from the struct member type IDs list.
						SpvID_t *memberType=&IDs[resultTypeKind->type.structMemberTypeIDs[j]];

						// Decoration associated with most members, search for them just like other decorations.
						uint32_t offset=0;

						for(uint32_t k=0;k<resultTypeKind->memberDecoration.member[j].decorationCount;k++)
						{
							switch(resultTypeKind->memberDecoration.member[j].decorations[k].decoration)
							{
								case SpvDecorationOffset:
									offset=resultTypeKind->memberDecoration.member[j].decorations[k].decorationValue;
									break;
							}
						}

						// Print out the information for the member.
						DBGPRINTF(DEBUG_INFO, "\tmember: %d type: %s offset: %d\n", j, printSpvOp(memberType->opCode), offset);
					}
				}
			}
		}
	}

	Zone_Free(zone, IDs);

	return true;
}
